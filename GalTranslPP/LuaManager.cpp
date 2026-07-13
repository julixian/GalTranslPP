module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
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

namespace luabridge
{
	template <typename K, typename V, typename Hash, typename Eq, typename Allocator>
	struct Stack<absl::flat_hash_map<K, V, Hash, Eq, Allocator>>
	{
		using Type = absl::flat_hash_map<K, V, Hash, Eq, Allocator>;

		static Result push(lua_State* lua, const Type& value)
		{
			const std::unordered_map<K, V> converted(value.begin(), value.end());
			return Stack<std::unordered_map<K, V>>::push(lua, converted);
		}

		static TypeResult<Type> get(lua_State* lua, int index)
		{
			auto converted = Stack<std::unordered_map<K, V>>::get(lua, index);
			if (!converted) {
				return converted.error();
			}
			return Type(converted->begin(), converted->end());
		}

		static bool isInstance(lua_State* lua, int index)
		{
			return Stack<std::unordered_map<K, V>>::isInstance(lua, index);
		}
	};

	template <typename K, typename Hash, typename Eq, typename Allocator>
	struct Stack<absl::flat_hash_set<K, Hash, Eq, Allocator>>
	{
		using Type = absl::flat_hash_set<K, Hash, Eq, Allocator>;

		static Result push(lua_State* lua, const Type& value)
		{
			const std::unordered_set<K> converted(value.begin(), value.end());
			return Stack<std::unordered_set<K>>::push(lua, converted);
		}

		static TypeResult<Type> get(lua_State* lua, int index)
		{
			auto converted = Stack<std::unordered_set<K>>::get(lua, index);
			if (!converted) {
				return converted.error();
			}
			return Type(converted->begin(), converted->end());
		}

		static bool isInstance(lua_State* lua, int index)
		{
			return Stack<std::unordered_set<K>>::isInstance(lua, index);
		}
	};
}

namespace lua_binding
{
	struct NoConstructor {};
	struct BaseClasses {};
	inline constexpr NoConstructor noConstructor;
	inline constexpr BaseClasses baseClasses;

	template<typename... Signatures>
	struct Constructors {};

	template<typename... Bases>
	struct BasesList {};

	template<typename... Bases>
	BasesList<Bases...> bases()
	{
		return {};
	}

	template<typename Signature>
	struct ConstructorPointer;

	template<typename Result, typename... Args>
	struct ConstructorPointer<Result(Args...)>
	{
		using type = void(*)(Args...);
	};

	template<typename Getter>
	struct ReadOnlyProperty
	{
		Getter getter;

		template<typename Registration>
		void addTo(Registration& registration, const char* name)
		{
			registration.addProperty(name, std::move(getter));
		}
	};

	template<typename Getter, typename Setter>
	struct ReadWriteProperty
	{
		Getter getter;
		Setter setter;

		template<typename Registration>
		void addTo(Registration& registration, const char* name)
		{
			registration.addProperty(name, std::move(getter), std::move(setter));
		}
	};

	template<typename Getter>
	ReadOnlyProperty<Getter> property(Getter getter)
	{
		return { std::move(getter) };
	}

	template<typename Getter, typename Setter>
	ReadWriteProperty<Getter, Setter> property(Getter getter, Setter setter)
	{
		return { std::move(getter), std::move(setter) };
	}

	template<typename... Functions>
	struct Overload
	{
		std::tuple<Functions...> functions;
	};

	template<typename... Functions>
	Overload<Functions...> overload(Functions... functions)
	{
		return { std::tuple<Functions...>{ std::move(functions)... } };
	}

	template<typename T>
	struct IsProperty : std::false_type {};

	template<typename Getter>
	struct IsProperty<ReadOnlyProperty<Getter>> : std::true_type {};

	template<typename Getter, typename Setter>
	struct IsProperty<ReadWriteProperty<Getter, Setter>> : std::true_type {};

	template<typename T>
	struct IsOverload : std::false_type {};

	template<typename... Functions>
	struct IsOverload<Overload<Functions...>> : std::true_type {};

	template<typename Registration>
	void addMembers(Registration&)
	{}

	template<typename Registration, typename Value, typename... Rest>
	void addMembers(Registration& registration, const char* name, Value value, Rest&&... rest);

	template<typename Registration, typename... Rest>
	void addMembers(Registration& registration, NoConstructor, Rest&&... rest)
	{
		addMembers(registration, std::forward<Rest>(rest)...);
	}

	template<typename Registration, typename... Signatures, typename... Rest>
	void addMembers(Registration& registration, Constructors<Signatures...>, Rest&&... rest)
	{
		registration.template addConstructor<typename ConstructorPointer<Signatures>::type...>();
		addMembers(registration, std::forward<Rest>(rest)...);
	}

