module;

#include "GPPMacros.hpp"
#include <toml.hpp>
#include <sol/sol.hpp>
#include <ctpl_stl.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/regex.h>

module LuaManager;

import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import LuaTranslator;
import NLPTool;

import ITranslator;
import Tool;

namespace fs = std::filesystem;

class LuaJson {
public:

	// 从 sol::object 转换到 json 的辅助函数
	static json solObj2JsonValue(sol::object obj) {
		sol::type type = obj.get_type();
		switch (type) 
		{
		case sol::type::string:
			return obj.as<std::string>();
		case sol::type::number:
			if (obj.is<int64_t>()) {
				return obj.as<int64_t>();
			}
			return obj.as<double>();
		case sol::type::boolean:
			return obj.as<bool>();
		case sol::type::table: {
			sol::table luaTable = obj.as<sol::table>();
			bool arrayLike = true;
			if (luaTable.empty()) {
				// 如果是空表，我们默认它是一个数组。
				// 这是一个约定，你也可以选择默认为 table。
				// 对于 j 来说，空的 array [] 更常见。
				arrayLike = true;
			}
			else {
				// 遍历所有键，检查它们是否都是从1开始的连续整数
				size_t expectedKey = 1;
				for (auto& kv : luaTable) {
					if (!kv.first.is<lua_Integer>() || kv.first.as<lua_Integer>() != expectedKey) {
						arrayLike = false;
						break;
					}
					++expectedKey;
				}
				// 确保 Lua table 的 #size 和我们遍历的元素数量一致
				arrayLike &= (luaTable.size() == expectedKey - 1);
			}

			if (arrayLike) {
				json arr = json::array();
				for (auto& kv : luaTable) {
					arr.push_back(solObj2JsonValue(kv.second));
				}
				return arr;
			}
			else { // 否则，当作字典处理
				json tbl = json::object();
				for (auto& kv : luaTable) {
					// 确保键是字符串，因为 j 的键必须是字符串
					if (kv.first.is<std::string>()) {
						tbl[kv.first.as<std::string>()] = solObj2JsonValue(kv.second);
					}
					else {
						throw std::runtime_error(gppTr("LuaJson.solObj2JsonValue", "LuaJson: key 必须是字符串")
						    .toStdString());
					}
				}
				return tbl;
			}
		}
		case sol::type::nil:
			return nullptr;
		default:
			return "LuaJson: unsupported type";
		}
	}

	// 递归转换函数：将 json 转换为 sol::object
	static sol::object jsonValue2SolObject(const json& value, sol::state_view lua) {
		// 检查节点类型并进行相应转换
		switch (value.type()) 
		{
		case json::value_t::string:
			return sol::make_object(lua, value.get<std::string>());
		case json::value_t::number_unsigned:
			return sol::make_object(lua, value.get<uint64_t>());
		case json::value_t::number_integer:
			return sol::make_object(lua, value.get<int64_t>());
		case json::value_t::number_float:
			return sol::make_object(lua, value.get<double>());
		case json::value_t::boolean:
			return sol::make_object(lua, value.get<bool>());
		case json::value_t::array: {
			sol::table resultArray = lua.create_table();
			for (const auto& elem : value) {
				resultArray.add(jsonValue2SolObject(elem, lua));
			}
			return sol::make_object(lua, resultArray);
		}
		case json::value_t::object: {
			sol::table resultMap = lua.create_table();
			for (const auto& jObj : value.items()) {
				resultMap[jObj.key()] = jsonValue2SolObject(jObj.value(), lua);
			}
			return sol::make_object(lua, resultMap);
		}
		case json::value_t::null:
		case json::value_t::discarded:
		default:
			return sol::make_object(lua, sol::nil);
		}
	}
};

class LuaToml {
public:

	// 从 sol::object 转换到 toml::node 的辅助函数
	static toml::value solObj2TomlValue(sol::object obj) {
		sol::type type = obj.get_type();
		switch (type) 
		{
		case sol::type::string:
			return toml::value(obj.as<std::string>());
		case sol::type::number:
			if (obj.is<int64_t>()) {
				return toml::value(obj.as<int64_t>());
			}
			return toml::value(obj.as<double>());
		case sol::type::boolean:
			return toml::value(obj.as<bool>());
		case sol::type::table: {
			sol::table luaTable = obj.as<sol::table>();
			bool arrayLike = true;
			if (luaTable.empty()) {
				// 如果是空表，我们默认它是一个数组。
				// 这是一个约定，你也可以选择默认为 table。
				// 对于 toml 来说，空的 array [] 更常见。
				arrayLike = true;
			}
			else {
				// 遍历所有键，检查它们是否都是从1开始的连续整数
				size_t expectedKey = 1;
				for (auto& kv : luaTable) {
					if (!kv.first.is<lua_Integer>() || kv.first.as<lua_Integer>() != expectedKey) {
						arrayLike = false;
						break;
					}
					++expectedKey;
				}
				// 确保 Lua table 的 #size 和我们遍历的元素数量一致
				arrayLike &= (luaTable.size() == expectedKey - 1);
			}

			if (arrayLike) {
				toml::array arr;
				for (auto& kv : luaTable) {
					arr.push_back(solObj2TomlValue(kv.second));
				}
				return arr;
			}
			else { // 否则，当作字典处理
				toml::table tbl;
				for (auto& kv : luaTable) {
					// 确保键是字符串，因为 TOML 的键必须是字符串
					if (kv.first.is<std::string>()) {
						tbl.insert({ kv.first.as<std::string>(), solObj2TomlValue(kv.second) });
					}
					else {
						throw std::runtime_error(gppTr("LuaToml.solObj2TomlValue", "LuaToml: key 必须是字符串")
						    .toStdString());
					}
				}
				return tbl;
			}
		}
		default:
			return toml::value{ "LuaToml: unsupported type" };
		}
	}

