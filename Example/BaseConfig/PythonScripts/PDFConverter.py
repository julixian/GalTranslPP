import json
import logging
import multiprocessing
import os
import queue
import sys
import tempfile
import traceback
from contextlib import contextmanager
from pathlib import Path
from threading import Lock


def _configure_multiprocessing() -> None:
    if sys.platform != "win32":
        return

    python_path = Path(sys.prefix) / "python.exe"
    if not python_path.exists():
        python_path = Path(sys.executable).with_name("python.exe")
    if python_path.exists():
        multiprocessing.set_executable(str(python_path))


_configure_multiprocessing()


def _attach_console_output() -> None:
    if sys.platform != "win32":
        return
    try:
        stdout = open("CONOUT$", "w", encoding="utf-8", buffering=1)
        sys.stdout = stdout
        sys.stderr = stdout
    except OSError:
        pass


class FinishReading(Exception):
    """BabelDOC 翻译阶段完成文本收集后，用这个异常提前结束后续流程。"""

    @classmethod
    def raise_after_call(cls, func):
        def wrapper(*args, **kwargs):
            func(*args, **kwargs)
            raise cls

        return wrapper


class _BasicTranslator:
    def do_llm_translate(self, text, rate_limit_params: dict | None = None):
        raise NotImplementedError

    def get_rich_text_left_placeholder(self, placeholder_id: int | str) -> str:
        return f"<b{placeholder_id}>"

    def get_rich_text_right_placeholder(self, placeholder_id: int | str) -> str:
        return f"</b{placeholder_id}>"

    def get_formular_placeholder(self, placeholder_id: int | str) -> str:
        return self.get_rich_text_left_placeholder(placeholder_id)


class PdfTextCollector(_BasicTranslator):
    """伪装成翻译器，只收集 BabelDOC 交给翻译阶段的原文。"""

    def __init__(self):
        self.source_texts: list[str] = []

    def translate(self, text, *args, **kwargs):
        self.source_texts.append(text)
        return text

    def do_translate(self, text, rate_limit_params: dict | None = None):
        return self.translate(text)


class SequentialTranslator(_BasicTranslator):
    """按 BabelDOC 请求顺序返回已经翻译好的文本。"""

    def __init__(self, translated_texts: list[str]):
        self.translated_texts = translated_texts
        self.index = 0
        self.lock = Lock()

    def translate(self, text, *args, **kwargs):
        with self.lock:
            if self.index < len(self.translated_texts):
                result = self.translated_texts[self.index]
                self.index += 1
                return result
            logging.warning("Translation text exhausted. Keeping original: %r", text)
            return text

    def do_translate(self, text, rate_limit_params: dict | None = None):
        return self.translate(text)


class IgnoreFinishReadingExceptionFilter(logging.Filter):
    def filter(self, record):
        return not (record.exc_info and issubclass(record.exc_info[0], FinishReading))


def _create_progress_stage(progress_monitor):
    from rich.console import Console
    from rich.progress import (
        BarColumn,
        MofNCompleteColumn,
        Progress,
        TextColumn,
        TimeElapsedColumn,
        TimeRemainingColumn,
    )

    class CustomTranslationStage(progress_monitor.TranslationStage):
        def __enter__(self):
            if self.pm.pbar_manager:
                self.pbar = self.pm.pbar_manager.add_task(self.name, total=self.total)
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            if hasattr(self, "pbar"):
                with self.lock:
                    remaining = self.total - self.current
                    if remaining > 0:
                        self.pm.pbar_manager.update(self.pbar, advance=remaining)

        def advance(self, n: int = 1):
            if hasattr(self, "pbar"):
                with self.lock:
                    self.current += n
                    self.pm.pbar_manager.update(self.pbar, advance=n)

        @staticmethod
        def create_progress_bar():
            return Progress(
                TextColumn("[progress.description]{task.description:<30}"),
                BarColumn(),
                MofNCompleteColumn(),
                TextColumn("•"),
                TimeElapsedColumn(),
                TextColumn("•"),
                TimeRemainingColumn(),
                console=Console(file=sys.stdout, force_terminal=True),
            )

    return CustomTranslationStage


def _load_babeldoc():
    from babeldoc.docvision.doclayout import DocLayoutModel
    from babeldoc.format.pdf.document_il.midend import il_translator
    from babeldoc.format.pdf.high_level import TRANSLATE_STAGES, do_translate
    from babeldoc.format.pdf.translation_config import TranslationConfig, WatermarkOutputMode
    from babeldoc.main import create_parser
    import babeldoc.progress_monitor as progress_monitor

    logging.getLogger("babeldoc.format.pdf.high_level").addFilter(
        IgnoreFinishReadingExceptionFilter()
    )
    return {
        "create_parser": create_parser,
        "do_translate": do_translate,
        "translate_stages": TRANSLATE_STAGES,
        "translation_config": TranslationConfig,
        "watermark_output_mode": WatermarkOutputMode,
        "il_translator": il_translator,
        "doc_layout_model": DocLayoutModel,
        "progress_monitor": progress_monitor,
        "progress_stage": _create_progress_stage(progress_monitor),
    }


