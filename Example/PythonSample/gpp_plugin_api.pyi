from __future__ import annotations

from enum import Enum
from pathlib import Path
from typing import Any, Callable, Sequence

PathLike = str | Path
NLPPair = tuple[str, str]
WordPosVec = Sequence[NLPPair]
EntityVec = Sequence[NLPPair]
NLPResult = tuple[list[NLPPair], list[NLPPair]]
TokenizeCache = dict[str, list[NLPPair]]
DictList = list[tuple[str, str, str]]


StringStringMap = dict[str, str]
PathPathMap = dict[Path, Path]
PathBoolMap = dict[Path, bool]
PathPathBoolMap = dict[Path, PathBoolMap]
PathJsonInfoMap = dict[Path, "JsonInfo"]
PathJsonMap = dict[Path, Any]


class NameType(Enum):
    # The C++ enum also exposes a member named "None", but that name cannot be
    # written as normal Python syntax. Use getattr(NameType, "None") if needed.
    Single = 1
    Multiple = 2


class TransEngine(Enum):
    # The C++ enum also exposes a member named "None"; see NameType above.
    ForGalTsv = 1
    ForNovelTsv = 2
    ForGalJson = 3
    Sakura = 4
    DumpName = 5
    NameTrans = 6
    GenDict = 7
    Rebuild = 8
    ShowNormal = 9


class CachePart(Enum):
    # The C++ enum also exposes a member named "None"; see NameType above.
    Index = 1
    FileName = 2
    Name = 3
    Names = 4
    NameTrans = 5
    NamesTrans = 6
    Orig = 7
    Preproc = 8
    Problems = 9
    OtherInfo = 10
    TransBy = 11
    TransRaw = 12
    Transview = 13


class ApiProtocol(Enum):
    OpenAI = 0
    Claude = 1
    Gemini = 2


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

OpenAI: ApiProtocol
Claude: ApiProtocol
Gemini: ApiProtocol


class SentencePosition:
    file: str
    index: int

    def __init__(self) -> None: ...


class Sentence:
    index: int
    filename: str
    name: str
    names: list[str]
    nametrans: str
    namestrans: list[str]
    orig: str
    preproc: str
    problems: list[str]
    transby: str
    transraw: str
    transview: str
    linebreak: str
    otherinfo: dict[str, str]
    ref: SentencePosition | None
    refBy: list[SentencePosition]
    nameType: NameType
    prev: Sentence | None
    next: Sentence | None
    transCompleted: bool
    problemAnalyzeDisabled: bool
    isRefPending: bool

    def __init__(self) -> None: ...
    def getProblemByIndex(self, index: int) -> str | None: ...
    def setProblemByIndex(self, index: int, problem: str) -> bool: ...


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
    def splitIntoTokens(self, wordPosVec: WordPosVec, text: str) -> list[str]: ...
    def splitIntoGraphemes(self, sourceString: str) -> list[str]: ...
    def countGraphemes(self, sourceString: str) -> int: ...
    def getMostCommonChar(self, sourceString: str) -> tuple[str, int]: ...
    def hasPunctuation(self, sourceString: str) -> bool: ...
    def hasWhitespace(self, sourceString: str) -> bool: ...
    def isAllPunctuation(self, sourceString: str) -> bool: ...
    def isAllWhitespace(self, sourceString: str) -> bool: ...
    def removePunctuation(self, sourceString: str) -> str: ...
    def removeWhitespace(self, sourceString: str) -> str: ...
    def hasKatakana(self, sourceString: str) -> bool: ...
    def hasKana(self, sourceString: str) -> bool: ...
    def hasLatin(self, sourceString: str) -> bool: ...
    def hasHangul(self, sourceString: str) -> bool: ...
    def hasCJK(self, sourceString: str) -> bool: ...
    def extractKatakana(self, sourceString: str) -> str: ...
    def extractKana(self, sourceString: str) -> str: ...
    def extractLatin(self, sourceString: str) -> str: ...
    def extractHangul(self, sourceString: str) -> str: ...
    def extractCJK(self, sourceString: str) -> str: ...
    def getTraditionalChineseExtractor(self) -> Callable[[str], str]: ...
    def getConsoleWidth(self) -> int: ...
    def loadTokenizeCache(self, cachePath: PathLike, logger: spdlogLogger) -> TokenizeCache: ...
    def saveTokenizeCache(self, cache: TokenizeCache, cachePath: PathLike, logger: spdlogLogger) -> None: ...


utils: _UtilsModule


class RuntimeTransSuccessEvent:
    timestamp: str
    filename: str
    index: int
    speakers: list[str]
    problems: list[str]
    sourcePreview: str
    translationPreview: str
    transby: str

    def __init__(self) -> None: ...


