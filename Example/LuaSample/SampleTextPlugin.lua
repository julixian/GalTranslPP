-- Text 插件可以选择让 C++ 注入分词器函数。可配置 sourceLang / targetLang 两套。
-- 可选后端: "MeCab" / "spaCy" / "Stanza" / "pkuseg"。
-- MeCab: 需要 sourceLangMecabDictDir 或 targetLangMecabDictDir。
-- spaCy: 需要 sourceLangSpaCyModelName 或 targetLangSpaCyModelName，例如 "ja_core_news_trf" / "zh_core_web_trf"。
-- Stanza: 需要 sourceLangStanzaLang 或 targetLangStanzaLang，例如 "ja" / "zh"。
-- pkuseg: 不需要额外模型名。

targetLangUseTokenizer = true
targetLangTokenizerBackend = "spaCy"
targetLangSpaCyModelName = "zh_core_web_trf"

local sampleOrig = "\n  「――――きて」\n "

local function appendProblem(sentence, problem)
    -- C++ 容器会作为 table 副本交给 Lua。
    -- 需要修改时，先改副本，再整体赋回 C++ 字段。
    local problems = sentence.problems
    table.insert(problems, problem)
    sentence.problems = problems
end

local function setOtherinfo(sentence, key, value)
    local otherinfo = sentence.otherinfo
    otherinfo[key] = value
    sentence.otherinfo = otherinfo
end

function init(projectDir)
    utils.logger:info("SampleTextPlugin 初始化，projectDir: " .. tostring(projectDir))
end

function checkConditionForRetranslKeysFunc(sentence)
    -- retranslKey 条件函数只负责判断是否重翻，返回 true 表示命中。
    if sentence.index == 2 and sentence.orig == sampleOrig then
        utils.logger:info("Lua retranslKey 示例命中检查")
    end
    return false
end

function checkConditionForSkipProblemsFunc(sentence, problem)
    -- skipProblems 条件函数拿到的是即将输出的 Sentence 和当前命中的问题文本。
    -- 返回 true 表示跳过这个 problem，返回 false 表示保留。
    if sentence.index ~= 2 or sentence.orig ~= sampleOrig then
        return false
    end

    setOtherinfo(sentence, "luaSkippedProblem", problem)
    utils.logger:info("Lua skipProblems 示例命中检查: " .. problem)
    return problem == "测试问题1"
end

function dPostRun(sentence)
    -- dPostRun 在后处理后执行。这里演示调用 tokenizer 并把结果写入 otherinfo。
    if sentence.orig ~= sampleOrig then
        return
    end
    if utils.targetLangTokenizeFunc == nil then
        return
    end

    local wordPosVec = utils.targetLangTokenizeFunc("测试目标语言分词器")
    local tokens = utils.splitIntoTokens(wordPosVec, "测试目标语言分词器")
    setOtherinfo(sentence, "tokensLua", table.concat(tokens, "|"))

    appendProblem(sentence, "测试问题1")
    appendProblem(sentence, "测试问题2")
    sentence.transview = sentence.transview .. "❤️🧡❤️"
end

function unload()
    utils.logger:info("SampleTextPlugin 卸载")
end

-- 可选入口还有 dPreRun、preRun、postRun。