@contextmanager
def _patched_progress_stage(runtime):
    progress_monitor = runtime["progress_monitor"]
    progress_stage = runtime["progress_stage"]
    original_stage = progress_monitor.TranslationStage
    progress_monitor.TranslationStage = progress_stage
    try:
        yield progress_stage
    finally:
        progress_monitor.TranslationStage = original_stage


def _run_babeldoc(runtime, config, show_progress: bool) -> object:
    progress_monitor = runtime["progress_monitor"]
    progress_stage = runtime["progress_stage"]
    progress_bar = progress_stage.create_progress_bar() if show_progress else None

    with progress_monitor.ProgressMonitor(runtime["translate_stages"]) as monitor:
        monitor.pbar_manager = progress_bar
        try:
            if progress_bar:
                progress_bar.start()
            return runtime["do_translate"](monitor, config)
        finally:
            if progress_bar:
                progress_bar.stop()


def _extract_texts_from_pdf(pdf_path: Path, babeldoc_lang_out: str, show_progress: bool) -> list[str]:
    runtime = _load_babeldoc()
    text_collector = PdfTextCollector()

    with tempfile.TemporaryDirectory() as temp_dir:
        parser = runtime["create_parser"]()
        args = parser.parse_args(
            [
                "--files",
                str(pdf_path),
                "--output",
                temp_dir,
                "--no-mono",
                "--no-dual",
                "--ignore-cache",
                "--lang-out",
                babeldoc_lang_out,
            ]
        )
        config = runtime["translation_config"](
            input_file=args.files[0],
            output_dir=args.output,
            translator=text_collector,
            doc_layout_model=runtime["doc_layout_model"].load_onnx(),
            watermark_output_mode=runtime["watermark_output_mode"].NoWatermark,
            skip_scanned_detection=True,
            lang_in=args.lang_in,
            lang_out=args.lang_out,
        )

        il_translator = runtime["il_translator"]
        original_translate = il_translator.ILTranslator.translate
        try:
            il_translator.ILTranslator.translate = FinishReading.raise_after_call(
                original_translate
            )
            with _patched_progress_stage(runtime):
                try:
                    _run_babeldoc(runtime, config, show_progress)
                except FinishReading:
                    pass
        finally:
            il_translator.ILTranslator.translate = original_translate

    return text_collector.source_texts


def _generate_pdf_from_texts(
    original_pdf: Path,
    translated_texts: list[str],
    output_dir: Path,
    babeldoc_lang_out: str,
    bilingual_output: bool,
    show_progress: bool,
) -> tuple[str | None, str | None]:
    runtime = _load_babeldoc()
    parser = runtime["create_parser"]()
    cmd_args = [
        "--files",
        str(original_pdf),
        "--output",
        str(output_dir),
        "--ignore-cache",
        "--skip-scanned-detection",
        "--lang-out",
        babeldoc_lang_out,
    ]

    args = parser.parse_args(cmd_args)
    config = runtime["translation_config"](
        input_file=args.files[0],
        output_dir=args.output,
        translator=SequentialTranslator(translated_texts),
        doc_layout_model=runtime["doc_layout_model"].load_onnx(),
        no_dual=not bilingual_output,
        no_mono=False,
        watermark_output_mode=runtime["watermark_output_mode"].NoWatermark,
        lang_in=args.lang_in,
        lang_out=args.lang_out,
    )

    with _patched_progress_stage(runtime):
        result = _run_babeldoc(runtime, config, show_progress)

    mono_path = str(Path(result.mono_pdf_path).resolve()) if result.mono_pdf_path else None
    dual_path = str(Path(result.dual_pdf_path).resolve()) if result.dual_pdf_path else None
    return mono_path, dual_path