	// 递归转换函数：将 toml::value 转换为 sol::object
	static sol::object tomlValue2SolObject(const toml::value& value, sol::state_view lua) {
		// 检查节点类型并进行相应转换
		if (value.is_table()) {
			sol::table resultMap = lua.create_table();
			for (const auto& [key, val] : value.as_table()) {
				resultMap[key] = tomlValue2SolObject(val, lua);
			}
			return resultMap;
		}
		else if (value.is_array()) {
			sol::table resultVec = lua.create_table();
			for (const auto& elem : value.as_array()) {
				resultVec.add(tomlValue2SolObject(elem, lua));
			}
			return resultVec;
		}
		else if (value.is_string()) {
			return sol::make_object(lua, value.as_string());
		}
		else if (value.is_integer()) {
			return sol::make_object(lua, value.as_integer());
		}
		else if (value.is_floating()) {
			return sol::make_object(lua, value.as_floating());
		}
		else if (value.is_boolean()) {
			return sol::make_object(lua, value.as_boolean());
		}
		// 对于其他类型（如 toml::date, toml::time），我们返回 nil
		return sol::make_object(lua, sol::nil);
	}
};

std::optional<std::shared_ptr<LuaStateInstance>> LuaManager::registerFunction(const std::string& scriptPath, const std::string& functionName) {
	const fs::path stdScriptPath = fs::weakly_canonical(ascii2Wide(scriptPath));
	if (!fs::exists(stdScriptPath)) {
		m_logger->error(gppTr("LuaManager.registerFunction", "脚本不存在: %1").arg(scriptPath).toStdString());
		return std::nullopt;
	}

	auto it = m_scriptStates.find(stdScriptPath);
	if (it == m_scriptStates.end()) {
		try {
			auto& state = m_scriptStates[stdScriptPath];
			state = std::make_shared<LuaStateInstance>();
			state->lua->open_libraries();
			std::ifstream ifs(stdScriptPath, std::ios::binary);
			std::string script((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			state->lua->script(script);
			registerCustomTypes(state, scriptPath);
			it = m_scriptStates.find(stdScriptPath);
		}
		catch (const sol::error& e) {
			m_logger->error(gppTr("LuaManager.registerFunction", "加载脚本 %1 失败: %2")
			    .arg(scriptPath)
			    .arg(e.what())
			    .toStdString());
			return std::nullopt;
		}
	}

	if (functionName.empty()) {
		return it->second;
	}

	if (!it->second->functions.contains(functionName)) {
		auto pFunc = std::make_unique<sol::function>((*(it->second->lua))[functionName]);
		if (!pFunc->valid()) {
			m_logger->debug(gppTr("LuaManager.registerFunction", "在脚本 %1 中未找到函数 %2")
			    .arg(scriptPath)
			    .arg(functionName)
			    .toStdString());
			return std::nullopt;
		}
		it->second->functions.insert({ functionName, std::move(pFunc) });
	}

	return it->second;
}

void LuaManager::registerCustomTypes(const std::shared_ptr<LuaStateInstance>& luaStateInstance, const std::string& scriptPath) {
	sol::state& lua = *(luaStateInstance->lua);
	// 绑定 NameType 枚举
	lua.new_enum("NameType",
		"None", NameType::None,
		"Single", NameType::Single,
		"Multiple", NameType::Multiple
	);

	lua.new_enum("TransEngine",
		"None", TransEngine::None,
		"ForGalJson", TransEngine::ForGalJson,
		"ForGalTsv", TransEngine::ForGalTsv,
		"ForNovelTsv", TransEngine::ForNovelTsv,
		"DeepseekJson", TransEngine::DeepseekJson,
		"Sakura", TransEngine::Sakura,
		"DumpName", TransEngine::DumpName,
		"NameTrans", TransEngine::NameTrans,
		"GenDict", TransEngine::GenDict,
		"Rebuild", TransEngine::Rebuild,
		"ShowNormal", TransEngine::ShowNormal
	);

	lua.new_usertype<SentencePosition>("SentencePosition",
		sol::constructors<SentencePosition()>(),
		"file", &SentencePosition::file,
		"index", &SentencePosition::index
	);

	// 绑定 Sentence 结构体
	lua.new_usertype<Sentence>("Sentence",
		sol::constructors<Sentence()>(), // 允许在 Lua 中创建 Sentence
		"index", &Sentence::index,
		"fileName", &Sentence::fileName,
		"name", &Sentence::name,
		"names", &Sentence::names,
		"name_preview", &Sentence::name_preview,
		"names_preview", &Sentence::names_preview,
		"original_text", &Sentence::original_text,
		"pre_processed_text", &Sentence::pre_processed_text,
		"pre_translated_text", &Sentence::pre_translated_text,
		"problems", &Sentence::problems,
		"translated_by", &Sentence::translated_by,
		"translated_preview", &Sentence::translated_preview,
		"other_info", NESTED_CVT(Sentence, other_info),
		"repeatedBlockRefTo", &Sentence::repeatedBlockRefTo,
		"repeatedBlockRefBy", &Sentence::repeatedBlockRefBy,
		"problems_get_by_index", &Sentence::problems_get_by_index,
		"problems_set_by_index", &Sentence::problems_set_by_index,
		"complete", &Sentence::complete,
		"notAnalyzeProblem", &Sentence::notAnalyzeProblem,
		"repeatedBlockRefPending", &Sentence::repeatedBlockRefPending,
		"nameType", &Sentence::nameType,
		"prev", &Sentence::prev,
		"next", &Sentence::next,
		"originalLinebreak", &Sentence::originalLinebreak
	);

	lua.new_usertype<fs::path>("Path",
		sol::meta_function::construct,
		sol::factories([](const std::string& str)
			{
				return fs::path(ascii2Wide(str));
			},
			[]()
			{
				return fs::path();
			},
			[](const fs::path& p)
			{
				return fs::path(p);
			}),
		"value", sol::property([](const fs::path& self)
			{
				return wide2Ascii(self);
			},
			[](fs::path& self, const std::string& str)
			{
				self = ascii2Wide(str);
			}),
		sol::meta_function::to_string,
		[](const fs::path& self)
		{
			return wide2Ascii(self);
		},
		sol::meta_function::division, sol::overload(
			[](const fs::path& self, const fs::path& other) { return self / other; },
			[](const fs::path& self, const std::string& other) { return self / ascii2Wide(other); },
			[](const std::string& self, const fs::path& other) { return ascii2Wide(self) / other; }),
		sol::meta_function::equal_to, [](const fs::path& self, const fs::path& other) { return self == other; },
		"filename", sol::property([](const fs::path& self) { return self.filename(); }),
		"stem", sol::property([](const fs::path& self) { return self.stem(); }),
		"extension", sol::property([](const fs::path& self) { return self.extension(); }),
		"parent_path", sol::property([](const fs::path& self) { return self.parent_path(); }),
		"empty", sol::property([](const fs::path& self) { return self.empty(); }),
		"is_absolute", sol::property([](const fs::path& self) { return self.is_absolute(); }),
		"is_relative", sol::property([](const fs::path& self) { return self.is_relative(); }),
		"equivalent", [](const fs::path& self, const fs::path& other) { return fs::equivalent(self, other); },
		"weakly_canonical", [](const fs::path& self) { return fs::weakly_canonical(self); },
		"canonical", [](const fs::path& self) { return fs::canonical(self); },
		"relative_to", [](const fs::path& self, const fs::path& base) { return fs::relative(self, base); }
	);

	sol::table luaTomlTable = lua.create_named_table("toml");
	auto luaTomlParseFunc = [](fs::path path, sol::this_state s) -> std::tuple<sol::object, std::optional<std::string>>
		{
			sol::state_view lua = s;
			try {
				toml::value tomlValue = toml::uparse(path);
				return std::make_tuple(LuaToml::tomlValue2SolObject(tomlValue, lua), std::nullopt);
			}
			catch (const toml::exception& e) {
				return std::make_tuple(sol::make_object(lua, sol::nil), std::string(e.what()));
			}
		};
	luaTomlTable["parse"] = sol::overload(luaTomlParseFunc, [=](const std::string& str, sol::this_state s) { return luaTomlParseFunc(ascii2Wide(str), s); });
	auto luaTomlSaveFunc = [](const fs::path& path, sol::object obj) -> std::tuple<bool, std::optional<std::string>>
		{
			toml::value tomlValue = LuaToml::solObj2TomlValue(obj);
			try {
				std::ofstream ofs(path, std::ios::binary);
				if (!ofs.is_open()) {
					return std::make_tuple(false, std::string("Failed to open file for writing"));
				}
				ofs << tomlValue;
				ofs.close();
				return std::make_tuple(true, std::nullopt);
			}
			catch (const toml::exception& e) {
				return std::make_tuple(false, std::string(e.what()));
			}
		};
	luaTomlTable["str"] = [](sol::object obj) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
		{
			toml::value tomlValue = LuaToml::solObj2TomlValue(obj);
			try {
				return std::make_tuple(toml::format(tomlValue), std::nullopt);
			}
			catch (const toml::exception& e) {
				return std::make_tuple(std::nullopt, std::string(e.what()));
			}
		};
	luaTomlTable["save"] = sol::overload(luaTomlSaveFunc, [=](const std::string& str, sol::object obj) { return luaTomlSaveFunc(ascii2Wide(str), obj); });

	auto luaJsonTable = lua.create_named_table("json");
	auto luaJsonParseFunc = [](const fs::path& path, sol::this_state s) -> std::tuple<sol::object, std::optional<std::string>>
		{
			sol::state_view lua = s;
			try {
				std::ifstream ifs(path, std::ios::binary);
				json jsonValue = json::parse(ifs);
				return std::make_tuple(LuaJson::jsonValue2SolObject(jsonValue, lua), std::nullopt);
			}
			catch (const std::exception& e) {
				return std::make_tuple(sol::make_object(lua, sol::nil), std::string(e.what()));
			}
		};
	luaJsonTable["parse"] = sol::overload(luaJsonParseFunc, [=](const std::string& str, sol::this_state s) { return luaJsonParseFunc(ascii2Wide(str), s); });
	auto luaJsonSaveFunc = [](const fs::path& path, sol::object obj, sol::optional<int> indent) -> std::tuple<bool, std::optional<std::string>>
		{
			json jsonValue = LuaJson::solObj2JsonValue(obj);
			try {
				std::ofstream ofs(path, std::ios::binary);
				if (!ofs.is_open()) {
					return std::make_tuple(false, std::string("Failed to open file for writing"));
				}
				ofs << jsonValue.dump(indent.value_or(2));
				ofs.close();
				return std::make_tuple(true, std::nullopt);
			}
			catch (const std::exception& e) {
				return std::make_tuple(false, std::string(e.what()));
			}
		};
	luaJsonTable["save"] = sol::overload(luaJsonSaveFunc, [=](const std::string& str, sol::object obj, sol::optional<int> indent) { return luaJsonSaveFunc(ascii2Wide(str), obj, indent); });


	lua.new_usertype<ITranslator>("ITranslator",
		sol::no_constructor,
		"run", &ITranslator::run
	);
	lua.new_usertype<RuntimeSuccessEvent>("RuntimeSuccessEvent",
		sol::constructors<RuntimeSuccessEvent()>(),
		"timestamp", &RuntimeSuccessEvent::timestamp,
		"filename", &RuntimeSuccessEvent::filename,
		"index", &RuntimeSuccessEvent::index,
		"speakers", &RuntimeSuccessEvent::speakers,
		"sourcePreview", &RuntimeSuccessEvent::sourcePreview,
		"translationPreview", &RuntimeSuccessEvent::translationPreview,
		"translatedBy", &RuntimeSuccessEvent::translatedBy
	);
	lua.new_usertype<RuntimeErrorEvent>("RuntimeErrorEvent",
		sol::constructors<RuntimeErrorEvent()>(),
		"timestamp", &RuntimeErrorEvent::timestamp,
		"kind", &RuntimeErrorEvent::kind,
		"level", &RuntimeErrorEvent::level,
		"message", &RuntimeErrorEvent::message,
		"filename", &RuntimeErrorEvent::filename,
		"indexRange", &RuntimeErrorEvent::indexRange,
		"retryCount", &RuntimeErrorEvent::retryCount,
		"model", &RuntimeErrorEvent::model,
		"sleepSeconds", &RuntimeErrorEvent::sleepSeconds
	);
	lua.new_usertype<RuntimeFileProgress>("RuntimeFileProgress",
		sol::constructors<RuntimeFileProgress()>(),
		"filename", &RuntimeFileProgress::filename,
		"total", &RuntimeFileProgress::total,
		"completed", &RuntimeFileProgress::completed,
		"problems", &RuntimeFileProgress::problems
	);
	lua.new_usertype<IController>("IController",
		"m_totalSentences", sol::property([](IController& self) { return self.m_totalSentences.load(); },
			[](IController& self, int value) { self.m_totalSentences = value; }),
		"m_completedSentences", sol::property([](IController& self) { return self.m_completedSentences.load(); },
			[](IController& self, int value) { self.m_completedSentences = value; }),
		"m_workersActive", sol::property([](IController& self) { return self.m_workersActive.load(); },
			[](IController& self, int value) { self.m_workersActive = value; }),
		"m_workersConfigured", sol::property([](IController& self) { return self.m_workersConfigured.load(); },
			[](IController& self, int value) { self.m_workersConfigured = value; }),
		"makeBar", &IController::makeBar,
		"writeLog", &IController::writeLog,
		"addThreadNum", &IController::addThreadNum,
		"reduceThreadNum", &IController::reduceThreadNum,
		"updateBar", sol::overload(
			[](IController& self) { self.updateBar(); },
			[](IController& self, int ticks) { self.updateBar(ticks); }),
		"setRuntimeFiles", &IController::setRuntimeFiles,
		"setRuntimeStage", sol::overload(
			[](IController& self, const std::string& stage) { self.setRuntimeStage(stage); },
			[](IController& self, const std::string& stage, const std::string& currentFile) { self.setRuntimeStage(stage, currentFile); }),
		"recordFileSentenceDone", &IController::recordFileSentenceDone,
		"recordRuntimeSuccess", &IController::recordRuntimeSuccess,
		"recordRuntimeError", &IController::recordRuntimeError,
		"shouldStop", &IController::shouldStop,
		"flush", &IController::flush
	);
	lua.new_usertype<ctpl::thread_pool>("ThreadPool",
		sol::no_constructor,
		"resize", &ctpl::thread_pool::resize,
		"size", &ctpl::thread_pool::size
	);
	lua.new_usertype<NormalJsonTranslator>("NormalJsonTranslator",
		sol::base_classes, sol::bases<ITranslator>(),
		"m_transEngine", &NormalJsonTranslator::m_transEngine,
		"m_controller", &NormalJsonTranslator::m_controller,
		"m_logger", &NormalJsonTranslator::m_logger,
		"m_inputDir", &NormalJsonTranslator::m_inputDir,
		"m_inputCacheDir", &NormalJsonTranslator::m_inputCacheDir,
		"m_outputDir", &NormalJsonTranslator::m_outputDir,
		"m_outputCacheDir", &NormalJsonTranslator::m_outputCacheDir,
		"m_transCacheDir", &NormalJsonTranslator::m_transCacheDir,
		"m_otherCacheDir", &NormalJsonTranslator::m_otherCacheDir,
		"m_backgroundTextCachePath", &NormalJsonTranslator::m_backgroundTextCachePath,
		"m_projectDir", &NormalJsonTranslator::m_projectDir,
		"m_agentRootDir", &NormalJsonTranslator::m_agentRootDir,
		"m_agentTermLedgerPath", &NormalJsonTranslator::m_agentTermLedgerPath,
		"m_agentFileNotesDir", &NormalJsonTranslator::m_agentFileNotesDir,
		"m_agentSearchCatalogPath", &NormalJsonTranslator::m_agentSearchCatalogPath,
		"m_backgroundTextCacheMap", NESTED_CVT(NormalJsonTranslator, m_backgroundTextCacheMap),
		"m_systemPrompt", &NormalJsonTranslator::m_systemPrompt,
		"m_userPrompt", &NormalJsonTranslator::m_userPrompt,
		"m_agentSystemPrompt", &NormalJsonTranslator::m_agentSystemPrompt,
		"m_agentUserPrompt", &NormalJsonTranslator::m_agentUserPrompt,
		"m_genDictReviewSystemPrompt", &NormalJsonTranslator::m_genDictReviewSystemPrompt,
		"m_genDictReviewUserPrompt", &NormalJsonTranslator::m_genDictReviewUserPrompt,
		"m_targetLang", &NormalJsonTranslator::m_targetLang,
		"m_pythonTranslator", &NormalJsonTranslator::m_pythonTranslator,
		"m_threadsNum", &NormalJsonTranslator::m_threadsNum,
		"m_nameTransBatchSize", &NormalJsonTranslator::m_nameTransBatchSize,
		"m_batchSize", &NormalJsonTranslator::m_batchSize,
		"m_contextHistorySize", &NormalJsonTranslator::m_contextHistorySize,
		"m_maxRetries", &NormalJsonTranslator::m_maxRetries,
		"m_saveCacheInterval", &NormalJsonTranslator::m_saveCacheInterval,
		"m_apiTimeOutMs", &NormalJsonTranslator::m_apiTimeOutMs,
		"m_checkQuota", &NormalJsonTranslator::m_checkQuota,
		"m_smartRetry", &NormalJsonTranslator::m_smartRetry,
		"m_retransAllWhenFail", &NormalJsonTranslator::m_retransAllWhenFail,
		"m_usePreDictInName", &NormalJsonTranslator::m_usePreDictInName,
		"m_usePostDictInName", &NormalJsonTranslator::m_usePostDictInName,
		"m_usePreDictInMsg", &NormalJsonTranslator::m_usePreDictInMsg,
		"m_usePostDictInMsg", &NormalJsonTranslator::m_usePostDictInMsg,
		"m_useGptDictToReplaceName", &NormalJsonTranslator::m_useGptDictToReplaceName,
		"m_outputWithSrc", &NormalJsonTranslator::m_outputWithSrc,
		"m_agentEnabled", &NormalJsonTranslator::m_agentEnabled,
		"m_reuseRepeatedBlocks", &NormalJsonTranslator::m_reuseRepeatedBlocks,
		"m_useRepeatedBlockInputCache", &NormalJsonTranslator::m_useRepeatedBlockInputCache,
		"m_apiStrategy", &NormalJsonTranslator::m_apiStrategy,
		"m_sortMethod", &NormalJsonTranslator::m_sortMethod,
		"m_splitFile", &NormalJsonTranslator::m_splitFile,
		"m_splitFileNum", &NormalJsonTranslator::m_splitFileNum,
		"m_repeatedBlockMinSize", &NormalJsonTranslator::m_repeatedBlockMinSize,
		"m_cacheSearchDistance", &NormalJsonTranslator::m_cacheSearchDistance,
		"m_linebreakSymbol", &NormalJsonTranslator::m_linebreakSymbol,
		"m_agentMaxTurnsPerChunk", &NormalJsonTranslator::m_agentMaxTurnsPerChunk,
		"m_agentSoftContextChars", &NormalJsonTranslator::m_agentSoftContextChars,
		"m_agentHardContextChars", &NormalJsonTranslator::m_agentHardContextChars,
		"m_agentSearchResultLimit", &NormalJsonTranslator::m_agentSearchResultLimit,
		"m_needsCombining", &NormalJsonTranslator::m_needsCombining,
		"m_splitFilePartsToJson", NESTED_CVT(NormalJsonTranslator, m_splitFilePartsToJson),
		"m_jsonToSplitFileParts", NESTED_CVT(NormalJsonTranslator, m_jsonToSplitFileParts),
		"m_agentKnownRelFiles", &NormalJsonTranslator::m_agentKnownRelFiles,
		"m_agentDictionaryPaths", &NormalJsonTranslator::m_agentDictionaryPaths,
		"m_agentProjectInfoPath", &NormalJsonTranslator::m_agentProjectInfoPath,
		"m_nameMap", NESTED_CVT(NormalJsonTranslator, m_nameMap),
		"m_currentRunRelFilePaths", &NormalJsonTranslator::m_currentRunRelFilePaths,
		"m_onFileProcessed", &NormalJsonTranslator::m_onFileProcessed,
		"m_onPerformApi", &NormalJsonTranslator::m_onPerformApi,
		"m_onDictProcessed", &NormalJsonTranslator::m_onDictProcessed,
		"m_threadPool", &NormalJsonTranslator::m_threadPool,
		"preProcess", &NormalJsonTranslator::preProcess,
		"postProcess", &NormalJsonTranslator::postProcess,
		"processFile", &NormalJsonTranslator::processFile,
		"shouldReportRuntimeWorkbench", &NormalJsonTranslator::shouldReportRuntimeWorkbench,
		"applyAgentRetranslateSuggestions", &NormalJsonTranslator::applyAgentRetranslateSuggestions,
		"resolveRepeatedBlockReferences", &NormalJsonTranslator::resolveRepeatedBlockReferences,
		"normalJsonInit", &NormalJsonTranslator::normalJsonInit,
		"normalJsonBeforeRun", &NormalJsonTranslator::normalJsonBeforeRun,
		"normalJsonProcessFiles", sol::overload(
			&NormalJsonTranslator::normalJsonProcessFiles,
			[](NormalJsonTranslator& self, const std::vector<std::string>& relFilePaths)
			{
				std::vector<fs::path> convertedRelFilePaths;
				convertedRelFilePaths.reserve(relFilePaths.size());
				for (const auto& relFilePath : relFilePaths) {
					convertedRelFilePaths.emplace_back(ascii2Wide(relFilePath));
				}
				self.normalJsonProcessFiles(convertedRelFilePaths);
			}),
		"normalJsonProcess", &NormalJsonTranslator::normalJsonProcess,
		"normalJsonAfterRun", &NormalJsonTranslator::normalJsonAfterRun,
		"normalJsonRun", [](NormalJsonTranslator& self) { self.NormalJsonTranslator::run(); }
	);

	lua.new_usertype<EpubTextNodeInfo>("EpubTextNodeInfo",
		sol::constructors<EpubTextNodeInfo()>(),
		"offset", &EpubTextNodeInfo::offset,
		"length", &EpubTextNodeInfo::length
	);
	lua.new_usertype<JsonInfo>("JsonInfo",
		sol::constructors<JsonInfo()>(),
		"metadata", &JsonInfo::metadata,
		"htmlPath", &JsonInfo::htmlPath,
		"epubPath", &JsonInfo::epubPath,
		"normalPostPath", &JsonInfo::normalPostPath,
		"content", &JsonInfo::content
	);
	lua.new_usertype<EpubTranslator>("EpubTranslator",
		sol::base_classes, sol::bases<ITranslator, NormalJsonTranslator>(),
		"m_epubInputDir", &EpubTranslator::m_epubInputDir,
		"m_epubOutputDir", &EpubTranslator::m_epubOutputDir,
		"m_tempUnpackDir", &EpubTranslator::m_tempUnpackDir,
		"m_tempRebuildDir", &EpubTranslator::m_tempRebuildDir,
		"m_bilingualOutput", &EpubTranslator::m_bilingualOutput,
		"m_originalTextColor", &EpubTranslator::m_originalTextColor,
		"m_originalTextScale", &EpubTranslator::m_originalTextScale,
		"m_jsonToInfoMap", NESTED_CVT(EpubTranslator, m_jsonToInfoMap),
		"m_epubToJsonsMap", NESTED_CVT(EpubTranslator, m_epubToJsonsMap),
		"epubInit", &EpubTranslator::epubInit,
		"epubBeforeRun", &EpubTranslator::epubBeforeRun,
		"epubRun", [](EpubTranslator& self) { self.EpubTranslator::run(); }
	);

	lua.new_usertype<PDFTranslator>("PDFTranslator",
		sol::base_classes, sol::bases<ITranslator, NormalJsonTranslator>(),
		"m_pdfInputDir", &PDFTranslator::m_pdfInputDir,
		"m_pdfOutputDir", &PDFTranslator::m_pdfOutputDir,
		"m_bilingualOutput", &PDFTranslator::m_bilingualOutput,
		"m_jsonToPDFPathMap", NESTED_CVT(PDFTranslator, m_jsonToPDFPathMap),
		"pdfInit", &PDFTranslator::pdfInit,
		"pdfBeforeRun", &PDFTranslator::pdfBeforeRun,
		"pdfRun", [](PDFTranslator& self) { self.PDFTranslator::run(); }
	);
	

	// 绑定 utils 库
	sol::table utilsTable = lua.create_named_table("utils");
	utilsTable["executeCommand"] = [](const std::string& program, const std::string& args, std::optional<bool> showWindow, std::optional<int> timeDelayAfterCommand) -> bool
		{
			return executeCommand(ascii2Wide(program), ascii2Wide(args), showWindow.value_or(true), timeDelayAfterCommand.value_or(5));
		};
	utilsTable["getConsoleWidth"] = &getConsoleWidth;
	utilsTable["createParent"] = [](const std::string& path) -> bool
		{
			return createParent(ascii2Wide(path));
		};
	utilsTable["splitString"] = [](const std::string& str, const std::string& delimiter) -> std::vector<std::string>
		{
			return splitString(str, delimiter);
		};
	utilsTable["isSameExtension"] = [](const std::string& path, const std::string& ext) -> bool
		{
			return isSameExtension(ascii2Wide(path), ascii2Wide(ext));
		};
	utilsTable["removePunctuation"] = &removePunctuation;
	utilsTable["removeWhitespace"] = &removeWhitespace;
	utilsTable["getMostCommonChar"] = [](const std::string& str) -> std::tuple<std::string, int>
		{
			auto [charString, count] = getMostCommonChar(str);
			return std::make_tuple(charString, count);
		};
	utilsTable["splitIntoTokens"] = &::splitIntoTokens;
	utilsTable["splitIntoGraphemes"] = &splitIntoGraphemes;
	utilsTable["countGraphemes"] = [](const std::string& str) -> size_t
		{
			return countGraphemes(str);
		};
	utilsTable["countSubstring"] = &countSubstring;
	utilsTable["getSubstringPositions"] = &getSubstringPositions;
	utilsTable["replaceStr"] = [](const std::string& str, const std::string& org, const std::string& rep) -> std::string
		{
			std::string result = str;
			return replaceStrInplace(result, org, rep);
		};
	utilsTable["extractKatakana"] = &extractKatakana;
	utilsTable["extractKana"] = &extractKana;
	utilsTable["extractLatin"] = &extractLatin;
	utilsTable["extractHangul"] = &extractHangul;
	utilsTable["extractCJK"] = &extractCJK;
	utilsTable["getTraditionalChineseExtractor"] = &getTraditionalChineseExtractor;
	utilsTable["isApiTranslationEngine"] = &isApiTranslationEngine;
	utilsTable["extractZip"] = [](const std::string& zipPath, const std::string& outputDir)
		{
			extractZip(ascii2Wide(zipPath), ascii2Wide(outputDir));
		};
	utilsTable["extractFileFromZip"] = [](const std::string& zipPath, const std::string& outputDir, const std::string& fileName)
		{
			extractFileFromZip(ascii2Wide(zipPath), ascii2Wide(outputDir), fileName);
		};
	utilsTable["extractFilesFromZip"] = [](const std::string& zipPath, const std::string& outputDir, std::vector<std::string> fileNames)
		{
			const std::set<std::string> fileNameSet(fileNames.begin(), fileNames.end());
			extractZipExclude(ascii2Wide(zipPath), ascii2Wide(outputDir), fileNameSet);
		};
	utilsTable["extractZipInclude"] = [](const std::string& zipPath, const std::string& outputDir, std::vector<std::string> includePrefixes)
		{
			const std::set<std::string> includeSet(includePrefixes.begin(), includePrefixes.end());
			extractZipInclude(ascii2Wide(zipPath), ascii2Wide(outputDir), includeSet);
		};
	utilsTable["extractZipExclude"] = [](const std::string& zipPath, const std::string& outputDir, std::vector<std::string> excludePrefixes)
		{
			const std::set<std::string> excludeSet(excludePrefixes.begin(), excludePrefixes.end());
			extractZipExclude(ascii2Wide(zipPath), ascii2Wide(outputDir), excludeSet);
		};
	utilsTable["pcre2RegexSearch1"] = [](const std::string& str, const std::string& pattern, std::optional<std::string> modifier)
			-> std::vector<std::vector<std::string>>
		{
			jpc::Regex re(pattern, modifier.value_or(defaultRegCompileModifier));
			jpc::RegexMatch rm(&re);
			jpc::VecNum vecNum;
			rm.setModifier("g").setSubject(&str).setNumberedSubstringVector(&vecNum).match();
			return vecNum;
		};
	utilsTable["pcre2RegexSearch2"] = [](const std::string& str, const std::string& pattern, std::optional<std::string> modifier)
		-> std::vector<std::map<std::string, std::string>>
		{
			jpc::Regex re(pattern, modifier.value_or(defaultRegCompileModifier));
			jpc::RegexMatch rm(&re);
			jpc::VecNas vecNas;
			rm.setModifier("g").setSubject(&str).setNamedSubstringVector(&vecNas).match();
			return vecNas;
		};
	utilsTable["pcre2RegexReplace"] = [](const std::string& str, const std::string& pattern, const std::string& rep,
			std::optional<std::string> compileModifier, std::optional<std::string> replaceModifier) -> std::string
		{
			jpc::Regex re(pattern, compileModifier.value_or(defaultRegCompileModifier));
			jpc::RegexReplace rr(&re);
			return rr.setModifier(replaceModifier.value_or(defaultRegReplaceModifier)).setSubject(str).replace();
		};


	// 绑定 spdlog::logger
	lua.new_enum("spdlogLevel",
		"trace", spdlog::level::trace,
		"debug", spdlog::level::debug,
		"info", spdlog::level::info,
		"warn", spdlog::level::warn,
		"err", spdlog::level::err,
		"critical", spdlog::level::critical
	);
	lua.new_usertype<spdlog::logger>(
		"spdlogLogger",
		sol::no_constructor,
		"name", &spdlog::logger::name,
		"level", &spdlog::logger::level,
		"set_level", &spdlog::logger::set_level,
		"set_pattern", [](spdlog::logger& logger, const std::string& pattern) { logger.set_pattern(pattern); },
		"flush", &spdlog::logger::flush,
		"trace", [](spdlog::logger& logger, const std::string& msg) { logger.trace(msg); },
		"debug", [](spdlog::logger& logger, const std::string& msg) { logger.debug(msg); },
		"info", [](spdlog::logger& logger, const std::string& msg) { logger.info(msg); },
		"warn", [](spdlog::logger& logger, const std::string& msg) { logger.warn(msg); },
		"error", [](spdlog::logger& logger, const std::string& msg) { logger.error(msg); },
		"critical", [](spdlog::logger& logger, const std::string& msg) { logger.critical(msg); }
	);
	utilsTable["logger"] = m_logger;

	auto supplyTokenizerFunc = [&](const std::string& langMode)
		{
			if (lua[langMode + "useTokenizer"].get_or(false)) {
				const std::string tokenizerBackend = lua[langMode + "tokenizerBackend"].get<std::string>();
				if (tokenizerBackend == "MeCab") {
					const std::string mecabDictDir = lua[langMode + "mecabDictDir"].get<std::string>();
					m_logger->info(gppTr("LuaManager.registerCustomTypes", "%1 已配置 MeCab 分词器，首次使用时加载。")
					    .arg(scriptPath)
					    .toStdString());
					utilsTable[langMode + "tokenizeFunc"] = getMeCabTokenizeFunc(mecabDictDir, m_logger);
				}
				else if (tokenizerBackend == "spaCy") {
					const std::string spaCyModelName = lua[langMode + "spaCyModelName"].get<std::string>();
					m_logger->info(gppTr("LuaManager.registerCustomTypes", "%1 已配置 spaCy 分词器，首次使用时加载。")
					    .arg(scriptPath)
					    .toStdString());
					utilsTable[langMode + "tokenizeFunc"] = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger);
				}
				else if (tokenizerBackend == "Stanza") {
					const std::string stanzaLang = lua[langMode + "stanzaLang"].get<std::string>();
					m_logger->info(gppTr("LuaManager.registerCustomTypes", "%1 已配置 Stanza 分词器，首次使用时加载。")
					    .arg(scriptPath)
					    .toStdString());
					utilsTable[langMode + "tokenizeFunc"] = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger);
				}
				else if (tokenizerBackend == "pkuseg") {
					m_logger->info(gppTr("LuaManager.registerCustomTypes", "%1 已配置 pkuseg 分词器，首次使用时加载。")
					    .arg(scriptPath)
					    .toStdString());
					utilsTable[langMode + "tokenizeFunc"] = getNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg" }, "tokenizer_pkuseg", "default", m_logger);
				}
				else {
					throw std::invalid_argument(gppTr(
					    "LuaManager.registerCustomTypes",
					    "%1 中注册了无效的 tokenizerBackend: %2")
					    .arg(scriptPath)
					    .arg(tokenizerBackend)
					    .toStdString());
				}
			}
		};
	supplyTokenizerFunc("sourceLang_");
	supplyTokenizerFunc("targetLang_");

}