	template<typename Registration, typename Value, typename... Rest>
	void addMembers(Registration& registration, const char* name, Value value, Rest&&... rest)
	{
		if constexpr (IsProperty<Value>::value) {
			value.addTo(registration, name);
		}
		else if constexpr (IsOverload<Value>::value) {
			std::apply([&](auto... functions)
				{
					registration.addFunction(name, std::move(functions)...);
				}, std::move(value.functions));
		}
		else if constexpr (std::is_member_object_pointer_v<Value>) {
			registration.addPropertyReadWrite(name, value);
		}
		else {
			registration.addFunction(name, std::move(value));
		}
		addMembers(registration, std::forward<Rest>(rest)...);
	}

	class Table
	{
	public:
		class Item
		{
		public:
			Item(luabridge::LuaRef table, std::string key)
				: m_table(std::move(table)), m_key(std::move(key))
			{}

			template<typename Value>
			Item& operator=(Value value)
			{
				if constexpr (luabridge::detail::is_callable_v<Value>) {
					m_table[m_key] = luabridge::LuaRef::newFunction(m_table.state(), std::move(value));
				}
				else {
					m_table[m_key] = std::move(value);
				}
				return *this;
			}

		private:
			luabridge::LuaRef m_table;
			std::string m_key;
		};

		explicit Table(luabridge::LuaRef table) : m_table(std::move(table)) {}

		Item operator[](const std::string& key)
		{
			return Item(m_table, key);
		}

	private:
		luabridge::LuaRef m_table;
	};

	class Registry
	{
	public:
		explicit Registry(lua_State* lua) : m_lua(lua) {}

		template<typename... Args>
		void newEnum(const char* name, Args&&... args)
		{
			auto table = luabridge::getGlobalNamespace(m_lua).beginNamespace(name);
			addEnumValues(table, std::forward<Args>(args)...);
			table.endNamespace();
		}

		template<typename T, typename... Args>
		void newUsertype(const char* name, Args&&... args)
		{
			auto registration = luabridge::getGlobalNamespace(m_lua).beginClass<T>(name);
			addMembers(registration, std::forward<Args>(args)...);
			registration.endClass();
		}

		template<typename T, typename... Bases, typename... Args>
		void newUsertype(const char* name, BaseClasses, BasesList<Bases...>, Args&&... args)
		{
			auto registration = luabridge::getGlobalNamespace(m_lua).deriveClass<T, Bases...>(name);
			addMembers(registration, std::forward<Args>(args)...);
			registration.endClass();
		}

		Table createNamedTable(const char* name)
		{
			luabridge::LuaRef table = luabridge::LuaRef::newTable(m_lua);
			if (!luabridge::setGlobal(m_lua, table, name)) {
				throw std::runtime_error(std::string("创建 Lua table 失败: ") + name);
			}
			return Table(std::move(table));
		}

		luabridge::LuaRef getGlobal(const std::string& name) const
		{
			return luabridge::getGlobal(m_lua, name.c_str());
		}

	private:
		template<typename Registration>
		void addEnumValues(Registration&)
		{}

		template<typename Registration, typename Value, typename... Rest>
		void addEnumValues(Registration& registration, const char* name, Value value, Rest&&... rest)
		{
			registration.addVariable(name, value);
			addEnumValues(registration, std::forward<Rest>(rest)...);
		}

		lua_State* m_lua;
	};
}

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
	m_lua.reset(luaL_newstate());
	if (!m_lua) {
		throw std::runtime_error("创建 Lua 状态失败");
	}
	luaL_openlibs(m_lua.get());
	luabridge::enableExceptions(m_lua.get());
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
		catch (const luabridge::LuaException& e) {
			task->promise.set_exception(std::make_exception_ptr(std::runtime_error(e.what())));
		}
		catch (...) {
			task->promise.set_exception(std::current_exception());
		}
	}
}