def _extract_text_to_json(
    input_pdf_path: str,
    output_json_path: str,
    babeldoc_lang_out: str = "zh-CN",
    show_progress: bool = False,
) -> tuple[bool, str]:
    try:
        pdf_path = Path(input_pdf_path)
        json_path = Path(output_json_path)
        if not pdf_path.exists():
            return False, f"Error: Input PDF not found at '{input_pdf_path}'"

        if show_progress:
            print(f"[*] Extracting original texts from `{pdf_path.name}`...")
        extracted_texts = _extract_texts_from_pdf(pdf_path, babeldoc_lang_out, show_progress)
        if not extracted_texts:
            return True, "Warning: No text was extracted. JSON file will not be created."

        json_data = [{"message": text} for text in extracted_texts]
        if show_progress:
            print(f"[*] Writing {len(json_data)} text items to `{json_path.name}`...")
        json_path.write_text(
            json.dumps(json_data, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        return True, f"Success: Extracted {len(json_data)} items to '{json_path.resolve()}'"
    except Exception as e:
        logging.exception("An error occurred during text extraction:")
        return False, f"Fatal Error during extraction: {e}"


def _reinject_json_to_pdf(
    original_pdf_path: str,
    translation_json_path: str,
    output_dir_path: str,
    babeldoc_lang_out: str = "zh-CN",
    bilingual_output: bool = True,
    show_progress: bool = False,
) -> tuple[bool, str]:
    try:
        pdf_path = Path(original_pdf_path)
        json_path = Path(translation_json_path)
        output_dir = Path(output_dir_path)

        if not pdf_path.exists():
            return False, f"Error: Original PDF not found at '{original_pdf_path}'"
        if not json_path.exists():
            return False, f"Error: Translation JSON not found at '{translation_json_path}'"
        output_dir.mkdir(parents=True, exist_ok=True)

        if show_progress:
            print("[*] Step 1/3: Extracting original texts for validation...")
        original_texts = _extract_texts_from_pdf(pdf_path, babeldoc_lang_out, show_progress)
        if not original_texts:
            return False, "Error: Could not extract any text from the original PDF for validation."

        if show_progress:
            print("[*] Step 2/3: Loading and validating translation file...")
        try:
            translated_texts = [
                item["message"]
                for item in json.loads(json_path.read_text(encoding="utf-8"))
            ]
        except (OSError, json.JSONDecodeError, KeyError) as e:
            return False, f"Error: Failed to read or parse JSON file. Details: {e}"

        if len(original_texts) != len(translated_texts):
            return False, (
                "Fatal Error: Mismatch in text counts. "
                f"Original: {len(original_texts)}, Translated: {len(translated_texts)}"
            )

        if show_progress:
            print(f"[+] Validation successful. Count: {len(translated_texts)}.")
            print("[*] Step 3/3: Generating translated PDF(s)...")
        mono_path, dual_path = _generate_pdf_from_texts(
            pdf_path,
            translated_texts,
            output_dir,
            babeldoc_lang_out,
            bilingual_output,
            show_progress,
        )

        if mono_path:
            mono_target_path = output_dir / pdf_path.name
            Path(mono_path).replace(mono_target_path)
            mono_path = str(mono_target_path.resolve())
        if dual_path:
            dual_target_path = output_dir / f"{pdf_path.stem}.dual.pdf"
            Path(dual_path).replace(dual_target_path)
            dual_path = str(dual_target_path.resolve())

        paths = [path for path in (mono_path, dual_path) if path]
        if not paths:
            return True, "Task completed, but no PDF was generated."

        report = ["Success! Generated files:"]
        if mono_path:
            report.append(f"Mono PDF: {mono_path}")
        if dual_path:
            report.append(f"Dual PDF: {dual_path}")
        return True, "\n".join(report)
    except Exception as e:
        logging.exception("An error occurred during PDF generation:")
        return False, f"Fatal Error during PDF generation: {e}"


def _pdf_worker_main(task_name: str, args: tuple, response_queue) -> None:
    try:
        show_progress = bool(args[-1])
        if show_progress:
            _attach_console_output()
            print(f"[GPP PDF] Worker PID: {os.getpid()}", flush=True)
        if task_name == "extract":
            result = _extract_text_to_json(*args)
        elif task_name == "reinject":
            result = _reinject_json_to_pdf(*args)
        else:
            raise ValueError(f"Unknown PDF task: {task_name}")
        response_queue.put({"ok": True, "result": result})
    except BaseException as e:
        response_queue.put(
            {
                "ok": False,
                "error": f"{type(e).__name__}: {e}",
                "traceback": traceback.format_exc(),
            }
        )


def _run_pdf_task(task_name: str, args: tuple) -> tuple[bool, str]:
    multiprocessing.freeze_support()
    ctx = multiprocessing.get_context("spawn")
    response_queue = ctx.Queue()
    process = ctx.Process(target=_pdf_worker_main, args=(task_name, args, response_queue))
    process.start()
    process.join()

    try:
        message = response_queue.get_nowait()
    except queue.Empty:
        return False, f"PDF worker exited without result. exitcode={process.exitcode}"
    finally:
        response_queue.close()
        response_queue.join_thread()

    if message.get("ok"):
        return message["result"]
    return False, f"PDF worker failed: {message.get('error')}\n{message.get('traceback', '')}"


def extract_text_to_json(
    input_pdf_path: str,
    output_json_path: str,
    babeldoc_lang_out: str = "zh-CN",
    show_progress: bool = False,
) -> tuple[bool, str]:
    return _run_pdf_task(
        "extract",
        (input_pdf_path, output_json_path, babeldoc_lang_out, show_progress),
    )


def reinject_json_to_pdf(
    original_pdf_path: str,
    translation_json_path: str,
    output_dir_path: str,
    babeldoc_lang_out: str = "zh-CN",
    bilingual_output: bool = True,
    show_progress: bool = False,
) -> tuple[bool, str]:
    return _run_pdf_task(
        "reinject",
        (
            original_pdf_path,
            translation_json_path,
            output_dir_path,
            babeldoc_lang_out,
            bilingual_output,
            show_progress,
        ),
    )
