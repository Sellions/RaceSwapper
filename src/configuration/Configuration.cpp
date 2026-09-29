#pragma once
#include "Configuration.h"
#include "WeightedSelection.h"
#include "Utils.h"

void ConfigurationDatabase::Initialize()
{
	logger::info("Reading config APIs...");
	entries.clear();

	constexpr auto path = L"Data/SKSE/Plugins/RaceSwap";
	std::error_code error;
	std::filesystem::directory_iterator entry(path, error), end;
	if (error) {
		logger::warn("Cannot read RaceSwap folder: {}", error.message());
		return;
	}
	std::vector<std::filesystem::path> configPaths;
	for (; entry != end; entry.increment(error)) {
		if (error) {
			break;
		}
		std::error_code statusError;
		if (entry->is_regular_file(statusError) && !statusError) {
			configPaths.push_back(entry->path());
		}
	}
	if (error) {
		logger::error("Stopped scanning RaceSwap folder: {}", error.message());
	}

	std::sort(configPaths.begin(), configPaths.end());
	for (const auto& configPath : configPaths) {
		try {
			const auto utf8 = configPath.u8string();
			const std::string fileName(utf8.begin(), utf8.end());
			logger::info("Parsing file {}", fileName);
			std::ifstream config(configPath);
			if (!config.is_open()) {
				logger::error("Couldn't open file {}", fileName);
				continue;
			}
			std::string line;
			bool firstLine = true;
			while (std::getline(config, line)) {
				if (firstLine && line.starts_with("\xEF\xBB\xBF")) {
					line.erase(0, 3);  // UTF-8 BOM, common in files saved on Windows.
				}
				firstLine = false;
				if (auto configEntry = ConfigurationEntry::ConstructNewEntry(line, fileName)) {
					entries.emplace_back(configEntry);
				}
			}
			if (config.bad()) {
				logger::error("I/O error reading config {}", fileName);
			}
		} catch (const std::exception& exception) {
			logger::error("Skipping unreadable config file: {}", exception.what());
		}
	}
	logger::info("Config APIs fully parsed!");
}

ConfigurationEntry* PickRandomWeightedEntry(const std::vector<std::pair<std::uint32_t, ConfigurationEntry*>>& a_entries, RE::TESNPC* a_npc)
{
	if (a_entries.empty()) {
		return nullptr;	
	}
	if (a_entries.size() == 1) {
		return a_entries[0].first ? a_entries[0].second : nullptr;
	}
	std::vector<std::uint32_t> weights;
	weights.reserve(a_entries.size());
	for (const auto& entry : a_entries) {
		weights.push_back(entry.first);
	}
	if (configparse::TotalWeight(weights) == 0) {
		return nullptr;
	}

	const auto roll = utils::StableRandom(utils::HashForm(a_npc), 0x52535750);
	const auto index = configparse::WeightedIndex(weights, roll);
	return index ? a_entries[*index].second : nullptr;
}

std::unique_ptr<AppearanceConfiguration> ConfigurationDatabase::GetConfigurationForNPC(RE::TESNPC* a_npc) {
	if (!a_npc || !a_npc->race) {
		return nullptr;
	}
	std::vector<std::pair<std::uint32_t, ConfigurationEntry*>> matchedEntries;
	for (const auto& entry : entries) {
		if (entry->MatchesNPC(a_npc)) {
			matchedEntries.push_back({ entry->entryData.weight, entry.get() });
		}
	}

	for (auto& entry : matchedEntries) {
		logger::debug("Consider following entry for NPC {:x}: file: \"{}\" entry: \"{}\"",
			a_npc->formID,
			entry.second->entryData.file,
			entry.second->entryData.entry
		);
	}

	if (!matchedEntries.empty()) {
		auto matchedEntry = PickRandomWeightedEntry(matchedEntries, a_npc);
		if (!matchedEntry) {
			return nullptr;
		}
		auto config = std::make_unique<AppearanceConfiguration>();
		config->otherRace = matchedEntry->entryData.otherRace;
		config->otherNPC = matchedEntry->entryData.otherNPC;
		// Setup config to match vampire/non-vampire NPC to vampire/non-vampire race counterpart
		if (utils::IsVampire(a_npc)) {
			config->otherRace = utils::AsVampireRace(config->otherRace);
		} else {
			config->otherRace = utils::AsNonVampireRace(config->otherRace);
		}
		config->otherSex = matchedEntry->entryData.otherSex;
		config->file = matchedEntry->entryData.file;
		config->entry = matchedEntry->entryData.entry;
		return config;
	}

	return nullptr;
}
