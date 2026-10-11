// F4SE entry points for Fallout 4 1.10.163 (old-gen) and F4SE 0.6.23.
//
// F4SE 0.6.x looks for F4SEPlugin_Query + F4SEPlugin_Load. Plugins built
// only with the newer F4SEPlugin_Version export (CommonLibF4 NG, LooksMenu
// 1.7.x) are skipped by this loader, so we deliberately use the old style.

#include "PCH.h"

#include "Plugin.h"

namespace
{
	void InitLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= PLUGIN_NAME ".log"sv;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);
	}

	void F4SEAPI OnMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			witcher::plugin::Install();
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			witcher::plugin::OnGameLoaded();
			break;
		default:
			break;
		}
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	InitLogging();

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = PLUGIN_NAME;
	a_info->version = PLUGIN_VERSION_MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	// Addresses come from the 1.10.163 Address Library; refuse anything else
	// rather than crash on a mismatched layout.
	const auto ver = a_f4se->RuntimeVersion();
	if (ver != F4SE::RUNTIME_1_10_163) {
		logger::critical("unsupported runtime v{}, need 1.10.163", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	logger::info("{} v{}.{}.{} loading", PLUGIN_NAME, PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH);

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::critical("couldn't register for F4SE messages");
		return false;
	}
	return true;
}
