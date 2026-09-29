#pragma once
#include "ConfigurationEntry.h"
#include "ConfigParsing.h"
#include "Utils.h"
#include "MergeMapperPluginAPI.h"

std::string trimLine(std::string a_line)
{
	return std::string(configparse::Trim(a_line));
}

template <class T>
T* GetFormFromString(std::string line) {
	auto form = RE::TESForm::LookupByEditorID(line);
	if (form && form->As<T>()) {
		return form->As<T>();
	}

	if (line.find('~') == std::string::npos) {
		logger::error("missing plugin: {}", line);
		return nullptr;
	}

	auto plugin = trimLine(line.substr(line.find('~') + 1));
	const auto parsedID = configparse::Unsigned(line.substr(0, line.find('~')), 16);
	if (!parsedID || plugin.empty()) {
		logger::error("Invalid form reference: {}", line);
		return nullptr;
	}
	RE::FormID formID = *parsedID;
	if (g_mergeMapperInterface) {
		auto mergeForm = g_mergeMapperInterface->GetNewFormID(plugin.c_str(), formID);
		if (!mergeForm.first) {
			logger::error("MergeMapper returned no plugin for {}", line);
			return nullptr;
		}
		plugin = mergeForm.first;
		formID = mergeForm.second;
	}

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	form = dataHandler ? dataHandler->LookupForm(formID, plugin) : nullptr;
	if (form == nullptr) {
		logger::error("invalid form ID: {}", line);
		return nullptr;
	}

	if (form->As<T>()) {
		return form->As<T>();
	}
	return nullptr;
}

RE::TESForm* GetFormFromString(std::string line)
{
	return GetFormFromString<RE::TESForm>(line);
}

RE::SEX GetSexFromString(std::string line) {
	std::transform(line.begin(), line.end(),
		line.begin(),  // write to the same location
		[](unsigned char c) { return (char) std::toupper(c); });
	
	if (line == "MALE") {
		return RE::SEX::kMale;
	} else if (line == "FEMALE") {
		return RE::SEX::kFemale;
	} else {
		return RE::SEX::kNone;
	}
}

bool ConstructExcludesData(std::string a_line, ConfigurationEntry::EntryData* a_data) {
	auto line = trimLine(a_line);
	if (line == "") {
		return true;
	}
	std::string match = "exclude=";
	line.erase(0, match.size());
	auto filters = utils::split_string(line, '|');
	for (auto& entry : filters) {
		entry = trimLine(entry);
		if (auto sex = GetSexFromString(entry); sex != RE::SEX::kNone) {
			a_data->excludedSexes.insert(sex);
		} else if(auto form = GetFormFromString(entry); form && form->Is(RE::FormType::NPC)) {
			a_data->excludedNPCs.insert(form->As<RE::TESNPC>());
		} else if (form && form->Is(RE::FormType::Race)) {
			a_data->excludedRaces.insert(form->As<RE::TESRace>());
		} else if (form && form->Is(RE::FormType::Faction)) {
			a_data->excludedFactions.insert(form->As<RE::TESFaction>());
		} else {
			// An unresolved exclusion must invalidate the rule, not widen its match.
			return false;
		}
	}

	return true;
}

bool ConstructMatchData(std::string a_line, ConfigurationEntry::EntryData* a_data)
{
	auto line = trimLine(a_line);
	std::string match = "match=";
	line.erase(0, match.size());
	auto filters = utils::split_string(line, '|');

	bool hasValidData = true;

	for (auto& entry : filters) {
		entry = trimLine(entry);
		if (auto percent = configparse::Percentage(entry); percent) {
			a_data->probability = *percent;
		} else if (auto sex = GetSexFromString(entry); sex != RE::SEX::kNone) {
			a_data->sexMatch = sex;
		} else if (auto form = GetFormFromString(entry); form && form->Is(RE::FormType::NPC)) {
			a_data->npcMatch = form->As<RE::TESNPC>();
		} else if (form && form->Is(RE::FormType::Race)) {
			a_data->raceMatch = form->As<RE::TESRace>();
		} else if (form && form->Is(RE::FormType::Faction)) {
			a_data->factionMatch = form->As<RE::TESFaction>();
		} else {
			hasValidData = false;
		}
	}

	// Enforce NPC, race or faction match
	if (!a_data->npcMatch && !a_data->factionMatch && !a_data->raceMatch) {
		hasValidData = false; 
	}

	return hasValidData;
}

bool ConstructSwapData(std::string a_line, ConfigurationEntry::EntryData* a_data)
{
	auto line = trimLine(a_line);
	std::string swap = "swap=";
	line.erase(0, swap.size());
	auto filters = utils::split_string(line, '|');

	bool hasValidData = true;

	for (auto& entry : filters) {
		entry = trimLine(entry);
		logger::debug("Parsing for swap \"{}\"", entry);
		if (auto sex = GetSexFromString(entry); sex != RE::SEX::kNone) {
			a_data->otherSex = sex;
		} else if (auto percent = configparse::Percentage(entry); percent) {
			a_data->weight = *percent;
		} else if (auto weight = configparse::Unsigned(entry); weight) {
			a_data->weight = std::min(*weight, 100U);
		} else if (auto form = GetFormFromString(entry); form && form->Is(RE::FormType::NPC)) {
			a_data->otherNPC = form->As<RE::TESNPC>();
		} else if (form && form->Is(RE::FormType::Race)) {
			a_data->otherRace = form->As<RE::TESRace>();
		} else {
			hasValidData = false;
		}
	}

	// Enforce swap has an NPC/race/sex to actually swap to
	if (!a_data->otherNPC && !a_data->otherRace && a_data->otherSex == RE::SEX::kNone) {
		hasValidData = false;
	}

	return hasValidData;
}

