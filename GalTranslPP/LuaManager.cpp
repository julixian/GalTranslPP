module;

#define SOL2_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>
#include <ctpl_stl.h>

module LuaManager;

import NormalJsonTranslator;
import EpubTranslator;
import PDFTranslator;
import NLPTool;

import ITranslator;
import Tool;

namespace fs = std::filesystem;

LuaStateInstance::LuaStateInstance()
	: m_daemonThread(&LuaStateInstance::daemonThreadFunc, this)
{}

LuaStateInstance::~LuaStateInstance()
{
	auto functionClearTaskFunc = [this]()
		{
			this->m_functions.clear();
		};
	this->submitTask(std::move(functionClearTaskFunc)).get();
	m_taskQueue.stop();
	if (m_daemonThread.joinable()) {
		m_daemonThread.join();
	}
	m_lua.reset();
}

std::future<void> LuaStateInstance::submitTask(std::function<void()> taskFunc)
{
	auto task = std::make_unique<LuaTask>();
	task->taskFunc = std::move(taskFunc);
	auto future = task->promise.get_future();
	m_taskQueue.push(std::move(task));
	return future;
}

void LuaStateInstance::daemonThreadFunc()
{
	m_lua = std::make_unique<sol::state>();
	m_lua->open_libraries();
	while (true) {
		const auto taskOpt = m_taskQueue.pop();
		if (!taskOpt) {
			break;
		}
		const std::unique_ptr<LuaTask>& task = taskOpt.value();
		try {
			task->taskFunc();
			task->promise.set_value();
		}
		catch (const sol::error& e) {
			task->promise.set_exception(std::make_exception_ptr(std::runtime_error(e.what())));
		}
		catch (...) {
			task->promise.set_exception(std::current_exception());
		}
	}
}

class LuaJson {
public:
	static json solObj2JsonValue(sol::object obj)
	{
		switch (obj.get_type())
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
		case sol::type::table:
		{
			sol::table luaTable = obj.as<sol::table>();
			bool arrayLike = true;
			if (!luaTable.empty()) {
				size_t expectedKey = 1;
				for (auto& kv : luaTable) {
					if (!kv.first.is<lua_Integer>() || kv.first.as<lua_Integer>() != expectedKey) {
						arrayLike = false;
						break;
					}
					++expectedKey;
				}
				arrayLike &= luaTable.size() == expectedKey - 1;
			}

			if (arrayLike) {
				json arr = json::array();
				for (size_t i = 1; i <= luaTable.size(); ++i) {
					arr.push_back(solObj2JsonValue(luaTable.get<sol::object>(i)));
				}
				return arr;
			}

			json tbl = json::object();
			for (auto& kv : luaTable) {
				if (!kv.first.is<std::string>()) {
					throw std::runtime_error(gppTr("LuaJson.solObj2JsonValue", "LuaJson: key 必须是字符串")
						.toStdString());
				}
				tbl[kv.first.as<std::string>()] = solObj2JsonValue(kv.second);
			}
			return tbl;
		}
		case sol::type::nil:
			return nullptr;
		default:
			return "LuaJson: unsupported type";
		}
	}

	static sol::object jsonValue2SolObject(const json& value, sol::state_view lua)
	{
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
		case json::value_t::array:
		{
			sol::table resultArray = lua.create_table();
			for (const auto& elem : value) {
				resultArray.add(jsonValue2SolObject(elem, lua));
			}
			return sol::make_object(lua, resultArray);
		}
		case json::value_t::object:
		{
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
	static toml::value solObj2TomlValue(sol::object obj)
	{
		switch (obj.get_type())
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
		case sol::type::table:
		{
			sol::table luaTable = obj.as<sol::table>();
			bool arrayLike = true;
			if (!luaTable.empty()) {
				size_t expectedKey = 1;
				for (auto& kv : luaTable) {
					if (!kv.first.is<lua_Integer>() || kv.first.as<lua_Integer>() != expectedKey) {
						arrayLike = false;
						break;
					}
					++expectedKey;
				}
				arrayLike &= luaTable.size() == expectedKey - 1;
			}

			if (arrayLike) {
				toml::array arr;
				for (size_t i = 1; i <= luaTable.size(); ++i) {
					arr.push_back(solObj2TomlValue(luaTable.get<sol::object>(i)));
				}
				return arr;
			}

			toml::table tbl;
			for (auto& kv : luaTable) {
				if (!kv.first.is<std::string>()) {
					throw std::runtime_error(gppTr("LuaToml.solObj2TomlValue", "LuaToml: key 必须是字符串")
						.toStdString());
				}
				tbl.insert({ kv.first.as<std::string>(), solObj2TomlValue(kv.second) });
			}
			return tbl;
		}
		default:
			return toml::value{ "LuaToml: unsupported type" };
		}
	}

	static sol::object tomlValue2SolObject(const toml::value& value, sol::state_view lua)
	{
		if (value.is_table()) {
			sol::table resultMap = lua.create_table();
			for (const auto& [key, val] : value.as_table()) {
				resultMap[key] = tomlValue2SolObject(val, lua);
			}
			return resultMap;
		}
		if (value.is_array()) {
			sol::table resultVec = lua.create_table();
			for (const auto& elem : value.as_array()) {
				resultVec.add(tomlValue2SolObject(elem, lua));
			}
			return resultVec;
		}
		if (value.is_string()) {
			return sol::make_object(lua, value.as_string());
		}
		if (value.is_integer()) {
			return sol::make_object(lua, value.as_integer());
		}
		if (value.is_floating()) {
			return sol::make_object(lua, value.as_floating());
		}
		if (value.is_boolean()) {
			return sol::make_object(lua, value.as_boolean());
		}
		return sol::make_object(lua, sol::nil);
	}
};

