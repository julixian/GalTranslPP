from __future__ import annotations

from enum import Enum
from pathlib import Path
from typing import Any, Callable, Sequence

PathLike = str | Path
WordPosVec = Sequence[Sequence[str]]
EntityVec = Sequence[Sequence[str]]
NLPResult = tuple[list[list[str]], list[list[str]]]
DictList = list[tuple[str, str, str]]


class NameType(Enum):
    # The C++ enum also exposes a member named "None", but that name cannot be
    # written as normal Python syntax. Use getattr(NameType, "None") if needed.
    Single = 1
    Multiple = 2


class TransEngine(Enum):
    # The C++ enum also exposes a member named "None"; see NameType above.
    ForGalJson = 1
    ForGalTsv = 2
    ForNovelTsv = 3
    DeepseekJson = 4
    Sakura = 5
    DumpName = 6
    NameTrans = 7
    GenDict = 8
    Rebuild = 9
    ShowNormal = 10


class LogLevel(Enum):
    trace = 0
    debug = 1
    info = 2
    warn = 3
    err = 4
    critical = 5


Single: NameType
Multiple: NameType

ForGalJson: TransEngine
ForGalTsv: TransEngine
ForNovelTsv: TransEngine
DeepseekJson: TransEngine
Sakura: TransEngine
DumpName: TransEngine
NameTrans: TransEngine
GenDict: TransEngine
Rebuild: TransEngine
ShowNormal: TransEngine

trace: LogLevel
debug: LogLevel
info: LogLevel
warn: LogLevel
err: LogLevel
critical: LogLevel


class SentencePosition:
    file: str
    index: int

    def __init__(self) -> None: ...


class Sentence:
    index: int
    fileName: str
    name: str
    names: list[str]
    name_preview: str
    names_preview: list[str]
    original_text: str
    pre_processed_text: str
    pre_translated_text: str
    problems: list[str]
    translated_by: str
    translated_preview: str
    originalLinebreak: str
    other_info: dict[str, str]
    repeatedBlockRefTo: SentencePosition | None
    repeatedBlockRefBy: list[SentencePosition]
    nameType: NameType
    prev: Sentence | None
    next: Sentence | None
    complete: bool
    notAnalyzeProblem: bool
    repeatedBlockRefPending: bool

    def __init__(self) -> None: ...
    def problems_get_by_index(self, index: int) -> str | None: ...
    def problems_set_by_index(self, index: int, problem: str) -> bool: ...


class spdlogLogger:
    def name(self) -> str: ...
    def level(self) -> LogLevel: ...
    def set_level(self, level: LogLevel) -> None: ...
    def set_pattern(self, pattern: str) -> None: ...
    def trace(self, msg: str) -> None: ...
    def debug(self, msg: str) -> None: ...
    def info(self, msg: str) -> None: ...
    def warn(self, msg: str) -> None: ...
    def error(self, msg: str) -> None: ...
    def critical(self, msg: str) -> None: ...


class _UtilsModule:
    def executeCommand(
        self,
        program: str,
        args: str,
        showWindow: bool,
        timeDelayAfterCommand: int,
    ) -> bool: ...
    def getConsoleWidth(self) -> int: ...
    def removePunctuation(self, sourceString: str) -> str: ...
    def removeWhitespace(self, sourceString: str) -> str: ...
    def getMostCommonChar(self, s: str) -> tuple[str, int]: ...
    def splitIntoTokens(self, wordPosVec: WordPosVec, text: str) -> list[str]: ...
    def splitIntoGraphemes(self, sourceString: str) -> list[str]: ...
    def extractKatakana(self, sourceString: str) -> str: ...
    def extractKana(self, sourceString: str) -> str: ...
    def extractLatin(self, sourceString: str) -> str: ...
    def extractHangul(self, sourceString: str) -> str: ...
    def extractCJK(self, sourceString: str) -> str: ...
    def getTraditionalChineseExtractor(
        self,
        logger: spdlogLogger,
    ) -> Callable[[str], str]: ...


utils: _UtilsModule


class RuntimeSuccessEvent:
    timestamp: str
    filename: str
    index: int
    speakers: list[str]
    sourcePreview: str
    translationPreview: str
    translatedBy: str

    def __init__(self) -> None: ...


class RuntimeErrorEvent:
    timestamp: str
    kind: str
    level: str
    message: str
    filename: str
    indexRange: str
    retryCount: int
    model: str
    sleepSeconds: float

    def __init__(self) -> None: ...


class RuntimeFileProgress:
    filename: str
    total: int
    completed: int
    problems: int

    def __init__(self) -> None: ...


class IController:
    m_totalSentences: int
    m_completedSentences: int
    m_workersActive: int
    m_workersConfigured: int

    def makeBar(self, totalSentences: int, totalThreads: int) -> None: ...
    def writeLog(self, log: str) -> None: ...
    def addThreadNum(self) -> None: ...
    def reduceThreadNum(self) -> None: ...
    def updateBar(self, ticks: int = 1) -> None: ...
    def setRuntimeFiles(self, fileTotals: dict[str, int]) -> None: ...
    def setRuntimeStage(self, stage: str, currentFile: str = "") -> None: ...
    def recordFileSentenceDone(self, runtimeFile: str, hasProblem: bool) -> None: ...
    def recordRuntimeSuccess(self, event: RuntimeSuccessEvent) -> None: ...
    def recordRuntimeError(self, event: RuntimeErrorEvent) -> None: ...
    def shouldStop(self) -> bool: ...
    def flush(self) -> None: ...


