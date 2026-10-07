module;

#include "GPPMacros.hpp"
#include <proxy/proxy_macros.h>
#ifdef RESHARPER_HIGHLIGHT
#include <proxy/proxy.h>
#endif

export module IPlugin;

export import GPPDefines;
export import LuaManager;
export import PythonManager;
export import proxy.v4;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

extern "C++" {
	PRO_DEF_MEM_DISPATCH(MemDPreRun, dPreRun);
	PRO_DEF_MEM_DISPATCH(MemPreRun, preRun);
	PRO_DEF_MEM_DISPATCH(MemPostRun, postRun);
	PRO_DEF_MEM_DISPATCH(MemDPostRun, dPostRun);
}

NAMESPACE_END(gpp)

export NAMESPACE_BEGIN(pro)

template <>
struct weak_dispatch<gpp::MemDPreRun> : gpp::MemDPreRun {
	using gpp::MemDPreRun::operator();
	template <class... Args>
	void operator()(Args&&...) const
		requires(!std::is_invocable_v<gpp::MemDPreRun, Args...>)
	{ }
};

template <>
struct weak_dispatch<gpp::MemPreRun> : gpp::MemPreRun {
	using gpp::MemPreRun::operator();
	template <class... Args>
	void operator()(Args&&...) const
		requires(!std::is_invocable_v<gpp::MemPreRun, Args...>)
	{ }
};

template <>
struct weak_dispatch<gpp::MemPostRun> : gpp::MemPostRun {
	using gpp::MemPostRun::operator();
	template <class... Args>
	void operator()(Args&&...) const
		requires(!std::is_invocable_v<gpp::MemPostRun, Args...>)
	{ }
};

template <>
struct weak_dispatch<gpp::MemDPostRun> : gpp::MemDPostRun {
	using gpp::MemDPostRun::operator();
	template <class... Args>
	void operator()(Args&&...) const
		requires(!std::is_invocable_v<gpp::MemDPostRun, Args...>)
	{ }
};

NAMESPACE_END(pro)

export NAMESPACE_BEGIN(gpp)

struct PPlugin : pro::facade_builder
	::add_convention<pro::weak_dispatch<MemDPreRun>, void(Sentence*)>
	::add_convention<pro::weak_dispatch<MemPreRun>, void(Sentence*)>
	::add_convention<pro::weak_dispatch<MemPostRun>, void(Sentence*)>
	::add_convention<pro::weak_dispatch<MemDPostRun>, void(Sentence*)> // 弱 proxy 约束的实现不是必须的，如果没有实现则默认空实现
	::build { };

void registerPlugins(std::vector<pro::proxy<PPlugin>>& plugins, const std::vector<std::string>& pluginNames, const fs::path& projectDir, const fs::path& otherCacheDir,
	const std::unique_ptr<PythonManager>&, const std::unique_ptr<LuaManager>&, const std::shared_ptr<spdlog::logger>& logger,
	const toml::value& projectConfig, bool preProcOnly);

NAMESPACE_END(gpp)
