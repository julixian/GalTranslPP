--[[ 

GPPDefines.ixx:

enum class NameType
{
    None, Single, Multiple
};

struct SentencePosition {
    std::string file;
    int index = -1;

    friend bool operator==(const SentencePosition&, const SentencePosition&) = default;
};

struct Sentence {
    int index = -1;
    std::string fileName;
    std::string name;
    std::vector<std::string> names;
    std::string name_preview;
    std::vector<std::string> names_preview;
    std::string original_text;
    std::string pre_processed_text;
    std::string pre_translated_text;
    std::vector<std::string> problems;
    std::string translated_by;
    std::string translated_preview;
    std::string originalLinebreak;
    std::map<std::string, std::string> other_info;
    std::optional<SentencePosition> repeatedBlockRefTo;
    std::vector<SentencePosition> repeatedBlockRefBy;

    NameType nameType = NameType::None;
    Sentence* prev = nullptr;
    Sentence* next = nullptr;
    bool complete = false;
    bool notAnalyzeProblem = false;
    bool repeatedBlockRefPending = false;

    std::optional<std::string> problems_get_by_index(int index) {
        if (index < 0 || index >= problems.size()) {
            return std::nullopt;
        }
        return problems[index];
    }

    bool problems_set_by_index(int index, const std::string& problem) {
        if (index < 0 || index >= problems.size()) {
            return false;
        }
        problems[index] = problem;
        return true;
    }
};

enum class TransEngine
{
    None, ForGalJson, ForGalTsv, ForNovelTsv, DeepseekJson, Sakura, DumpName, NameTrans, GenDict, Rebuild, ShowNormal
};

utils里的工具函数的签名详见 LuaMnager::registerCustomTypes 中绑定 utils 库的部分
]]

sourceLang_useTokenizer = false
targetLang_useTokenizer = true
--sourceLang_tokenizerBackend = "MeCab"
targetLang_tokenizerBackend = "pkuseg"
--sourceLang_meCabDictDir = "..."
--targetLang_spaCyModelName = "..."
--targetLang_stanzaLang = "..."
--...

function checkConditionForRetranslKeysFunc(sentence)
    if sentence.index == 278 then 
        utils.logger:info("RetransFromLua")
    end
    return false
end

function checkConditionForSkipProblemsFunc(sentence)
    if sentence.index == 278 then 
        -- Example/Python/MySampleTextPlugin 中有对此函数的进一步介绍
        current_problem = ""
        for i, s in ipairs(sentence.problems) do
            if string.find(s, "^Current problem:") then 
                current_problem = string.sub(s, 17)
            else
                local success = sentence:problems_set_by_index(i-1, "My new problem")
                if success then
                    sentence.other_info["luaSuccess"] = "New problem suc"
                else
                    sentence.other_info["luaFail"] = "New problem fail"
                end
            end
        end
        local other_info_map = sentence.other_info
        other_info_map["luaCheck"] = "The 278th"
        other_info_map["luaCp"] = current_problem
        sentence.other_info = other_info_map
    end
    return false
end

function dPostRun(sentence)
    -- 为了能使用 pairs 遍历，map 返回类型都是副本，如果想修改，必须 mapVar = copy
    -- 但非递归的 vector 类型可使用 sol2 提供的 container 通用函数进行引用修改，具体函数及其作用详见
    -- https://github.com/ThePhD/sol2/blob/main/documentation/source/containers.rst 的 container operations 部分
    local other_info_map = sentence.other_info
    if sentence.index == 0 then
        other_info_map["MySampleKeyForFirstSentence"] = "MySampleValueForFirstSentence"
        sentence.other_info = other_info_map
    elseif sentence.index == 278 then
        sentence.problems:add("278FlagProblem")
    end
    if sentence.next == nil and sentence.prev ~= nil and sentence.prev.nameType == NameType.None then
        sentence.problems:add("MySampleProblemForLastSentence")
        sentence.notAnalyzeProblem = true
    end
    if sentence.translated_preview == "久远紧锁眉头，脸上写着「骗人的吧，喂」。" then
        -- utils, json, toml 等无实例类的纯工具表直接使用 .func(...) 调用即可
        -- logger，translator 等有具体实例的对象需要用 :func(...) 或 .func(self, ...) 调用
        local wordpos_vec, entity_vec = utils.targetLang_tokenizeFunc(sentence.translated_preview)
        local parts = {}
        for i, value in ipairs(wordpos_vec) do
            parts[i] = "[" .. value[1] .. "]"
        end
        local result = table.concat(parts, "#")
        other_info_map["tokens_lua"] = result
        utils.logger.info(utils.logger, string.format("测试 logger ，当前 index: %d", sentence.index))
        sentence.other_info = other_info_map
    end
end

function init(projectDir)
    utils.logger:info(string.format("MySampleTextPluginFromLua 初始化成功，projectDir: '%s'", projectDir.value))
end

-- function unload()
--     -- ctrl + /
--     -- return
--     -- utils.logger:info("MySampleTextPluginFromLua unloads")
--     -- local cmd = [[D:\GALGAME\linshi\天冥のコンキスタ\WORK\#toSjis.bat]]
--     -- local cmd_acp = utils.ascii2Ascii(cmd, 65001, 0)
--     -- local handle = io.popen(cmd_acp, 'r')
--     -- if handle then
--     --     local output = handle:read("*a")
--     --     handle:close()
--     --     utils.logger:info("--- BAT 脚本的输出内容 开始 ---\n" .. output .. "\n--- BAT 脚本的输出内容 结束 ---")
--     -- else
--     --     utils.logger:error("无法执行 BAT")
--     -- end
-- end

-- dPreRun, preRun, postRun, dPostRun, unload 均可选择性定义，如果定义则分别在对应阶段调用
