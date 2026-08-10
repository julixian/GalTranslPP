-- 所有已注册类型和函数详见 GalTranslPP/LuaManager.cpp。
-- C++ 会在 init/run 前把当前 NormalJsonTranslator 注入为全局 luaTranslator。

local function logRelFilePaths(relFilePaths)
    local lines = {}
    for i, relFilePath in ipairs(relFilePaths) do
        lines[i] = tostring(relFilePath)
    end
    utils.logger:info("本轮待处理文件:\n" .. table.concat(lines, "\n"))
end

local function finishSharedPostSteps()
    if luaTranslator.m_reuseRepeatedBlocks and luaTranslator.m_transEngine ~= TransEngine.ShowNormal then
        luaTranslator:resolveRepeatedBlockReferences()
    end
    if luaTranslator.m_agentEnabled and luaTranslator.m_transAgent ~= nil then
        luaTranslator.m_transAgent:applyAgentSuggestions()
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

    -- 由于语言性质，Lua 文件插件本身无法手写多线程，只能把文件表交回 C++ 线程池。
    logRelFilePaths(relFilePaths)
    luaTranslator:normalJsonProcessFiles(relFilePaths)
    finishSharedPostSteps()
end

function init()
    utils.logger:info("SampleNormalJsonFilePlugin 初始化")
    utils.logger:info("当前项目目录: " .. tostring(luaTranslator.m_projectDir))
end

function run()
    luaTranslator:normalJsonInit()
    luaTranslator:normalJsonBeforeRun()

    local ok, err = pcall(function()
        processCurrentFilesWithLua()
    end)

    luaTranslator:normalJsonAfterRun()
    if not ok then
        error(err)
    end
end

function runStandardLifecycle()
    -- 不需要在 Lua 中查看或调整文件列表时，直接走标准 NormalJson 生命周期即可。
    luaTranslator:normalJsonInit()
    luaTranslator:normalJsonBeforeRun()
    luaTranslator:normalJsonProcess()
    luaTranslator:normalJsonAfterRun()
end

function unload()
    utils.logger:info("SampleNormalJsonFilePlugin 卸载")
end
