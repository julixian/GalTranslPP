import multiprocessing as mp
import os
import queue
import subprocess
import sys
import traceback
from pathlib import Path


_REQUEST_TIMEOUT_SECONDS = 300.0
_CLOSE_TIMEOUT_SECONDS = 5.0

USE_GPU = False
GPU_ID = 0

HIDE_MULTIPROCESS_CONSOLE = True


def _python_exe() -> Path:
    python_path = Path(sys.prefix) / "python.exe"
    if not python_path.exists():
        python_path = Path(sys.executable).with_name("python.exe")
    return python_path


if HIDE_MULTIPROCESS_CONSOLE and sys.platform == "win32":
    pythonw_path = Path(sys.prefix) / "pythonw.exe"
    if not pythonw_path.exists():
        pythonw_path = Path(sys.executable).with_name("pythonw.exe")
    if pythonw_path.exists():
        mp.set_executable(str(pythonw_path))


def _close_queue(q) -> None:
    try:
        q.close()
    except Exception:
        pass
    try:
        q.join_thread()
    except Exception:
        pass


def _run_bool_worker(target, *args) -> bool:
    mp.freeze_support()
    ctx = mp.get_context("spawn")
    result_queue = ctx.Queue()
    process = ctx.Process(target=target, args=(*args, result_queue))
    process.start()
    try:
        try:
            result = bool(result_queue.get(timeout=_REQUEST_TIMEOUT_SECONDS))
        except queue.Empty:
            result = False
        process.join(_CLOSE_TIMEOUT_SECONDS)
        if process.is_alive():
            process.terminate()
            process.join(_CLOSE_TIMEOUT_SECONDS)
        return result
    finally:
        _close_queue(result_queue)


def _model_probe_worker(model_name: str, result_queue) -> None:
    try:
        import spacy

        if not spacy.util.is_package(model_name) and not Path(model_name).exists():
            result_queue.put(False)
            return
        spacy.load(model_name)
        result_queue.put(True)
    except BaseException:
        result_queue.put(False)


def ensure_model(model_name: str) -> bool:
    if _run_bool_worker(_model_probe_worker, model_name):
        return True

    subprocess.run([str(_python_exe()), "-m", "pip", "cache", "purge"])
    completed = subprocess.run([str(_python_exe()), "-m", "spacy", "download", model_name])
    if completed.returncode != 0:
        return False
    return _run_bool_worker(_model_probe_worker, model_name)


def _worker_loop(model_name: str, request_queue, response_queue) -> None:
    try:
        import spacy

        if USE_GPU:
            spacy.require_gpu(GPU_ID)

        nlp = spacy.load(model_name)
        response_queue.put({
            "type": "ready",
            "pid": os.getpid(),
            "pipeline": list(nlp.pipe_names),
        })
    except BaseException as e:
        response_queue.put({
            "type": "init_error",
            "errorType": type(e).__name__,
            "error": str(e),
            "traceback": traceback.format_exc(),
        })
        return

    while True:
        request = request_queue.get()
        request_id = request.get("id")
        command = request.get("command")
        if command == "close":
            response_queue.put({"type": "closed", "id": request_id})
            return
        if command != "process":
            response_queue.put({
                "type": "error",
                "id": request_id,
                "errorType": "ValueError",
                "error": f"Unknown command: {command}",
            })
            continue

        try:
            text = request.get("text", "")
            if not text:
                result = ([], [])
            else:
                doc = nlp(text)
                word_pos_list = [(token.text, token.pos_) for token in doc]
                entity_list = [(ent.text, ent.label_) for ent in doc.ents]
                result = (word_pos_list, entity_list)
            response_queue.put({
                "type": "result",
                "id": request_id,
                "result": result,
            })
        except BaseException as e:
            response_queue.put({
                "type": "error",
                "id": request_id,
                "errorType": type(e).__name__,
                "error": str(e),
                "traceback": traceback.format_exc(),
            })


class NLPProcessor:
    """
    spaCy 分词代理。父解释器只保存轻量代理对象，真正的 spaCy 模型在独立子进程中加载。
    """
    def __init__(self, model_name: str):
        self.model_name = model_name
        self._ctx = mp.get_context("spawn")
        self._request_queue = self._ctx.Queue()
        self._response_queue = self._ctx.Queue()
        self._next_id = 1
        self._closed = False
        self._process = self._ctx.Process(
            target=_worker_loop,
            args=(model_name, self._request_queue, self._response_queue),
        )
        self._process.start()
        message = self._response_queue.get(timeout=_REQUEST_TIMEOUT_SECONDS)
        message_type = message.get("type")
        if message_type == "ready":
            self.worker_pid = message.get("pid")
            self.pipeline = message.get("pipeline", [])
            return

        self.close()
        if message_type == "init_error":
            raise RuntimeError(
                f"spaCy worker failed to load model {model_name}: "
                f"{message.get('errorType')}: {message.get('error')}\n{message.get('traceback', '')}"
            )
        raise RuntimeError(f"spaCy worker returned unexpected init message: {message!r}")

    def process_text(self, text: str):
        if self._closed:
            raise RuntimeError("spaCy worker is already closed")

        request_id = self._next_id
        self._next_id += 1
        self._request_queue.put({
            "id": request_id,
            "command": "process",
            "text": text,
        })

        while True:
            try:
                message = self._response_queue.get(timeout=_REQUEST_TIMEOUT_SECONDS)
            except queue.Empty as e:
                raise TimeoutError("Timed out waiting for spaCy worker result") from e

            if message.get("id") != request_id:
                continue

            message_type = message.get("type")
            if message_type == "result":
                return message["result"]
            if message_type == "error":
                raise RuntimeError(
                    f"spaCy worker failed: {message.get('errorType')}: {message.get('error')}\n"
                    f"{message.get('traceback', '')}"
                )
            raise RuntimeError(f"spaCy worker returned unexpected message: {message!r}")

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        process = self._process
        if process is None:
            return

        try:
            if process.is_alive():
                self._request_queue.put({
                    "id": self._next_id,
                    "command": "close",
                })
                process.join(_CLOSE_TIMEOUT_SECONDS)
            if process.is_alive():
                process.terminate()
                process.join(_CLOSE_TIMEOUT_SECONDS)
        finally:
            self._process = None
            _close_queue(self._request_queue)
            _close_queue(self._response_queue)

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
