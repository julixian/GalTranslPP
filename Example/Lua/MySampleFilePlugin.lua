-- 所有已注册类型和函数详见 GalTranslPP/LuaManager.cpp

function isApiTranslationEngine(transEngine)
    if transEngine == TransEngine.ForGalJson or transEngine == TransEngine.ForGalTsv
        or transEngine == TransEngine.ForNovelTsv or transEngine == TransEngine.DeepseekJson
        or transEngine == TransEngine.Sakura then
        return true
    end
    return false
end

function run()
    -- Lua 没有多线程，所有函数都是阻塞的
    -- 这导致 Lua 的文件多线程只能依赖 C++ 侧开辟线程
    -- 但并不影响 TextPlugin，即使你的 conditionFunc 放在这个脚本里也没问题
    -- 因为 FilePlugin 和 TextPlugin 用的是不同的 LuaState(Lua环境)
    -- normalJsonProcess() 是完整标准流程：处理 m_currentRunRelFilePaths，并执行重复块引用复用、Agent 建议等后续统一阶段
    -- normalJsonProcessFiles(relFilePaths) 是文件级线程池辅助：只把传入文件按 m_threadsNum 投递给 processFile 并等待完成
    -- 如果想在 Lua 中自己挑选/排序文件，可以先改 relFilePaths，再调用 normalJsonProcessFiles(relFilePaths)
    -- beforeRun() 会进行人名表更新，文件分割等操作，字典生成也是在这步里完成的
    luaTranslator:normalJsonBeforeRun()
    local relFilePaths = luaTranslator.m_currentRunRelFilePaths
    if relFilePaths == nil then
        utils.logger:info("可能是 DumpName/NameTrans/GenDict 之类无需 processFile 的 TransEngine")
        luaTranslator:normalJsonAfterRun()
        return
    end
    local relFilePathsStr = {}
    for i = 1, #relFilePaths do
        table.insert(relFilePathsStr, relFilePaths[i].value)
    end
    utils.logger:info("relFilePaths: \n" .. table.concat(relFilePathsStr, "\n"))
    luaTranslator:normalJsonProcessFiles(relFilePaths)
    if not luaTranslator.m_controller:shouldStop() then
        if luaTranslator.m_useRepeatedBlockInputCache and isApiTranslationEngine(luaTranslator.m_transEngine) then
            luaTranslator:resolveRepeatedBlockReferences()
        end
        if luaTranslator.m_agentEnabled then
            luaTranslator:applyAgentRetranslateSuggestions()
        end
    end
    luaTranslator:normalJsonAfterRun()
    return

    -- --或者直接交给 Process() 函数
    -- luaTranslator:normalJsonBeforeRun()
    -- if luaTranslator.m_currentRunRelFilePaths == nil then
    --     utils.logger:info("可能是 DumpName/NameTrans/GenDict 之类无需 processFile 的 TransEngine")
    --     luaTranslator:normalJsonAfterRun()
    --     return
    -- end
    -- luaTranslator:normalJsonProcess()
    -- luaTranslator:normalJsonAfterRun()
    -- return

    -- -- Epub 的 beforeRun() 主要是把 epub 转化为可翻译的 json 并定义 onFileProcessed 为重组函数
    -- -- 由于最后还是使用 NormalJson 类进行翻译，所以大部分情况下两个 beforeRun 都要执行
    -- luaTranslator:epubBeforeRun()
    -- -- std::function<void(fs::path)> m_onFileProcessed;
    -- orgOnFileProcessedInEpubTranslator = luaTranslator.m_onFileProcessed
    -- -- 有完整的文件处理完毕后会回调这个函数，可以是闭包
    -- luaTranslator.m_onFileProcessed = function (relFilePathProcessed)
    --     utils.logger:info("拦截然后加一条日志喵")
    --     orgOnFileProcessedInEpubTranslator(relFilePathProcessed)
    -- end
    -- luaTranslator:normalJsonBeforeRun()
    -- luaTranslator:normalJsonProcess()
    -- luaTranslator:normalJsonAfterRun()
    -- return
    
end

function init()
    -- 读取 config 和字典等
    luaTranslator:normalJsonInit()
    -- Epub/PDF 是继承自 NormalJson 的，如果 Lua 继承的是 Epub/PDF 等则大部分情况下两个 init() 都需要运行
    -- 具体可看它们在 GalTranslPP/xxTranslator.ixx xxTranslator::run() 虚函数中的执行顺序及各自的作用
    -- luaTranslator:epubInit()
    utils.logger:info("MySampleFilePluginFromLua starts")
    utils.logger:info(string.format("Current inputDir: %s", luaTranslator.m_inputDir))
    local tomlConfig, errMsg = toml.parse(luaTranslator.m_projectDir / "config.toml")
    if tomlConfig == nil then
        utils.logger:info("出错错了喵: " .. errMsg)
    else
        local epubPreReg1 = tomlConfig.plugins.Epub.preprocRegex[1]
        utils.logger:info("{epubPreReg1} org: " .. epubPreReg1.org .. ", rep: " .. epubPreReg1.rep)
    end
end

function countMap(map)
    local count = 0
    for k, v in pairs(map) do
        count = count + 1
    end
    return count
end

function unload()
    utils.logger:info("MySampleFilePluginFromLua unloads")
    -- absl::flat_hash_map<fs::path, std::map<fs::path, bool>> m_jsonToSplitFileParts;
    local partsTable = luaTranslator.m_jsonToSplitFileParts
    local strs = {}
    for jsonPath, filePartsMap in pairs(partsTable) do
        for splitFilePart, completed in pairs(filePartsMap) do
            if completed then
                table.insert(strs, splitFilePart.value)
                local j, errMsg = json.parse(luaTranslator.m_transCacheDir / splitFilePart)
                if j == nil then
                    utils.logger:info("出错错了喵: " .. errMsg)
                elseif #j >= 1 then
                    table.insert(strs, j[1].translated_preview)
                end
            else
                utils.logger:error(string.format("文件 %s 尚未翻译完成", splitFilePart))
            end
        end
    end
    utils.logger:info("map keys: \n" .. table.concat(strs, "\n"))

    -- 为了能使用 pairs 遍历，map返回类型都是副本，如果想修改，必须 luaTranslator.mapVar = copy
    utils.logger:info("原有 " .. tostring(countMap(luaTranslator.m_jsonToSplitFileParts)) .. " 个 key-value 对")
    local newPartTable = { MyNewPartPath1 = false, ["MyNewPartPath2"] = false }
    luaTranslator.m_jsonToSplitFileParts[Path.new("MyNewJsonPathKey")] = newPartTable
    utils.logger:info("依然为 " .. tostring(countMap(luaTranslator.m_jsonToSplitFileParts)) .. " 个 key-value 对")
    partsTable[Path.new("MyNewJsonPathKey")] = newPartTable
    luaTranslator.m_jsonToSplitFileParts = partsTable
    utils.logger:info("现有 " .. tostring(countMap(luaTranslator.m_jsonToSplitFileParts)) .. " 个 key-value 对")
    return

    -- -- EpubTranslator 的成员
    -- local jsonToInfoMap = luaTranslator.m_jsonToInfoMap
    -- for jsonPath, jsonInfo in pairs(jsonToInfoMap) do
    --     utils.logger:info("metadataVecSize: " .. tostring(#jsonInfo.metadata))
    --     for i=1, #jsonInfo.metadata do
    --         utils.logger:info("offset: " .. tostring(jsonInfo.metadata[i].offset) .. ", length: " .. tostring(jsonInfo.metadata[i].length))
    --     end
    -- end
    -- return
end