class ITranslator:
    def run(self) -> None: ...


class ThreadPool:
    def resize(self, n: int) -> None: ...
    def size(self) -> int: ...


class NormalJsonTranslator(ITranslator):
    m_transEngine: TransEngine
    m_controller: IController
    m_logger: spdlogLogger
    m_inputDir: Path
    m_inputCacheDir: Path
    m_outputDir: Path
    m_outputCacheDir: Path
    m_transCacheDir: Path
    m_otherCacheDir: Path
    m_backgroundTextCachePath: Path
    m_projectDir: Path
    m_agentRootDir: Path
    m_agentTermLedgerPath: Path
    m_agentFileNotesDir: Path
    m_agentSearchCatalogPath: Path
    m_backgroundTextCacheMap: dict[str, str]
    m_systemPrompt: str
    m_userPrompt: str
    m_agentSystemPrompt: str
    m_agentUserPrompt: str
    m_genDictReviewSystemPrompt: str
    m_genDictReviewUserPrompt: str
    m_targetLang: str
    m_pythonTranslator: bool
    m_threadsNum: int
    m_nameTransBatchSize: int
    m_batchSize: int
    m_contextHistorySize: int
    m_maxRetries: int
    m_saveCacheInterval: int
    m_apiTimeOutMs: int
    m_checkQuota: bool
    m_smartRetry: bool
    m_retransAllWhenFail: bool
    m_usePreDictInName: bool
    m_usePostDictInName: bool
    m_usePreDictInMsg: bool
    m_usePostDictInMsg: bool
    m_useGptDictToReplaceName: bool
    m_outputWithSrc: bool
    m_agentEnabled: bool
    m_reuseRepeatedBlocks: bool
    m_useRepeatedBlockInputCache: bool
    m_apiStrategy: str
    m_sortMethod: str
    m_splitFile: str
    m_splitFileNum: int
    m_repeatedBlockMinSize: int
    m_cacheSearchDistance: int
    m_linebreakSymbol: str
    m_agentMaxTurnsPerChunk: int
    m_agentSoftContextChars: int
    m_agentHardContextChars: int
    m_agentSearchResultLimit: int
    m_needsCombining: bool
    m_splitFilePartsToJson: dict[Path, Path]
    m_jsonToSplitFileParts: dict[Path, dict[Path, bool]]
    m_agentKnownRelFiles: list[Path]
    m_agentDictionaryPaths: list[Path]
    m_agentProjectInfoPath: Path | None
    m_nameMap: dict[str, str]
    m_currentRunRelFilePaths: list[Path] | None
    m_onFileProcessed: Callable[[Path], None] | None
    m_onPerformApi: Callable[[str], str] | None
    m_onDictProcessed: Callable[[DictList], DictList] | None
    m_threadPool: ThreadPool

    def preProcess(self, se: Sentence) -> None: ...
    def postProcess(self, se: Sentence) -> None: ...
    def processFile(self, relInputPath: PathLike, threadId: int) -> None: ...
    def shouldReportRuntimeWorkbench(self) -> bool: ...
    def applyAgentRetranslateSuggestions(self) -> None: ...
    def resolveRepeatedBlockReferences(self) -> None: ...
    def normalJsonInit(self) -> None: ...
    def normalJsonBeforeRun(self) -> None: ...
    def normalJsonProcessFiles(self, relFilePaths: Sequence[PathLike]) -> None: ...
    def normalJsonProcess(self) -> None: ...
    def normalJsonAfterRun(self) -> None: ...
    def normalJsonRun(self) -> None: ...


class EpubTextNodeInfo:
    offset: int
    length: int

    def __init__(self) -> None: ...


class JsonInfo:
    metadata: list[EpubTextNodeInfo]
    htmlPath: Path
    epubPath: Path
    normalPostPath: Path
    content: str

    def __init__(self) -> None: ...


class EpubTranslator(NormalJsonTranslator):
    m_epubInputDir: Path
    m_epubOutputDir: Path
    m_tempUnpackDir: Path
    m_tempRebuildDir: Path
    m_bilingualOutput: bool
    m_originalTextColor: str
    m_originalTextScale: str
    m_jsonToInfoMap: dict[Path, JsonInfo]
    m_epubToJsonsMap: dict[Path, dict[Path, bool]]

    def epubInit(self) -> None: ...
    def epubBeforeRun(self) -> None: ...
    def epubRun(self) -> None: ...


class PDFTranslator(NormalJsonTranslator):
    m_pdfInputDir: Path
    m_pdfOutputDir: Path
    m_bilingualOutput: bool
    m_jsonToPDFPathMap: dict[Path, Path]

    def pdfInit(self) -> None: ...
    def pdfBeforeRun(self) -> None: ...
    def pdfRun(self) -> None: ...