std::optional<std::shared_ptr<LuaStateInstance>> LuaManager::registerFunction(const std::string& scriptPath, const std::string& functionName)
{
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
			state->submitTask([this, state, stdScriptPath, scriptPath]()
				{
					std::ifstream ifs(stdScriptPath, std::ios::binary);
					std::string script((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
					const std::string chunkName = "@" + wide2Ascii(stdScriptPath);
					const sol::protected_function_result result = state->m_lua->script(script, chunkName);
					if (!result.valid()) {
						const sol::error error = result;
						throw std::runtime_error(error.what());
					}
					registerCustomTypes(state, scriptPath);
				}).get();
			it = m_scriptStates.find(stdScriptPath);
		}
		catch (const std::exception& e) {
			m_logger->error(gppTr("LuaManager.registerFunction", "加载脚本 [%1] 失败: %2")
				.arg(scriptPath)
				.arg(e.what())
				.toStdString());
			m_scriptStates.erase(stdScriptPath);
			return std::nullopt;
		}
	}

	if (functionName.empty()) {
		return it->second;
	}

	if (!it->second->m_functions.contains(functionName)) {
		bool success = false;
		std::shared_ptr<LuaStateInstance> luaState = it->second;
		luaState->submitTask([luaState, functionName, &success]()
			{
				auto pFunc = std::make_unique<sol::function>((*(luaState->m_lua))[functionName]);
				if (!pFunc->valid()) {
					return;
				}
				luaState->m_functions.insert({ functionName, std::move(pFunc) });
				success = true;
			}).get();
		if (!success) {
			m_logger->debug(gppTr("LuaManager.registerFunction", "在脚本 [%1] 中未找到函数 %2")
				.arg(scriptPath)
				.arg(functionName)
				.toStdString());
			return std::nullopt;
		}
	}

	return it->second;
}