class LuaJson {
public:
	static json luaRef2JsonValue(const luabridge::LuaRef& obj)
	{
		if (obj.isString()) {
			return obj.cast<std::string>().value();
		}
		if (obj.isNumber()) {
			obj.push();
			const bool isInteger = lua_isinteger(obj.state(), -1);
			lua_pop(obj.state(), 1);
			if (isInteger) {
				return obj.cast<int64_t>().value();
			}
			return obj.cast<double>().value();
		}
		if (obj.isBool()) {
			return obj.cast<bool>().value();
		}
		if (obj.isTable()) {
			bool arrayLike = true;
			size_t expectedKey = 1;
			for (const auto& [key, value] : luabridge::pairs(obj)) {
				const auto keyResult = key.cast<lua_Integer>();
				if (!keyResult || *keyResult != (lua_Integer)expectedKey) {
					arrayLike = false;
					break;
				}
				++expectedKey;
			}
			arrayLike &= obj.length() == (int)(expectedKey - 1);

			if (arrayLike) {
				json arr = json::array();
				for (size_t i = 1; i <= obj.length(); ++i) {
					arr.push_back(luaRef2JsonValue(obj[(lua_Integer)i]));
				}
				return arr;
			}

			json tbl = json::object();
			for (const auto& [key, value] : luabridge::pairs(obj)) {
				if (!key.isString()) {
					throw std::runtime_error(gppTr("LuaJson.luaRef2JsonValue", "LuaJson: key 必须是字符串")
						.toStdString());
				}
				tbl[key.cast<std::string>().value()] = luaRef2JsonValue(value);
			}
			return tbl;
		}
		if (obj.isNil()) {
			return nullptr;
		}
		return "LuaJson: unsupported type";
	}

	static luabridge::LuaRef jsonValue2LuaRef(const json& value, lua_State* lua)
	{
		switch (value.type())
		{
		case json::value_t::string:
			return luabridge::LuaRef(lua, value.get<std::string>());
		case json::value_t::number_unsigned:
			return luabridge::LuaRef(lua, value.get<uint64_t>());
		case json::value_t::number_integer:
			return luabridge::LuaRef(lua, value.get<int64_t>());
		case json::value_t::number_float:
			return luabridge::LuaRef(lua, value.get<double>());
		case json::value_t::boolean:
			return luabridge::LuaRef(lua, value.get<bool>());
		case json::value_t::array:
		{
			luabridge::LuaRef resultArray = luabridge::LuaRef::newTable(lua);
			lua_Integer index = 1;
			for (const auto& elem : value) {
				resultArray[index++] = jsonValue2LuaRef(elem, lua);
			}
			return resultArray;
		}
		case json::value_t::object:
		{
			luabridge::LuaRef resultMap = luabridge::LuaRef::newTable(lua);
			for (const auto& jObj : value.items()) {
				resultMap[jObj.key()] = jsonValue2LuaRef(jObj.value(), lua);
			}
			return resultMap;
		}
		case json::value_t::null:
		case json::value_t::discarded:
		default:
			return luabridge::LuaRef(lua);
		}
	}
};

class LuaToml {
public:
	static toml::value luaRef2TomlValue(const luabridge::LuaRef& obj)
	{
		if (obj.isString()) {
			return toml::value(obj.cast<std::string>().value());
		}
		if (obj.isNumber()) {
			obj.push();
			const bool isInteger = lua_isinteger(obj.state(), -1);
			lua_pop(obj.state(), 1);
			if (isInteger) {
				return toml::value(obj.cast<int64_t>().value());
			}
			return toml::value(obj.cast<double>().value());
		}
		if (obj.isBool()) {
			return toml::value(obj.cast<bool>().value());
		}
		if (obj.isTable()) {
			bool arrayLike = true;
			size_t expectedKey = 1;
			for (const auto& [key, value] : luabridge::pairs(obj)) {
				const auto keyResult = key.cast<lua_Integer>();
				if (!keyResult || *keyResult != (lua_Integer)expectedKey) {
					arrayLike = false;
					break;
				}
				++expectedKey;
			}
			arrayLike &= obj.length() == (int)(expectedKey - 1);

			if (arrayLike) {
				toml::array arr;
				for (size_t i = 1; i <= obj.length(); ++i) {
					arr.push_back(luaRef2TomlValue(obj[(lua_Integer)i]));
				}
				return arr;
			}

			toml::table tbl;
			for (const auto& [key, value] : luabridge::pairs(obj)) {
				if (!key.isString()) {
					throw std::runtime_error(gppTr("LuaToml.luaRef2TomlValue", "LuaToml: key 必须是字符串")
						.toStdString());
				}
				tbl.insert({ key.cast<std::string>().value(), luaRef2TomlValue(value) });
			}
			return tbl;
		}
		return toml::value{ "LuaToml: unsupported type" };
	}

