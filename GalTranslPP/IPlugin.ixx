module;

#include "GPPMacros.hpp"
#include <toml.hpp>
#include <proxy/proxy.h> // 直接 export import proxy.v4; 也是可以的，但 IDE 的智能提示会疯狂报红。。。

export module IPlugin;

export import GPPDefines;
export import LuaManager;
export import PythonManager;
// export import proxy.v4;

namespace fs = std::filesystem;

extern "C++" {
	PRO_DEF_MEM_DISPATCH(MemDPreRun, dPreRun);
	PRO_DEF_MEM_DISPATCH(MemPreRun, preRun);
	PRO_DEF_MEM_DISPATCH(MemPostRun, postRun);
	PRO_DEF_MEM_DISPATCH(MemDPostRun, dPostRun);
}

export
{
	template <>
	struct pro::weak_dispatch<MemDPreRun> : MemDPreRun {
		using MemDPreRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemDPreRun, Args...>)
		{ }
	};

	template <>
	struct pro::weak_dispatch<MemPreRun> : MemPreRun {
		using MemPreRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemPreRun, Args...>)
		{ }
	};

	template <>
	struct pro::weak_dispatch<MemPostRun> : MemPostRun {
		using MemPostRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemPostRun, Args...>)
		{ }
	};

	template <>
	struct pro::weak_dispatch<MemDPostRun> : MemDPostRun {
		using MemDPostRun::operator();
		template <class... Args>
		void operator()(Args&&...) const
			requires(!std::is_invocable_v<MemDPostRun, Args...>)
		{ }
	};

	struct PPlugin : pro::facade_builder
		::add_convention<pro::weak_dispatch<MemDPreRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemPreRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemPostRun>, void(Sentence*)>
		::add_convention<pro::weak_dispatch<MemDPostRun>, void(Sentence*)> // 弱 proxy 约束的实现不是必须的，如果没有实现则默认空实现
		::build { };

	void registerPlugins(std::vector<pro::proxy<PPlugin>>& plugins, const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
		const std::unique_ptr<PythonManager>&, const std::unique_ptr<LuaManager>&, const std::shared_ptr<spdlog::logger>& logger,
		const toml::value& projectConfig, bool preProcOnly);
}