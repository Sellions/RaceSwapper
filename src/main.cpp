#include "configuration/Configuration.h"
#include "Hooks.h"
#include "Utils.h"
#include "settings/Settings.h"
#include "swap/RaceSwapDatabase.h"
#include "MergeMapperPluginAPI.h"
#include "Papyrus.h"

void MessageInterface(SKSE::MessagingInterface::Message* msg) {
	switch (msg->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
		{
			logger::info("Dependencies check...");
			if (!GetModuleHandle(L"po3_Tweaks")) {
				logger::critical("po3_Tweaks not detected, mod will not function right!");
			}
			MergeMapperPluginAPI::GetMergeMapperInterface001();  // Request interface
			if (g_mergeMapperInterface) {                        // Use Interface
				const auto version = g_mergeMapperInterface->GetBuildNumber();
				logger::info("Got MergeMapper interface buildnumber {}", version);
			} else
				logger::info("MergeMapper not detected");
			logger::info("Dependencies check complete!");
			break;
		}
		case SKSE::MessagingInterface::kDataLoaded:
		{
			// TODO: Add console commands to revert appearance
			ConfigurationDatabase::GetSingleton()->Initialize();
			raceswap::DataBase::GetSingleton();
			hook::InstallHooks();
			break;
		}
	}
}

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifdef _DEBUG
	log->set_level(spdlog::level::debug);
	log->flush_on(spdlog::level::debug);
#else
	auto isDebugLoggingEnabled = Settings::GetSingleton()->features.any(Settings::Features::kDebugLogging);
	if (isDebugLoggingEnabled)
	{
		log->set_level(spdlog::level::debug);
		log->flush_on(spdlog::level::debug);
	} else {
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
	}

#endif
	

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%T.%e] [%=5t] [%L] %v");
	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(REL::Version{
		static_cast<std::uint16_t>(Version::MAJOR),
		static_cast<std::uint16_t>(Version::MINOR),
		static_cast<std::uint16_t>(Version::PATCH),
		0 });
	v.PluginName("RaceSwapper");
	v.AuthorName("Nightfallstorm and Hanotak");
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_1_7_104 });
	v.MinimumRequiredXSEVersion(REL::Version{ 2, 3, 1, 0 });

	return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = static_cast<std::uint32_t>(Version::MAJOR);

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .log = false, .trampoline = true, .trampolineSize = 1024 });
	Settings::GetSingleton()->Load();
	InitializeLog();

	constexpr auto supportedRuntime = SKSE::RUNTIME_SSE_1_7_104;
	const auto runtime = REL::Module::get().version();
	if (runtime != supportedRuntime) {
		logger::critical(
			"Unsupported Skyrim runtime {}. This build supports Steam 1.7.104.0 only.",
			runtime.string());
		return false;
	}

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageInterface)) {
		logger::critical("Failed to register the SKSE messaging listener");
		return false;
	}
	auto papyrusInterface = SKSE::GetPapyrusInterface();
	if (!papyrusInterface || !papyrusInterface->Register(Papyrus::Bind)) {
		logger::critical("Failed to register Papyrus functions");
		return false;
	}
	logger::info("Loaded Plugin");
	return true;
}

extern "C" DLLEXPORT const char* APIENTRY GetPluginVersion()
{
	return Version::NAME.data();
}