ConfigurationEntry* ConfigurationEntry::ConstructNewEntry(std::string a_line, std::string a_file)
{
	auto parsingLine = std::string(a_line);
	ConfigurationEntry::EntryData entryData{};

	/////////// Default Values /////////////
	entryData.weight = 10;
	entryData.probability = 100;
	////////////////////////////////////////

	const auto sections = configparse::SplitSections(parsingLine);
	if (!sections) {
		if (!configparse::Trim(std::string_view(parsingLine).substr(0, parsingLine.find('#'))).empty()) {
			logger::error("Invalid rule in {}: {}. Expected match=... swap=... [exclude=...]", a_file, a_line);
		}
		return nullptr;
	}
	logger::info("Parsing: {}", a_line);
	const auto matchLine = std::string(sections->match);
	const auto swapLine = std::string(sections->swap);
	const auto excludeLine = std::string(sections->exclude);

	bool success = false;
	try {
		success = ConstructMatchData(matchLine, &entryData) &&
			ConstructSwapData(swapLine, &entryData) &&
			ConstructExcludesData(excludeLine, &entryData); 
	} catch (...) {
		logger::error("line: \"{}\" is invalid", a_line);
	}
	

	if (success) {
		auto newEntry = new ConfigurationEntry();
		newEntry->entryData = entryData;
		newEntry->entryData.file = a_file;
		newEntry->entryData.entry = a_line;
		logger::debug("Converted entry: matchNPC={:x}", entryData.npcMatch ? entryData.npcMatch->formID : 0);
		logger::debug("Converted entry: matchRace={:x}", entryData.raceMatch ? entryData.raceMatch->formID : 0);
		logger::debug("Converted entry: matchFaction={:x}", entryData.factionMatch ? entryData.factionMatch->formID : 0);
		logger::debug("Converted entry: matchSex={}", std::to_underlying(entryData.sexMatch));
		logger::debug("Converted entry: swapNPC={:x}", entryData.otherNPC ? entryData.otherNPC->formID : 0);
		logger::debug("Converted entry: swapRace={:x}", entryData.otherRace ? entryData.otherRace->formID : 0);
		logger::debug("Converted entry: swapSex={}", std::to_underlying(entryData.otherSex));
		return newEntry;
	}
	logger::error("line: \"{}\" is invalid", a_line);
	return nullptr;	
}

bool ConfigurationEntry::MatchesNPC(RE::TESNPC* a_npc) {
	if (!a_npc || !a_npc->race || (entryData.otherNPC && !entryData.otherNPC->race)) {
		return false;
	}

	auto nonVampireRace = utils::AsNonVampireRace(a_npc->race);

	bool isMatch = true;

	isMatch = isMatch && (entryData.sexMatch == RE::SEX::kNone || entryData.sexMatch == a_npc->GetSex());
	isMatch = isMatch && (!entryData.npcMatch || entryData.npcMatch->formID == a_npc->formID);
	isMatch = isMatch && (!entryData.raceMatch || entryData.raceMatch->formID == nonVampireRace->formID);
	isMatch = isMatch && (!entryData.factionMatch || a_npc->IsInFaction(entryData.factionMatch));

	// If NPC matches exclusions, do not match
	isMatch = isMatch && !entryData.excludedNPCs.contains(a_npc);
	isMatch = isMatch && !entryData.excludedRaces.contains(nonVampireRace);
	isMatch = isMatch && !entryData.excludedSexes.contains(a_npc->GetSex());
	for (auto excludedFaction: entryData.excludedFactions) {
		isMatch = isMatch && !a_npc->IsInFaction(excludedFaction);
	}

	// Prevents child NPCs matching for adult swaps and vice-versa
	isMatch = isMatch && (!entryData.otherRace || nonVampireRace->IsChildRace() == entryData.otherRace->IsChildRace());
	isMatch = isMatch && (!entryData.otherNPC || nonVampireRace->IsChildRace() == entryData.otherNPC->race->IsChildRace());
	
	// Prevent matching if NPC would not be swapped at all (ie already male/female, already given race, already given NPC, etc>)
	bool npcCanSwap = false;
	npcCanSwap = npcCanSwap || (entryData.otherRace && entryData.otherRace != nonVampireRace);
	npcCanSwap = npcCanSwap || (entryData.otherNPC && entryData.otherNPC != a_npc);
	npcCanSwap = npcCanSwap || (entryData.otherSex != RE::SEX::kNone && entryData.otherSex != RE::SEX::kTotal && entryData.otherSex != a_npc->GetSex());

	isMatch = isMatch && npcCanSwap;

	if (isMatch) {
		const auto entrySeed = utils::HashString(entryData.file) ^ utils::HashString(entryData.entry);
		const auto roll = utils::StableRandom(utils::HashForm(a_npc) ^ entrySeed);
		isMatch = (roll % 100) < entryData.probability;
	}

	return isMatch;
}
