-- C++ 会在 init/run 前把当前 EpubTranslator 注入为全局 luaTranslator。
-- EPUB 的解包、JSON 生成和回打包仍由 C++ 侧负责，Lua 这里演示如何插入生命周期并查看映射表。

local function countMap(map)
    local count = 0
    for _ in pairs(map) do
        count = count + 1
    end
    return count
end

local function logEpubJsonInfo()
    local jsonToInfoMap = luaTranslator.m_jsonToInfoMap
    utils.logger:info("EPUB JSON 映射数量: " .. tostring(countMap(jsonToInfoMap)))

    for jsonPath, jsonInfo in pairs(jsonToInfoMap) do
        utils.logger:info(string.format(
            "EPUB JSON: %s, HTML: %s, textNodes: %d",
            tostring(jsonPath),
            tostring(jsonInfo.htmlPath),
            #jsonInfo.metadata
        ))
    end
end

local function processCurrentFilesWithLua()
    if luaTranslator.m_transEngine == TransEngine.DumpName then
        luaTranslator.m_controller:updateBar(luaTranslator.m_controller.m_totalSentences)
        return
    end

    if luaTranslator.m_transEngine == TransEngine.NameTrans then
        luaTranslator.m_nameTranslator:run(luaTranslator.m_nameTablePath)
        return
    end

    if luaTranslator.m_transEngine == TransEngine.GenDict then
        luaTranslator.m_dictionaryGenerator:generate(luaTranslator.m_projectDir / "ProjGptDict-Gen.toml")
        return
    end

    local relFilePaths = luaTranslator.m_currentRunRelFilePaths
    if relFilePaths == nil then
        return
    end

    luaTranslator:normalJsonProcessFiles(relFilePaths)

    if luaTranslator.m_reuseRepeatedBlocks and luaTranslator.m_transEngine ~= TransEngine.ShowNormal then
        luaTranslator:resolveRepeatedBlockReferences()
    end
    if luaTranslator.m_agentEnabled and luaTranslator.m_transAgent ~= nil then
        luaTranslator.m_transAgent:applyAgentSuggestions()
    end
end

function init()
    utils.logger:info("SampleEpubFilePlugin 初始化")
    utils.logger:info("当前项目目录: " .. tostring(luaTranslator.m_projectDir))
end

function run()
    luaTranslator:normalJsonInit()
    luaTranslator:epubInit()
    luaTranslator:epubBeforeRun()
    luaTranslator:normalJsonBeforeRun()

    local ok, err = pcall(function()
        logEpubJsonInfo()
        processCurrentFilesWithLua()
    end)

    luaTranslator:normalJsonAfterRun()
    if not ok then
        error(err)
    end
end

function runStandardLifecycle()
    luaTranslator:normalJsonInit()
    luaTranslator:epubInit()
    luaTranslator:epubBeforeRun()
    luaTranslator:normalJsonBeforeRun()
    luaTranslator:normalJsonProcess()
    luaTranslator:normalJsonAfterRun()
end

function unload()
    utils.logger:info("SampleEpubFilePlugin 卸载")
end