void LuaManager::registerCustomTypes(const std::shared_ptr<LuaStateInstance>& luaStateInstance, const std::string& scriptPath)
{
	sol::state& lua = *(luaStateInstance->m_lua);

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
		"Sakura", TransEngine::Sakura,
		"DumpName", TransEngine::DumpName,
		"NameTrans", TransEngine::NameTrans,
		"GenDict", TransEngine::GenDict,
		"Rebuild", TransEngine::Rebuild,
		"ShowNormal", TransEngine::ShowNormal
	);

	lua.new_enum("CachePart",
		"None", CachePart::None,
		"Name", CachePart::Name,
		"NameTrans", CachePart::NameTrans,
		"Names", CachePart::Names,
		"NamesTrans", CachePart::NamesTrans,
		"Orig", CachePart::Orig,
		"Preproc", CachePart::Preproc,
		"Pretrans", CachePart::Pretrans,
		"Problems", CachePart::Problems,
		"OtherInfo", CachePart::OtherInfo,
		"TranslatedBy", CachePart::TranslatedBy,
		"Transview", CachePart::Transview
	);

	lua.new_enum("ApiProtocol",
		"OpenAI", ApiProtocol::OpenAI,
		"Claude", ApiProtocol::Claude,
		"Gemini", ApiProtocol::Gemini
	);

	lua.new_usertype<SentencePosition>("SentencePosition",
		sol::constructors<SentencePosition()>(),
		"file", &SentencePosition::file,
		"index", &SentencePosition::index
	);

	lua.new_usertype<Sentence>("Sentence",
		sol::constructors<Sentence()>(),
		"index", &Sentence::index,
		"fileName", &Sentence::fileName,
		"name", &Sentence::name,
		"names", &Sentence::names,
		"nameTrans", &Sentence::nameTrans,
		"namesTrans", &Sentence::namesTrans,
		"orig", &Sentence::orig,
		"preproc", &Sentence::preproc,
		"pretrans", &Sentence::pretrans,
		"problems", &Sentence::problems,
		"translatedBy", &Sentence::translatedBy,
		"transview", &Sentence::transview,
		"linebreak", &Sentence::linebreak,
		"otherInfo", NESTED_CVT(Sentence, otherInfo),
		"ref", &Sentence::ref,
		"refBy", &Sentence::refBy,
		"nameType", &Sentence::nameType,
		"prev", &Sentence::prev,
		"next", &Sentence::next,
		"transCompleted", &Sentence::transCompleted,
		"problemAnalyzeDisabled", &Sentence::problemAnalyzeDisabled,
		"isRefPending", &Sentence::isRefPending,
		"getProblemByIndex", &Sentence::getProblemByIndex,
		"setProblemByIndex", &Sentence::setProblemByIndex
	);

	lua.new_usertype<fs::path>("Path",
		sol::meta_function::construct,
		sol::factories(
			[](const std::string& str) { return fs::path(ascii2Wide(str)); },
			[]() { return fs::path(); },
			[](const fs::path& p) { return fs::path(p); }),
		"value", sol::property(
			[](const fs::path& self) { return wide2Ascii(self); },
			[](fs::path& self, const std::string& str) { self = ascii2Wide(str); }),
		sol::meta_function::to_string,
		[](const fs::path& self) { return wide2Ascii(self); },
		sol::meta_function::division, sol::overload(
			[](const fs::path& self, const fs::path& other) { return self / other; },
			[](const fs::path& self, const std::string& other) { return self / ascii2Wide(other); },
			[](const std::string& self, const fs::path& other) { return ascii2Wide(self) / other; }),
		sol::meta_function::equal_to,
		[](const fs::path& self, const fs::path& other) { return self == other; },
		"filename", sol::property([](const fs::path& self) { return self.filename(); }),
		"stem", sol::property([](const fs::path& self) { return self.stem(); }),
		"extension", sol::property([](const fs::path& self) { return self.extension(); }),
		"parentPath", sol::property([](const fs::path& self) { return self.parent_path(); }),
		"empty", sol::property([](const fs::path& self) { return self.empty(); }),
		"isAbsolute", sol::property([](const fs::path& self) { return self.is_absolute(); }),
		"isRelative", sol::property([](const fs::path& self) { return self.is_relative(); }),
		"equivalent", [](const fs::path& self, const fs::path& other) { return fs::equivalent(self, other); },
		"weaklyCanonical", [](const fs::path& self) { return fs::weakly_canonical(self); },
		"canonical", [](const fs::path& self) { return fs::canonical(self); },
		"relativeTo", [](const fs::path& self, const fs::path& base) { return fs::relative(self, base); }
	);

	lua.new_enum("LogLevel",
		"trace", spdlog::level::trace,
		"debug", spdlog::level::debug,
		"info", spdlog::level::info,
		"warn", spdlog::level::warn,
		"err", spdlog::level::err,
		"critical", spdlog::level::critical
	);

	lua.new_usertype<spdlog::logger>("spdlogLogger",
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

	sol::table luaTomlTable = lua.create_named_table("toml");
	luaTomlTable["parse"] = [](const fs::path& path, sol::this_state s) -> std::tuple<sol::object, std::optional<std::string>>
		{
			sol::state_view lua = s;
			try {
				return { LuaToml::tomlValue2SolObject(toml::uparse(path), lua), std::nullopt };
			}
			catch (const std::exception& e) {
				return { sol::make_object(lua, sol::nil), std::string(e.what()) };
			}
		};
	luaTomlTable["str"] = [](sol::object obj) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
		{
			try {
				return { toml::format(LuaToml::solObj2TomlValue(obj)), std::nullopt };
			}
			catch (const std::exception& e) {
				return { std::nullopt, std::string(e.what()) };
			}
		};
	luaTomlTable["save"] = [](const fs::path& path, sol::object obj) -> std::tuple<bool, std::optional<std::string>>
		{
			try {
				atomicOutputFile(path, toml::format(LuaToml::solObj2TomlValue(obj)));
				return { true, std::nullopt };
			}
			catch (const std::exception& e) {
				return { false, std::string(e.what()) };
			}
		};

	sol::table luaJsonTable = lua.create_named_table("json");
	luaJsonTable["parse"] = [](const fs::path& path, sol::this_state s) -> std::tuple<sol::object, std::optional<std::string>>
		{
			sol::state_view lua = s;
			try {
				return { LuaJson::jsonValue2SolObject(parseJson(path), lua), std::nullopt };
			}
			catch (const std::exception& e) {
				return { sol::make_object(lua, sol::nil), std::string(e.what()) };
			}
		};
	luaJsonTable["save"] = [](const fs::path& path, sol::object obj, sol::optional<int> indent) -> std::tuple<bool, std::optional<std::string>>
		{
			try {
				const json value = LuaJson::solObj2JsonValue(obj);
				atomicOutputFile(path, value.dump(indent.value_or(2)));
				return { true, std::nullopt };
			}
			catch (const std::exception& e) {
				return { false, std::string(e.what()) };
			}
		};

	lua.new_usertype<RuntimeTransSuccessEvent>("RuntimeTransSuccessEvent",
		sol::constructors<RuntimeTransSuccessEvent()>(),
		"timestamp", &RuntimeTransSuccessEvent::timestamp,
		"filename", &RuntimeTransSuccessEvent::filename,
		"index", &RuntimeTransSuccessEvent::index,
		"speakers", &RuntimeTransSuccessEvent::speakers,
		"sourcePreview", &RuntimeTransSuccessEvent::sourcePreview,
		"translationPreview", &RuntimeTransSuccessEvent::translationPreview,
		"translatedBy", &RuntimeTransSuccessEvent::translatedBy
	);

	lua.new_usertype<RuntimeTransErrorEvent>("RuntimeTransErrorEvent",
		sol::constructors<RuntimeTransErrorEvent()>(),
		"timestamp", &RuntimeTransErrorEvent::timestamp,
		"kind", &RuntimeTransErrorEvent::kind,
		"level", &RuntimeTransErrorEvent::level,
		"message", &RuntimeTransErrorEvent::message,
		"filename", &RuntimeTransErrorEvent::filename,
		"indexRange", &RuntimeTransErrorEvent::indexRange,
		"requestCount", &RuntimeTransErrorEvent::requestCount,
		"model", &RuntimeTransErrorEvent::model,
		"sleepSeconds", &RuntimeTransErrorEvent::sleepSeconds
	);

	lua.new_usertype<RuntimeFileProgress>("RuntimeFileProgress",
		sol::constructors<RuntimeFileProgress()>(),
		"filename", &RuntimeFileProgress::filename,
		"total", &RuntimeFileProgress::total,
		"completed", &RuntimeFileProgress::completed,
		"problems", &RuntimeFileProgress::problems
	);

	lua.new_usertype<IController>("IController",
		sol::no_constructor,
		"m_totalSentences", sol::property([](IController& self) { return self.m_totalSentences.load(); },
			[](IController& self, int value) { self.m_totalSentences = value; }),
		"m_completedSentences", sol::property([](IController& self) { return self.m_completedSentences.load(); },
			[](IController& self, int value) { self.m_completedSentences = value; }),
		"m_activeThreads", sol::property([](IController& self) { return self.m_activeThreads.load(); },
			[](IController& self, int value) { self.m_activeThreads = value; }),
		"m_totalThreads", sol::property([](IController& self) { return self.m_totalThreads.load(); },
			[](IController& self, int value) { self.m_totalThreads = value; }),
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
		"recordRuntimeTransSuccess", &IController::recordRuntimeTransSuccess,
		"recordRuntimeTransError", &IController::recordRuntimeTransError,
		"shouldStop", &IController::shouldStop,
		"flush", &IController::flush
	);

	lua.new_usertype<ITranslator>("ITranslator",
		sol::no_constructor,
		"run", &ITranslator::run
	);

	lua.new_usertype<ctpl::thread_pool>("ThreadPool",
		sol::no_constructor,
		"resize", &ctpl::thread_pool::resize,
		"size", &ctpl::thread_pool::size
	);

	lua.new_usertype<ApiPool>("ApiPool",
		sol::no_constructor,
		"resortTokens", &ApiPool::resortTokens,
		"isEmpty", &ApiPool::isEmpty,
		"size", &ApiPool::size
	);

	lua.new_usertype<GptDictionary>("GptDictionary",
		sol::no_constructor,
		"sort", &GptDictionary::sort,
		"loadFromFile", &GptDictionary::loadFromFile
	);

	lua.new_usertype<NormalDictionary>("NormalDictionary",
		sol::no_constructor,
		"sort", &NormalDictionary::sort,
		"loadFromFile", &NormalDictionary::loadFromFile
	);

	lua.new_usertype<ProblemCompareObj>("ProblemCompareObj",
		sol::constructors<ProblemCompareObj()>(),
		"use", &ProblemCompareObj::use,
		"base", &ProblemCompareObj::base,
		"check", &ProblemCompareObj::check
	);

	lua.new_usertype<Problems>("Problems",
		sol::constructors<Problems()>(),
		"highFrequency", &Problems::highFrequency,
		"punctsMiss", &Problems::punctsMiss,
		"remainJp", &Problems::remainJp,
		"introLatin", &Problems::introLatin,
		"introHangul", &Problems::introHangul,
		"introTraditionalChinese", &Problems::introTraditionalChinese,
		"linebreakLost", &Problems::linebreakLost,
		"linebreakAdded", &Problems::linebreakAdded,
		"longer", &Problems::longer,
		"strictlyLonger", &Problems::strictlyLonger,
		"dictUnused", &Problems::dictUnused,
		"notTargetLang", &Problems::notTargetLang,
		"invalidChar", &Problems::invalidChar
	);

	lua.new_usertype<ProblemAnalyzer>("ProblemAnalyzer",
		sol::no_constructor,
		"setProblemRule", &ProblemAnalyzer::setProblemRule,
		"analyze", [](ProblemAnalyzer& self, Sentence& sentence) { self.analyze(&sentence); }
	);

	lua.new_usertype<NameTranslator>("NameTranslator",
		sol::no_constructor,
		"run", &NameTranslator::run
	);

	lua.new_usertype<DictionaryGenerator>("DictionaryGenerator",
		sol::no_constructor,
		"generate", &DictionaryGenerator::generate
	);

	lua.new_usertype<NormalJsonTranslatorTransAgent>("NormalJsonTranslatorTransAgent",
		sol::no_constructor,
		"applyAgentSuggestions", &NormalJsonTranslatorTransAgent::applyAgentSuggestions
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
		"m_nameTablePath", &NormalJsonTranslator::m_nameTablePath,
		"m_rollingContextCachePath", &NormalJsonTranslator::m_rollingContextCachePath,
		"m_projectDir", &NormalJsonTranslator::m_projectDir,
		"m_agentRootDir", &NormalJsonTranslator::m_agentRootDir,
		"m_agentTermLedgerPath", &NormalJsonTranslator::m_agentTermLedgerPath,
		"m_agentFileNotesDir", &NormalJsonTranslator::m_agentFileNotesDir,
		"m_rollingContextCacheMap", sol::property(
			[](NormalJsonTranslator& self)
			{
				std::shared_lock lock(self.m_rollingContextCacheMapMutex);
				return self.m_rollingContextCacheMap;
			},
			[](NormalJsonTranslator& self, decltype(NormalJsonTranslator::m_rollingContextCacheMap) value)
			{
				std::unique_lock lock(self.m_rollingContextCacheMapMutex);
				self.m_rollingContextCacheMap = std::move(value);
			}),
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
		"m_inputBlockMaxLines", &NormalJsonTranslator::m_inputBlockMaxLines,
		"m_problemMaxLines", &NormalJsonTranslator::m_problemMaxLines,
		"m_glossaryMaxLines", &NormalJsonTranslator::m_glossaryMaxLines,
		"m_maxRequestCount", &NormalJsonTranslator::m_maxRequestCount,
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
		"m_apiStrategy", &NormalJsonTranslator::m_apiStrategy,
		"m_sortMethod", &NormalJsonTranslator::m_sortMethod,
		"m_splitFileMethod", &NormalJsonTranslator::m_splitFileMethod,
		"m_problemOverviewFormat", &NormalJsonTranslator::m_problemOverviewFormat,
		"m_splitFileNum", &NormalJsonTranslator::m_splitFileNum,
		"m_repeatedBlockMinSize", &NormalJsonTranslator::m_repeatedBlockMinSize,
		"m_cacheSearchDistance", &NormalJsonTranslator::m_cacheSearchDistance,
		"m_linebreakSymbol", &NormalJsonTranslator::m_linebreakSymbol,
		"m_agentMaxTurnsPerChunk", &NormalJsonTranslator::m_agentMaxTurnsPerChunk,
		"m_agentCompactContextThresholdBytes", &NormalJsonTranslator::m_agentCompactContextThresholdBytes,
		"m_agentSearchResultLimit", &NormalJsonTranslator::m_agentSearchResultLimit,
		"m_agentContextLinesLimit", &NormalJsonTranslator::m_agentContextLinesLimit,
		"m_splitFileEnabled", &NormalJsonTranslator::m_splitFileEnabled,
		"m_splitFilePartsToJson", NESTED_CVT(NormalJsonTranslator, m_splitFilePartsToJson),
		"m_jsonToSplitFileParts", NESTED_CVT(NormalJsonTranslator, m_jsonToSplitFileParts),
		"m_gptDictionaryPaths", &NormalJsonTranslator::m_gptDictionaryPaths,
		"m_agentProjectNotePath", &NormalJsonTranslator::m_agentProjectNotePath,
		"m_nameMap", NESTED_CVT(NormalJsonTranslator, m_nameMap),
		"m_currentRunRelFilePaths", &NormalJsonTranslator::m_currentRunRelFilePaths,
		"m_repeatedBlockCompletedRelFilePaths", NESTED_CVT(NormalJsonTranslator, m_repeatedBlockCompletedRelFilePaths),
		"m_onFileProcessed", &NormalJsonTranslator::m_onFileProcessed,
		"m_onPerformApi", &NormalJsonTranslator::m_onPerformApi,
		"m_onDictProcessed", &NormalJsonTranslator::m_onDictProcessed,
		"m_threadPool", &NormalJsonTranslator::m_threadPool,
		"m_apiPool", sol::property([](NormalJsonTranslator& self) { return self.m_apiPool.get(); }),
		"m_gptDictionary", sol::property([](NormalJsonTranslator& self) { return self.m_gptDictionary.get(); }),
		"m_preDictionary", sol::property([](NormalJsonTranslator& self) { return self.m_preDictionary.get(); }),
		"m_postDictionary", sol::property([](NormalJsonTranslator& self) { return self.m_postDictionary.get(); }),
		"m_problemAnalyzer", sol::property([](NormalJsonTranslator& self) { return self.m_problemAnalyzer.get(); }),
		"m_nameTranslator", sol::property([](NormalJsonTranslator& self) { return self.m_nameTranslator.get(); }),
		"m_dictionaryGenerator", sol::property([](NormalJsonTranslator& self) { return self.m_dictionaryGenerator.get(); }),
		"m_transAgent", sol::property([](NormalJsonTranslator& self) { return self.m_transAgent.get(); }),
		"preProcess", &NormalJsonTranslator::preProcess,
		"postProcess", &NormalJsonTranslator::postProcess,
		"processFile", &NormalJsonTranslator::processFile,
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
		"m_babeldocLangOut", &PDFTranslator::m_babeldocLangOut,
		"m_jsonToPDFPathMap", NESTED_CVT(PDFTranslator, m_jsonToPDFPathMap),
		"pdfInit", &PDFTranslator::pdfInit,
		"pdfBeforeRun", &PDFTranslator::pdfBeforeRun,
		"pdfRun", [](PDFTranslator& self) { self.PDFTranslator::run(); }
	);

	sol::table utilsTable = lua.create_named_table("utils");
	utilsTable["splitString"] = [](const std::string& str, const std::string& delimiter) { return splitString(str, delimiter); };
	utilsTable["splitIntoTokens"] = &::splitIntoTokens;
	utilsTable["splitIntoGraphemes"] = &splitIntoGraphemes;
	utilsTable["countGraphemes"] = &countGraphemes;
	utilsTable["countSubstring"] = &countSubstring;
	utilsTable["getSubstringPositions"] = &getSubstringPositions;
	utilsTable["getMostCommonChar"] = &getMostCommonChar;
	utilsTable["replaceStr"] = [](const std::string& str, const std::string& org, const std::string& rep)
		{
			std::string result = str;
			return replaceStrInplace(result, org, rep);
		};
	utilsTable["removePunctuation"] = &removePunctuation;
	utilsTable["removeWhitespace"] = &removeWhitespace;
	utilsTable["extractKatakana"] = &extractKatakana;
	utilsTable["extractKana"] = &extractKana;
	utilsTable["extractLatin"] = &extractLatin;
	utilsTable["extractHangul"] = &extractHangul;
	utilsTable["extractCJK"] = &extractCJK;
	utilsTable["getTraditionalChineseExtractor"] = &getTraditionalChineseExtractor;
	utilsTable["isApiTranslationEngine"] = &isApiTranslationEngine;
	utilsTable["executeCommand"] = [](const std::string& program, const std::string& args, std::optional<bool> showWindow, std::optional<int> timeDelayAfterCommand)
		{
			return executeCommand(ascii2Wide(program), ascii2Wide(args), showWindow.value_or(true), timeDelayAfterCommand.value_or(5));
		};
	utilsTable["getConsoleWidth"] = &getConsoleWidth;
	utilsTable["createParent"] = [](const fs::path& path) { return createParent(path); };
	utilsTable["isSameExtension"] = [](const fs::path& path, const std::string& ext) { return isSameExtension(path, ascii2Wide(ext)); };
	utilsTable["extractZip"] = [](const fs::path& zipPath, const fs::path& outputDir) { extractZip(zipPath, outputDir); };
	utilsTable["extractFileFromZip"] = [](const fs::path& zipPath, const fs::path& outputDir, const std::string& fileName)
		{
			extractFileFromZip(zipPath, outputDir, fileName);
		};
	utilsTable["extractFilesFromZip"] = [](const fs::path& zipPath, const fs::path& outputDir, std::vector<std::string> fileNames)
		{
			extractFilesFromZip(zipPath, outputDir, std::set<std::string>(fileNames.begin(), fileNames.end()));
		};
	utilsTable["extractZipInclude"] = [](const fs::path& zipPath, const fs::path& outputDir, std::vector<std::string> includePrefixes)
		{
			extractZipInclude(zipPath, outputDir, std::set<std::string>(includePrefixes.begin(), includePrefixes.end()));
		};
	utilsTable["extractZipExclude"] = [](const fs::path& zipPath, const fs::path& outputDir, std::vector<std::string> excludePrefixes)
		{
			extractZipExclude(zipPath, outputDir, std::set<std::string>(excludePrefixes.begin(), excludePrefixes.end()));
		};
	utilsTable["pcre2RegexSearch1"] = [](const std::string& str, const std::string& pattern, std::optional<std::string> modifier) -> std::vector<std::vector<std::string>>
		{
			jpc::Regex re(pattern, modifier.value_or(defaultRegCompileModifier));
			jpc::RegexMatch rm(&re);
			jpc::VecNum vecNum;
			rm.setModifier("g").setSubject(&str).setNumberedSubstringVector(&vecNum).match();
			return vecNum;
		};
	utilsTable["pcre2RegexSearch2"] = [](const std::string& str, const std::string& pattern, std::optional<std::string> modifier) -> std::vector<std::map<std::string, std::string>>
		{
			jpc::Regex re(pattern, modifier.value_or(defaultRegCompileModifier));
			jpc::RegexMatch rm(&re);
			jpc::VecNas vecNas;
			rm.setModifier("g").setSubject(&str).setNamedSubstringVector(&vecNas).match();
			return vecNas;
		};
	utilsTable["pcre2RegexReplace"] = [](const std::string& str, const std::string& pattern, const std::string& rep,
		std::optional<std::string> compileModifier, std::optional<std::string> replaceModifier)
		{
			jpc::Regex re(pattern, compileModifier.value_or(defaultRegCompileModifier));
			jpc::RegexReplace rr(&re);
			return rr.setModifier(replaceModifier.value_or(defaultRegReplaceModifier)).setSubject(str).replace();
		};
	utilsTable["logger"] = m_logger;

	auto supplyTokenizerFunc = [&](const std::string& langMode)
		{
			const std::string useTokenizerName = langMode + "UseTokenizer";
			if (!lua[useTokenizerName].get_or(false)) {
				return;
			}

			const std::string tokenizerBackendName = langMode + "TokenizerBackend";
			const sol::optional<std::string> tokenizerBackend = lua[tokenizerBackendName];
			if (!tokenizerBackend) {
				throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
					.arg(scriptPath)
					.arg(tokenizerBackendName)
					.toStdString());
			}

			std::function<NLPResult(const std::string&)> tokenizeFunc;
			if (*tokenizerBackend == "MeCab") {
				const std::string mecabDictDirName = langMode + "MecabDictDir";
				const sol::optional<std::string> mecabDictDir = lua[mecabDictDirName];
				if (!mecabDictDir) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(mecabDictDirName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 MeCab 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getMeCabTokenizeFunc(*mecabDictDir, m_logger);
			}
			else if (*tokenizerBackend == "spaCy") {
				const std::string spaCyModelNameName = langMode + "SpaCyModelName";
				const sol::optional<std::string> spaCyModelName = lua[spaCyModelNameName];
				if (!spaCyModelName) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(spaCyModelNameName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 spaCy 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getPythonNLPTokenizeFunc({ "click", "spacy" }, "tokenizer_spacy",
					*spaCyModelName, m_logger);
			}
			else if (*tokenizerBackend == "Stanza") {
				const std::string stanzaLangName = langMode + "StanzaLang";
				const sol::optional<std::string> stanzaLang = lua[stanzaLangName];
				if (!stanzaLang) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(stanzaLangName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 Stanza 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getPythonNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza",
					*stanzaLang, m_logger);
			}
			else if (*tokenizerBackend == "pkuseg") {
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 pkuseg 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getPythonNLPTokenizeFunc({ "setuptools", "nes-py", "cython", "pkuseg" },
					"tokenizer_pkuseg", "default", m_logger);
			}
			else {
				throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes",
					"[%1] 中注册了无效的 tokenizerBackend: %2")
					.arg(scriptPath)
					.arg(*tokenizerBackend)
					.toStdString());
			}

			utilsTable[langMode + "TokenizeFunc"] = std::move(tokenizeFunc);
		};

	supplyTokenizerFunc("sourceLang");
	supplyTokenizerFunc("targetLang");
}
