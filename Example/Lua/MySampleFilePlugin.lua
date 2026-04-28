-- 所有已注册类型和函数详见 GalTranslPP/LuaManager.cpp

function run()
    -- Lua 没有多线程，所有函数都是阻塞的
    -- 这导致 Lua 的文件多线程只能依赖 C++ 侧开辟线程
    -- 但并不影响 TextPlugin，即使你的 conditionFunc 放在这个脚本里也没问题
    -- 因为 FilePlugin 和 TextPlugin 用的是不同的 LuaState(Lua环境)
    -- 如果想在 Lua 中使用 C++ NormalJsonTranslaor 的文件多线程
    -- 除过直接调用 xxTranslator_process() 函数之外
    -- 唯一的办法是使用 m_threadPool:push(luaTranslator , Vec{Paths}) (此方法为 Lua 独有，包括这个函数本身也是阻塞的)
    -- beforeRun() 会进行人名表更新，文件分割等操作，字典生成也是在这步里完成的，返回所有分割后的文件的相对路径
    local relFilePaths = luaTranslator:normalJsonTranslator_beforeRun()
    if relFilePaths == nil then
        utils.logger:info("可能是 DumpName 或 GenDict 之类无需 processFile 的 TransEngine")
        return
    end
    local relFilePathsStr = {}
    for i = 1, #relFilePaths do
        table.insert(relFilePathsStr, relFilePaths[i].value)
    end
    utils.logger:info("relFilePaths: \n" .. table.concat(relFilePathsStr, "\n"))
    luaTranslator.m_threadPool:resize(35)
    luaTranslator.m_controller:makeBar(luaTranslator.m_controller.m_totalSentences, 35)
    luaTranslator.m_threadPool:push(luaTranslator, relFilePaths)
    luaTranslator:normalJsonTranslator_afterRun()
    return

    -- --或者直接交给 process() 函数
    -- local relFilePaths = luaTranslator:normalJsonTranslator_beforeRun()
    -- if relFilePaths == nil then
    --     utils.logger:info("可能是 DumpName 或 GenDict 之类无需 processFile 的 TransEngine")
    --     return
    -- end
    -- luaTranslator:normalJsonTranslator_process(relFilePaths)
    -- luaTranslator:normalJsonTranslator_afterRun()
    -- return

    -- -- Epub 的 beforeRun() 主要是把 epub 转化为可翻译的 json 并定义 onFileProcessed 为重组函数
    -- -- 由于最后还是使用 NormalJson 类进行翻译，所以大部分情况下两个 beforeRun 都要执行
    -- luaTranslator:epubTranslator_beforeRun()
    -- -- std::function<void(fs::path)>
    -- orgOnFileProcessedInEpubTranslator = luaTranslator.m_onFileProcessed
    -- -- 有完整的文件处理完毕后会回调这个函数，可以是闭包
    -- luaTranslator.m_onFileProcessed = function (relFilePathProcessed)
    --     utils.logger:info("拦截然后加一条日志喵")
    --     orgOnFileProcessedInEpubTranslator(relFilePathProcessed)
    -- end
    -- local relFilePaths = luaTranslator:normalJsonTranslator_beforeRun()
    -- if relFilePaths == nil then
    --     utils.logger:info("可能是 DumpName 或 GenDict 之类无需 processFile 的 TransEngine")
    --     return
    -- end
    -- luaTranslator:normalJsonTranslator_process(relFilePaths)
    -- luaTranslator:normalJsonTranslator_afterRun()
    -- return
    
end

function init()
    -- 读取 config 和字典等
    luaTranslator:normalJsonTranslator_init()
    -- Epub/PDF 是继承自 NormalJson 的，如果 Lua 继承的是 Epub/PDF 等则大部分情况下两个 init() 都需要运行
    -- 具体可看它们在 GalTranslPP/xxTranslator.ixx xxTranslator::run() 虚函数中的执行顺序及各自的作用
    -- luaTranslator:epubTranslator_init()
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
    -- std::map<fs::path, std::map<fs::path, bool>> m_jsonToSplitFileParts;
    local partsTable = luaTranslator.m_jsonToSplitFileParts
    local strs = {}
    for jsonPath, filePartsMap in pairs(partsTable) do
        for splitFilePart, completed in pairs(filePartsMap) do
            if completed then
                table.insert(strs, splitFilePart.value)
                local j, errMsg = json.parse(luaTranslator.m_cacheDir / splitFilePart)
                if j == nil then
                    utils.logger:info("出错错了喵: " .. errMsg)
                elseif #j >= 1 then
                    table.insert(strs, j[1].translated_preview)
                end
            else
                utils.logger:error("不应该发生")
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