class RuntimeTransErrorEvent:
    timestamp: str
    kind: str
    level: str
    message: str
    filename: str
    indexRange: str
    requestCount: int
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
    m_activeThreads: int
    m_totalThreads: int

    def makeBar(self, totalSentences: int, totalThreads: int) -> None: ...
    def writeLog(self, log: str) -> None: ...
    def addThreadNum(self) -> None: ...
    def reduceThreadNum(self) -> None: ...
    def updateBar(self, ticks: int = 1) -> None: ...
    def setRuntimeFiles(self, fileTotals: dict[str, int]) -> None: ...
    def setRuntimeStage(self, stage: str, currentFile: str = "") -> None: ...
    def recordFileSentenceDone(self, runtimeFile: str, hasProblem: bool) -> None: ...
    def recordRuntimeTransSuccess(self, event: RuntimeTransSuccessEvent) -> None: ...
    def recordRuntimeTransError(self, event: RuntimeTransErrorEvent) -> None: ...
    def shouldStop(self) -> bool: ...
    def flush(self) -> None: ...


class ITranslator:
    def run(self) -> None: ...


class ThreadPool:
    def resize(self, n: int) -> None: ...
    def size(self) -> int: ...


class ApiPool:
    def resortTokens(self) -> None: ...
    def isEmpty(self) -> bool: ...
    def size(self) -> int: ...


class GptDictionary:
    def sort(self) -> None: ...
    def loadFromFile(self, filePath: PathLike) -> None: ...


class NormalDictionary:
    def sort(self) -> None: ...
    def loadFromFile(self, filePath: PathLike) -> None: ...


class ProblemCompareObj:
    use: bool
    base: CachePart
    check: CachePart

    def __init__(self) -> None: ...


class Problems:
    highFrequency: ProblemCompareObj
    punctsMiss: ProblemCompareObj
    remainJp: ProblemCompareObj
    introLatin: ProblemCompareObj
    introHangul: ProblemCompareObj
    introTraditionalChinese: ProblemCompareObj
    linebreakLost: ProblemCompareObj
    linebreakAdded: ProblemCompareObj
    longer: ProblemCompareObj
    strictlyLonger: ProblemCompareObj
    dictUnused: ProblemCompareObj
    notTargetLang: ProblemCompareObj
    invalidChar: ProblemCompareObj

    def __init__(self) -> None: ...


class ProblemAnalyzer:
    def setProblemRule(self, problemKey: str, enabled: bool, base: str, check: str) -> None: ...
    def analyze(self, sentence: Sentence) -> None: ...


class NameTranslator:
    def run(self, nameTablePath: PathLike) -> None: ...


class DictionaryGenerator:
    def generate(self, outputFilePath: PathLike) -> None: ...


class NormalJsonTranslatorTransAgent:
    def applyAgentSuggestions(self) -> None: ...


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
    m_nameTablePath: Path
    m_rollingContextCachePath: Path
    m_projectDir: Path
    m_agentRootDir: Path
    m_agentTermLedgerPath: Path
    m_agentFileNotesDir: Path
    m_rollingContextCacheMap: StringStringMap
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
    m_inputBlockMaxLines: int
    m_problemMaxLines: int
    m_glossaryMaxLines: int
    m_maxRequestCount: int
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
    m_apiStrategy: str
    m_sortMethod: str
    m_splitFileMethod: str
    m_problemOverviewFormat: str
    m_splitFileNum: int
    m_repeatedBlockMinSize: int
    m_cacheSearchDistance: int
    m_linebreakSymbol: str
    m_agentMaxTurnsPerChunk: int
    m_agentCompactContextThresholdBytes: int
    m_agentSearchResultLimit: int
    m_agentContextLinesLimit: int
    m_splitFileEnabled: bool
    m_splitFilePartsToJson: PathPathMap
    m_jsonToSplitFileParts: PathPathBoolMap
    m_gptDictionaryPaths: list[Path]
    m_agentProjectNotePath: Path | None
    m_nameMap: StringStringMap
    m_currentRunRelFilePaths: list[Path] | None
    m_inputJsonMap: PathJsonMap
    m_savedTranslCacheMap: PathJsonMap
    m_repeatedBlockCompletedRelFilePaths: set[Path]
    m_repeatedBlockReferenceCount: int
    m_onFileProcessed: Callable[[Path], None] | None
    m_onPerformApi: Callable[[str], str] | None
    m_onDictProcessed: Callable[[DictList], DictList] | None
    m_threadPool: ThreadPool
    m_apiPool: ApiPool | None
    m_gptDictionary: GptDictionary | None
    m_preDictionary: NormalDictionary | None
    m_postDictionary: NormalDictionary | None
    m_problemAnalyzer: ProblemAnalyzer | None
    m_nameTranslator: NameTranslator | None
    m_dictionaryGenerator: DictionaryGenerator | None
    m_transAgent: NormalJsonTranslatorTransAgent | None

    def preProcess(self, se: Sentence) -> None: ...
    def postProcess(self, se: Sentence) -> None: ...
    def processFile(self, relInputPath: PathLike, threadId: int) -> None: ...
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
    m_jsonToInfoMap: PathJsonInfoMap
    m_epubToJsonsMap: PathPathBoolMap

    def epubInit(self) -> None: ...
    def epubBeforeRun(self) -> None: ...
    def epubRun(self) -> None: ...


class PDFTranslator(NormalJsonTranslator):
    m_pdfInputDir: Path
    m_pdfOutputDir: Path
    m_bilingualOutput: bool
    m_babeldocLangOut: str
    m_jsonToPDFPathMap: PathPathMap

    def pdfInit(self) -> None: ...
    def pdfBeforeRun(self) -> None: ...
    def pdfRun(self) -> None: ...