	static luabridge::LuaRef tomlValue2LuaRef(const toml::value& value, lua_State* lua)
	{
		if (value.is_table()) {
			luabridge::LuaRef resultMap = luabridge::LuaRef::newTable(lua);
			for (const auto& [key, val] : value.as_table()) {
				resultMap[key] = tomlValue2LuaRef(val, lua);
			}
			return resultMap;
		}
		if (value.is_array()) {
			luabridge::LuaRef resultVec = luabridge::LuaRef::newTable(lua);
			lua_Integer index = 1;
			for (const auto& elem : value.as_array()) {
				resultVec[index++] = tomlValue2LuaRef(elem, lua);
			}
			return resultVec;
		}
		if (value.is_string()) {
			return luabridge::LuaRef(lua, value.as_string());
		}
		if (value.is_integer()) {
			return luabridge::LuaRef(lua, value.as_integer());
		}
		if (value.is_floating()) {
			return luabridge::LuaRef(lua, value.as_floating());
		}
		if (value.is_boolean()) {
			return luabridge::LuaRef(lua, value.as_boolean());
		}
		return luabridge::LuaRef(lua);
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
					lua_State* lua = state->m_lua.get();
					if (luaL_loadbuffer(lua, script.data(), script.size(), chunkName.c_str()) != LUA_OK
						|| lua_pcall(lua, 0, 0, 0) != LUA_OK)
					{
						const char* error = lua_tostring(lua, -1);
						const std::string message = error ? error : "未知 Lua 错误";
						lua_pop(lua, 1);
						throw std::runtime_error(message);
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
				luabridge::LuaRef function = luabridge::getGlobal(luaState->m_lua.get(), functionName.c_str());
				if (!function.isCallable()) {
					return;
				}
				auto pFunc = std::make_unique<LuaFunction>(std::move(function));
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
	lua_State* luaState = luaStateInstance->m_lua.get();
	lua_binding::Registry lua(luaState);

	lua.newEnum("NameType",
		"None", NameType::None,
		"Single", NameType::Single,
		"Multiple", NameType::Multiple
	);

	lua.newEnum("TransEngine",
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

	lua.newEnum("CachePart",
		"None", CachePart::None,
		"Name", CachePart::Name,
		"NameTrans", CachePart::NameTrans,
		"Names", CachePart::Names,
		"NamesTrans", CachePart::NamesTrans,
		"Orig", CachePart::Orig,
		"Preproc", CachePart::Preproc,
		"Problems", CachePart::Problems,
		"OtherInfo", CachePart::OtherInfo,
		"TransBy", CachePart::TransBy,
		"TransRaw", CachePart::TransRaw,
		"Transview", CachePart::Transview
	);

	lua.newEnum("ApiProtocol",
		"OpenAI", ApiProtocol::OpenAI,
		"Claude", ApiProtocol::Claude,
		"Gemini", ApiProtocol::Gemini
	);

	lua.newUsertype<SentencePosition>("SentencePosition",
		lua_binding::Constructors<SentencePosition()>(),
		"file", &SentencePosition::file,
		"index", &SentencePosition::index
	);

	lua.newUsertype<Sentence>("Sentence",
		lua_binding::Constructors<Sentence()>(),
		"index", &Sentence::index,
		"filename", &Sentence::filename,
		"name", &Sentence::name,
		"names", &Sentence::names,
		"nametrans", &Sentence::nametrans,
		"namestrans", &Sentence::namestrans,
		"orig", &Sentence::orig,
		"preproc", &Sentence::preproc,
		"problems", &Sentence::problems,
		"transby", &Sentence::transby,
		"transraw", &Sentence::transraw,
		"transview", &Sentence::transview,
		"linebreak", &Sentence::linebreak,
		"otherinfo", lua_binding::property(
			[](Sentence& self) { return self.otherinfo; },
			[](Sentence& self, decltype(Sentence::otherinfo) value) { self.otherinfo = std::move(value); }),
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

	auto pathRegistration = luabridge::getGlobalNamespace(luaState).beginClass<fs::path>("Path");
	pathRegistration.addConstructor(
		[](void* memory, const std::string& str) { return new (memory) fs::path(ascii2Wide(str)); },
		[](void* memory) { return new (memory) fs::path(); },
		[](void* memory, const fs::path& path) { return new (memory) fs::path(path); });
	pathRegistration
		.addProperty("value",
			[](const fs::path& self) { return wide2Ascii(self); },
			[](fs::path& self, const std::string& str) { self = ascii2Wide(str); })
		.addFunction("__tostring", [](const fs::path& self) { return wide2Ascii(self); })
		.addFunction("__div",
			[](const fs::path& self, const fs::path& other) { return self / other; },
			[](const fs::path& self, const std::string& other) { return self / ascii2Wide(other); })
		.addFunction("__eq", [](const fs::path& self, const fs::path& other) { return self == other; })
		.addProperty("filename", [](const fs::path& self) { return self.filename(); })
		.addProperty("stem", [](const fs::path& self) { return self.stem(); })
		.addProperty("extension", [](const fs::path& self) { return self.extension(); })
		.addProperty("parentPath", [](const fs::path& self) { return self.parent_path(); })
		.addProperty("empty", [](const fs::path& self) { return self.empty(); })
		.addProperty("isAbsolute", [](const fs::path& self) { return self.is_absolute(); })
		.addProperty("isRelative", [](const fs::path& self) { return self.is_relative(); })
		.addFunction("equivalent", [](const fs::path& self, const fs::path& other) { return fs::equivalent(self, other); })
		.addFunction("weaklyCanonical", [](const fs::path& self) { return fs::weakly_canonical(self); })
		.addFunction("canonical", [](const fs::path& self) { return fs::canonical(self); })
		.addFunction("relativeTo", [](const fs::path& self, const fs::path& base) { return fs::relative(self, base); });
	pathRegistration.endClass();

	lua.newEnum("LogLevel",
		"trace", spdlog::level::trace,
		"debug", spdlog::level::debug,
		"info", spdlog::level::info,
		"warn", spdlog::level::warn,
		"err", spdlog::level::err,
		"critical", spdlog::level::critical
	);

	lua.newUsertype<spdlog::logger>("spdlogLogger",
		lua_binding::noConstructor,
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

	lua_binding::Table luaTomlTable = lua.createNamedTable("toml");
	luaTomlTable["parse"] = [](const fs::path& path, lua_State* lua) -> std::tuple<luabridge::LuaRef, std::optional<std::string>>
		{
			try {
				return { LuaToml::tomlValue2LuaRef(toml::uparse(path), lua), std::nullopt };
			}
			catch (const std::exception& e) {
				return { luabridge::LuaRef(lua), std::string(e.what()) };
			}
		};
	luaTomlTable["str"] = [](const luabridge::LuaRef& obj) -> std::tuple<std::optional<std::string>, std::optional<std::string>>
		{
			try {
				return { toml::format(LuaToml::luaRef2TomlValue(obj)), std::nullopt };
			}
			catch (const std::exception& e) {
				return { std::nullopt, std::string(e.what()) };
			}
		};
	luaTomlTable["save"] = [](const fs::path& path, const luabridge::LuaRef& obj) -> std::tuple<bool, std::optional<std::string>>
		{
			try {
				atomicOutputFile(path, toml::format(LuaToml::luaRef2TomlValue(obj)));
				return { true, std::nullopt };
			}
			catch (const std::exception& e) {
				return { false, std::string(e.what()) };
			}
		};

	lua_binding::Table luaJsonTable = lua.createNamedTable("json");
	luaJsonTable["parse"] = [](const fs::path& path, lua_State* lua) -> std::tuple<luabridge::LuaRef, std::optional<std::string>>
		{
			try {
				return { LuaJson::jsonValue2LuaRef(parseJson(path), lua), std::nullopt };
			}
			catch (const std::exception& e) {
				return { luabridge::LuaRef(lua), std::string(e.what()) };
			}
		};
	luaJsonTable["save"] = [](const fs::path& path, const luabridge::LuaRef& obj, std::optional<int> indent) -> std::tuple<bool, std::optional<std::string>>
		{
			try {
				const json value = LuaJson::luaRef2JsonValue(obj);
				atomicOutputFile(path, value.dump(indent.value_or(2)));
				return { true, std::nullopt };
			}
			catch (const std::exception& e) {
				return { false, std::string(e.what()) };
			}
		};

	lua.newUsertype<RuntimeTransSuccessEvent>("RuntimeTransSuccessEvent",
		lua_binding::Constructors<RuntimeTransSuccessEvent()>(),
		"timestamp", &RuntimeTransSuccessEvent::timestamp,
		"filename", &RuntimeTransSuccessEvent::filename,
		"index", &RuntimeTransSuccessEvent::index,
		"speakers", &RuntimeTransSuccessEvent::speakers,
		"sourcePreview", &RuntimeTransSuccessEvent::sourcePreview,
		"translationPreview", &RuntimeTransSuccessEvent::translationPreview,
		"transby", &RuntimeTransSuccessEvent::transby
	);

	lua.newUsertype<RuntimeTransErrorEvent>("RuntimeTransErrorEvent",
		lua_binding::Constructors<RuntimeTransErrorEvent()>(),
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

	lua.newUsertype<RuntimeFileProgress>("RuntimeFileProgress",
		lua_binding::Constructors<RuntimeFileProgress()>(),
		"filename", &RuntimeFileProgress::filename,
		"total", &RuntimeFileProgress::total,
		"completed", &RuntimeFileProgress::completed,
		"problems", &RuntimeFileProgress::problems
	);

	lua.newUsertype<IController>("IController",
		lua_binding::noConstructor,
		"m_totalSentences", lua_binding::property([](IController& self) { return self.m_totalSentences.load(); },
			[](IController& self, int value) { self.m_totalSentences = value; }),
		"m_completedSentences", lua_binding::property([](IController& self) { return self.m_completedSentences.load(); },
			[](IController& self, int value) { self.m_completedSentences = value; }),
		"m_activeThreads", lua_binding::property([](IController& self) { return self.m_activeThreads.load(); },
			[](IController& self, int value) { self.m_activeThreads = value; }),
		"m_totalThreads", lua_binding::property([](IController& self) { return self.m_totalThreads.load(); },
			[](IController& self, int value) { self.m_totalThreads = value; }),
		"makeBar", &IController::makeBar,
		"writeLog", &IController::writeLog,
		"addThreadNum", &IController::addThreadNum,
		"reduceThreadNum", &IController::reduceThreadNum,
		"updateBar", lua_binding::overload(
			[](IController& self) { self.updateBar(); },
			[](IController& self, int ticks) { self.updateBar(ticks); }),
		"setRuntimeFiles", &IController::setRuntimeFiles,
		"setRuntimeStage", lua_binding::overload(
			[](IController& self, const std::string& stage) { self.setRuntimeStage(stage); },
			[](IController& self, const std::string& stage, const std::string& currentFile) { self.setRuntimeStage(stage, currentFile); }),
		"recordFileSentenceDone", &IController::recordFileSentenceDone,
		"recordRuntimeTransSuccess", &IController::recordRuntimeTransSuccess,
		"recordRuntimeTransError", &IController::recordRuntimeTransError,
		"shouldStop", &IController::shouldStop,
		"flush", &IController::flush
	);

	lua.newUsertype<ITranslator>("ITranslator",
		lua_binding::noConstructor,
		"run", &ITranslator::run
	);

	lua.newUsertype<ctpl::thread_pool>("ThreadPool",
		lua_binding::noConstructor,
		"resize", &ctpl::thread_pool::resize,
		"size", &ctpl::thread_pool::size
	);

	lua.newUsertype<ApiPool>("ApiPool",
		lua_binding::noConstructor,
		"resortTokens", &ApiPool::resortTokens,
		"isEmpty", &ApiPool::isEmpty,
		"size", &ApiPool::size
	);

	lua.newUsertype<GptDictionary>("GptDictionary",
		lua_binding::noConstructor,
		"sort", &GptDictionary::sort,
		"loadFromFile", &GptDictionary::loadFromFile
	);

	lua.newUsertype<NormalDictionary>("NormalDictionary",
		lua_binding::noConstructor,
		"sort", &NormalDictionary::sort,
		"loadFromFile", &NormalDictionary::loadFromFile
	);

	lua.newUsertype<ProblemCompareObj>("ProblemCompareObj",
		lua_binding::Constructors<ProblemCompareObj()>(),
		"use", &ProblemCompareObj::use,
		"base", &ProblemCompareObj::base,
		"check", &ProblemCompareObj::check
	);

	lua.newUsertype<Problems>("Problems",
		lua_binding::Constructors<Problems()>(),
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

	lua.newUsertype<ProblemAnalyzer>("ProblemAnalyzer",
		lua_binding::noConstructor,
		"setProblemRule", &ProblemAnalyzer::setProblemRule,
		"analyze", [](ProblemAnalyzer& self, Sentence& sentence) { self.analyze(&sentence); }
	);

	lua.newUsertype<NameTranslator>("NameTranslator",
		lua_binding::noConstructor,
		"run", &NameTranslator::run
	);

	lua.newUsertype<DictionaryGenerator>("DictionaryGenerator",
		lua_binding::noConstructor,
		"generate", &DictionaryGenerator::generate
	);

	lua.newUsertype<NormalJsonTranslatorTransAgent>("NormalJsonTranslatorTransAgent",
		lua_binding::noConstructor,
		"applyAgentSuggestions", &NormalJsonTranslatorTransAgent::applyAgentSuggestions
	);

	lua.newUsertype<NormalJsonTranslator>("NormalJsonTranslator",
		lua_binding::baseClasses, lua_binding::bases<ITranslator>(),
		"m_transEngine", &NormalJsonTranslator::m_transEngine,
		"m_controller", lua_binding::property([](NormalJsonTranslator& self) { return self.m_controller.get(); }),
		"m_logger", lua_binding::property([](NormalJsonTranslator& self) { return self.m_logger.get(); }),
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
		"m_rollingContextCacheMap", lua_binding::property(
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
		"m_splitFilePartsToJson", lua_binding::property(
			[](NormalJsonTranslator& self) { return self.m_splitFilePartsToJson; },
			[](NormalJsonTranslator& self, decltype(NormalJsonTranslator::m_splitFilePartsToJson) value)
			{ self.m_splitFilePartsToJson = std::move(value); }),
		"m_jsonToSplitFileParts", lua_binding::property(
			[](NormalJsonTranslator& self) { return self.m_jsonToSplitFileParts; },
			[](NormalJsonTranslator& self, decltype(NormalJsonTranslator::m_jsonToSplitFileParts) value)
			{ self.m_jsonToSplitFileParts = std::move(value); }),
		"m_gptDictionaryPaths", &NormalJsonTranslator::m_gptDictionaryPaths,
		"m_agentProjectNotePath", &NormalJsonTranslator::m_agentProjectNotePath,
		"m_nameMap", lua_binding::property(
			[](NormalJsonTranslator& self) { return self.m_nameMap; },
			[](NormalJsonTranslator& self, decltype(NormalJsonTranslator::m_nameMap) value)
			{ self.m_nameMap = std::move(value); }),
		"m_currentRunRelFilePaths", &NormalJsonTranslator::m_currentRunRelFilePaths,
		"m_repeatedBlockCompletedRelFilePaths", lua_binding::property(
			[](NormalJsonTranslator& self) { return self.m_repeatedBlockCompletedRelFilePaths; },
			[](NormalJsonTranslator& self, decltype(NormalJsonTranslator::m_repeatedBlockCompletedRelFilePaths) value)
			{ self.m_repeatedBlockCompletedRelFilePaths = std::move(value); }),
		"m_onFileProcessed", &NormalJsonTranslator::m_onFileProcessed,
		"m_onPerformApi", &NormalJsonTranslator::m_onPerformApi,
		"m_onDictProcessed", &NormalJsonTranslator::m_onDictProcessed,
		"m_threadPool", lua_binding::property([](NormalJsonTranslator& self) { return &self.m_threadPool; }),
		"m_apiPool", lua_binding::property([](NormalJsonTranslator& self) { return self.m_apiPool.get(); }),
		"m_gptDictionary", lua_binding::property([](NormalJsonTranslator& self) { return self.m_gptDictionary.get(); }),
		"m_preDictionary", lua_binding::property([](NormalJsonTranslator& self) { return self.m_preDictionary.get(); }),
		"m_postDictionary", lua_binding::property([](NormalJsonTranslator& self) { return self.m_postDictionary.get(); }),
		"m_problemAnalyzer", lua_binding::property([](NormalJsonTranslator& self) { return self.m_problemAnalyzer.get(); }),
		"m_nameTranslator", lua_binding::property([](NormalJsonTranslator& self) { return self.m_nameTranslator.get(); }),
		"m_dictionaryGenerator", lua_binding::property([](NormalJsonTranslator& self) { return self.m_dictionaryGenerator.get(); }),
		"m_transAgent", lua_binding::property([](NormalJsonTranslator& self) { return self.m_transAgent.get(); }),
		"preProcess", &NormalJsonTranslator::preProcess,
		"postProcess", &NormalJsonTranslator::postProcess,
		"processFile", &NormalJsonTranslator::processFile,
		"resolveRepeatedBlockReferences", &NormalJsonTranslator::resolveRepeatedBlockReferences,
		"normalJsonInit", &NormalJsonTranslator::normalJsonInit,
		"normalJsonBeforeRun", &NormalJsonTranslator::normalJsonBeforeRun,
		"normalJsonProcessFiles", lua_binding::overload(
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

	lua.newUsertype<EpubTextNodeInfo>("EpubTextNodeInfo",
		lua_binding::Constructors<EpubTextNodeInfo()>(),
		"offset", &EpubTextNodeInfo::offset,
		"length", &EpubTextNodeInfo::length
	);

	lua.newUsertype<JsonInfo>("JsonInfo",
		lua_binding::Constructors<JsonInfo()>(),
		"metadata", &JsonInfo::metadata,
		"htmlPath", &JsonInfo::htmlPath,
		"epubPath", &JsonInfo::epubPath,
		"normalPostPath", &JsonInfo::normalPostPath,
		"content", &JsonInfo::content
	);

	lua.newUsertype<EpubTranslator>("EpubTranslator",
		lua_binding::baseClasses, lua_binding::bases<ITranslator, NormalJsonTranslator>(),
		"m_epubInputDir", &EpubTranslator::m_epubInputDir,
		"m_epubOutputDir", &EpubTranslator::m_epubOutputDir,
		"m_tempUnpackDir", &EpubTranslator::m_tempUnpackDir,
		"m_tempRebuildDir", &EpubTranslator::m_tempRebuildDir,
		"m_bilingualOutput", &EpubTranslator::m_bilingualOutput,
		"m_originalTextColor", &EpubTranslator::m_originalTextColor,
		"m_originalTextScale", &EpubTranslator::m_originalTextScale,
		"m_jsonToInfoMap", lua_binding::property(
			[](EpubTranslator& self) { return self.m_jsonToInfoMap; },
			[](EpubTranslator& self, decltype(EpubTranslator::m_jsonToInfoMap) value)
			{ self.m_jsonToInfoMap = std::move(value); }),
		"m_epubToJsonsMap", lua_binding::property(
			[](EpubTranslator& self) { return self.m_epubToJsonsMap; },
			[](EpubTranslator& self, decltype(EpubTranslator::m_epubToJsonsMap) value)
			{ self.m_epubToJsonsMap = std::move(value); }),
		"epubInit", &EpubTranslator::epubInit,
		"epubBeforeRun", &EpubTranslator::epubBeforeRun,
		"epubRun", [](EpubTranslator& self) { self.EpubTranslator::run(); }
	);

	lua.newUsertype<PDFTranslator>("PDFTranslator",
		lua_binding::baseClasses, lua_binding::bases<ITranslator, NormalJsonTranslator>(),
		"m_pdfInputDir", &PDFTranslator::m_pdfInputDir,
		"m_pdfOutputDir", &PDFTranslator::m_pdfOutputDir,
		"m_bilingualOutput", &PDFTranslator::m_bilingualOutput,
		"m_babeldocLangOut", &PDFTranslator::m_babeldocLangOut,
		"m_jsonToPDFPathMap", lua_binding::property(
			[](PDFTranslator& self) { return self.m_jsonToPDFPathMap; },
			[](PDFTranslator& self, decltype(PDFTranslator::m_jsonToPDFPathMap) value)
			{ self.m_jsonToPDFPathMap = std::move(value); }),
		"pdfInit", &PDFTranslator::pdfInit,
		"pdfBeforeRun", &PDFTranslator::pdfBeforeRun,
		"pdfRun", [](PDFTranslator& self) { self.PDFTranslator::run(); }
	);

	lua_binding::Table utilsTable = lua.createNamedTable("utils");
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
			const auto useTokenizer = lua.getGlobal(useTokenizerName).cast<std::optional<bool>>();
			if (!useTokenizer || !useTokenizer->value_or(false)) {
				return;
			}

			const std::string tokenizerBackendName = langMode + "TokenizerBackend";
			const auto tokenizerBackendResult = lua.getGlobal(tokenizerBackendName).cast<std::optional<std::string>>();
			if (!tokenizerBackendResult || !*tokenizerBackendResult) {
				throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
					.arg(scriptPath)
					.arg(tokenizerBackendName)
					.toStdString());
			}
			const std::string& tokenizerBackend = **tokenizerBackendResult;

			std::function<NLPResult(const std::string&)> tokenizeFunc;
			if (tokenizerBackend == "MeCab") {
				const std::string mecabDictDirName = langMode + "MecabDictDir";
				const auto mecabDictDirResult = lua.getGlobal(mecabDictDirName).cast<std::optional<std::string>>();
				if (!mecabDictDirResult || !*mecabDictDirResult) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(mecabDictDirName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 MeCab 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getMeCabTokenizeFunc(**mecabDictDirResult, m_logger);
			}
			else if (tokenizerBackend == "spaCy") {
				const std::string spaCyModelNameName = langMode + "SpaCyModelName";
				const auto spaCyModelNameResult = lua.getGlobal(spaCyModelNameName).cast<std::optional<std::string>>();
				if (!spaCyModelNameResult || !*spaCyModelNameResult) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(spaCyModelNameName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 spaCy 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getPythonNLPTokenizeFunc({ "click", "spacy" }, "tokenizer_spacy",
					**spaCyModelNameResult, m_logger);
			}
			else if (tokenizerBackend == "Stanza") {
				const std::string stanzaLangName = langMode + "StanzaLang";
				const auto stanzaLangResult = lua.getGlobal(stanzaLangName).cast<std::optional<std::string>>();
				if (!stanzaLangResult || !*stanzaLangResult) {
					throw std::invalid_argument(gppTr("LuaManager.registerCustomTypes", "[%1] 未设置 %2")
						.arg(scriptPath)
						.arg(stanzaLangName)
						.toStdString());
				}
				m_logger->info(gppTr("LuaManager.registerCustomTypes", "[%1] 已配置 Stanza 分词器，首次使用时加载")
					.arg(scriptPath)
					.toStdString());
				tokenizeFunc = getPythonNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza",
					**stanzaLangResult, m_logger);
			}
			else if (tokenizerBackend == "pkuseg") {
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
					.arg(tokenizerBackend)
					.toStdString());
			}

			utilsTable[langMode + "TokenizeFunc"] = std::move(tokenizeFunc);
		};

	supplyTokenizerFunc("sourceLang");
	supplyTokenizerFunc("targetLang");
}
