#include "Utils.h"
#include "DeterministicRandom.h"
#include "PCH.h"
#include "settings/Settings.h"

#undef GetObject

namespace utils
{
	RE::TESNPC::Layer* AllocateTintLayer()
	{
		return RE::calloc<RE::TESNPC::Layer>(1, kTintLayerRuntimeSize);
	}

	void FreeTintLayers(RE::BSTArray<RE::TESNPC::Layer*>*& a_tintLayers)
	{
		if (!a_tintLayers) {
			return;
		}
		for (auto* layer : *a_tintLayers) {
			RE::free(layer);
		}
		delete a_tintLayers;
		a_tintLayers = nullptr;
	}

	std::string UniqueStringFromForm(RE::TESForm* a_form_seed)
	{
		if (!a_form_seed) {
			return std::string();
		}

		auto fileName = "DynamicForm";
		const auto file = a_form_seed->GetFile();
		if (!a_form_seed->IsDynamicForm() && file) {
			fileName = file->fileName;
		}

		auto rawFormID = std::to_string(a_form_seed->GetFormID() & 0x00FFFFFF);
		if (!a_form_seed->IsDynamicForm() && file && file->IsLight()) {
			rawFormID = std::to_string(a_form_seed->GetFormID() & 0x00000FFF);
		}

		std::string playthroughID = "";
		if (Settings::GetSingleton()->features.any(Settings::Features::kPlaythroughRandomization)) {
			if (const auto manager = RE::BGSSaveLoadManager::GetSingleton()) {
				playthroughID = std::to_string(manager->currentPlayerID);
			}
		}

		return rawFormID + "_" + fileName + "_" + playthroughID;
	}

	size_t HashForm(RE::TESForm* a_form_seed)
	{
		return static_cast<std::size_t>(HashString(UniqueStringFromForm(a_form_seed)));
	}

	std::uint64_t HashString(std::string_view a_value)
	{
		return deterministic_random::HashString(a_value);
	}

	std::uint64_t StableRandom(std::uint64_t a_seed, std::uint64_t a_stream)
	{
		// SplitMix64 is deterministic, local, and does not disturb the game's global
		// C random-number state.
		return deterministic_random::Generate(a_seed, a_stream);
	}

	RE::BSTArray<RE::TESNPC::Layer*>* CopyTintLayers(RE::BSTArray<RE::TESNPC::Layer*>* a_tintLayers)
	{
		if (!a_tintLayers) {
			return nullptr;
		}
		auto copiedTintLayers = new RE::BSTArray<RE::TESNPC::Layer*>();

		if (!a_tintLayers->empty()) {
			for (auto tint : *a_tintLayers) {
				if (!tint) {
					continue;
				}
				auto newLayer = AllocateTintLayer();
				if (!newLayer) {
					continue;
				}
				newLayer->tintColor = tint->tintColor;
				newLayer->tintIndex = tint->tintIndex;
				newLayer->preset = tint->preset;
				newLayer->interpolationValue = tint->interpolationValue;
				copiedTintLayers->emplace_back(newLayer);
			}
		}

		return copiedTintLayers;
	}

	RE::TESNPC::HeadRelatedData* CopyHeadRelatedData(RE::TESNPC::HeadRelatedData* a_data)
	{
		auto newHeadData = RE::calloc<RE::TESNPC::HeadRelatedData>(1);
		if (!newHeadData) {
			stl::report_and_fail("RaceSwapper could not allocate NPC head data.");
		}
		if (a_data) {
			newHeadData->hairColor = a_data->hairColor;
			newHeadData->faceDetails = a_data->faceDetails;
		}
		return newHeadData;
	}

	RE::BGSHeadPart** CopyHeadParts(RE::BGSHeadPart** a_parts, std::uint32_t a_numHeadParts)
	{
		if (!a_parts || a_numHeadParts == 0) {
			return nullptr;
		}
		auto newHeadParts = RE::calloc<RE::BGSHeadPart*>(a_numHeadParts);
		if (!newHeadParts) {
			stl::report_and_fail("RaceSwapper could not allocate NPC head parts.");
		}
		for (std::uint32_t index = 0; index < a_numHeadParts; index++) {
			newHeadParts[index] = a_parts[index];
		}
		return newHeadParts;
	}

	RE::TESNPC::FaceData* DeepCopyFaceData(RE::TESNPC::FaceData* a_faceData)
	{
		if (!a_faceData) {
			return nullptr;
		}

		auto newFaceData = RE::calloc<RE::TESNPC::FaceData>(1);
		if (!newFaceData) {
			stl::report_and_fail("RaceSwapper could not allocate NPC face data.");
		}

		for (std::uint32_t i = 0; i < 19; i++) {
			newFaceData->morphs[i] = a_faceData->morphs[i];
		}

		for (std::uint32_t i = 0; i < 4; i++) {
			newFaceData->parts[i] = a_faceData->parts[i];
		}
		return newFaceData;
	}

	RE::TESNPC* GetRootFaceNPCSafe(RE::TESNPC* a_npc)
	{
		std::unordered_set<RE::TESNPC*> visited;
		for (auto* current = a_npc; current; current = current->faceNPC) {
			if (!visited.insert(current).second) {
				logger::error("Cyclic face-NPC template chain detected at {:x}", current->formID);
				return nullptr;
			}
			if (!current->faceNPC) {
				return current;
			}
		}
		return nullptr;
	}

	std::vector<std::string> split_string(std::string& a_string, char a_delimiter)
	{
		std::vector<std::string> list;
		std::string strCopy = a_string;
		size_t pos = 0;
		std::string token;
		while ((pos = strCopy.find(a_delimiter)) != std::string::npos) {
			token = strCopy.substr(0, pos);
			list.push_back(token);
			strCopy.erase(0, pos + 1);
		}
		list.push_back(strCopy);
		return list;
	}

	std::string GetEditorID(RE::FormID a_formID)
	{
		static auto tweaks = GetModuleHandle(L"po3_Tweaks");
		static auto function = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
		if (function) {
			if (const auto editorID = function(a_formID)) {
				return editorID;
			}
		}
		return {};
	}

	std::string GetFormEditorID(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return {};
		}
		if (a_form->IsDynamicForm()) {
			return a_form->GetFormEditorID();
		}
		switch (a_form->GetFormType()) {
		case RE::FormType::Keyword:
		case RE::FormType::LocationRefType:
		case RE::FormType::Action:
		case RE::FormType::MenuIcon:
		case RE::FormType::Global:
		case RE::FormType::HeadPart:
		case RE::FormType::Race:
		case RE::FormType::Sound:
		case RE::FormType::Script:
		case RE::FormType::Navigation:
		case RE::FormType::Cell:
		case RE::FormType::WorldSpace:
		case RE::FormType::Land:
		case RE::FormType::NavMesh:
		case RE::FormType::Dialogue:
		case RE::FormType::Quest:
		case RE::FormType::Idle:
		case RE::FormType::AnimatedObject:
		case RE::FormType::ImageAdapter:
		case RE::FormType::VoiceType:
		case RE::FormType::Ragdoll:
		case RE::FormType::DefaultObject:
		case RE::FormType::MusicType:
		case RE::FormType::StoryManagerBranchNode:
		case RE::FormType::StoryManagerQuestNode:
		case RE::FormType::StoryManagerEventNode:
		case RE::FormType::SoundRecord:
			return a_form->GetFormEditorID();
		default:
			return GetEditorID(a_form->GetFormID());
		}
	};

	RE::TESRace* GetValidRaceForArmorRecursive(RE::TESObjectARMO* a_armor, RE::TESRace* a_race)
	{
		if (!a_armor) {
			return nullptr;
		}
		std::unordered_set<RE::TESRace*> visited;
		for (auto race = a_race; race && visited.insert(race).second; race = race->armorParentRace) {
			for (auto addon : a_armor->armorAddons) {
				if (addon && (addon->race == race || is_amongst(addon->additionalRaces, race))) {
					return race;
				}
			}
		}
		return nullptr;
	}

	bool IsVampire(RE::TESNPC* a_npc)
	{
		return a_npc && a_npc->race && a_npc->race->HasKeywordID(0xA82BB);  // Vampire keyword
	}

	static inline std::map<RE::TESRace*, RE::TESRace*> GetRaceCompatibilityMap(bool isVampireKey)
	{
		std::map<RE::TESRace*, RE::TESRace*> raceMap;

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return raceMap;
		}
		RE::BGSListForm* raceList = nullptr;
		RE::BGSListForm* raceVampireList = nullptr;

		raceList = raceList ? raceList : dataHandler->LookupForm<RE::BGSListForm>(0xD62, "RaceCompatibility.esm");
		raceVampireList = raceVampireList ? raceVampireList : dataHandler->LookupForm<RE::BGSListForm>(0xD63, "RaceCompatibility.esm");

		raceList = raceList ? raceList : RE::TESForm::LookupByEditorID<RE::BGSListForm>("PlayableRaceList");
		raceVampireList = raceVampireList ? raceVampireList : RE::TESForm::LookupByEditorID<RE::BGSListForm>("PlayableVampireList");

		if (!raceList || !raceVampireList ||
			raceList->scriptAddedFormCount != raceVampireList->scriptAddedFormCount ||
			raceList->forms.size() != raceVampireList->forms.size()) {
			return raceMap;
		}

		auto keyList = isVampireKey ? raceVampireList : raceList;
		auto valueList = isVampireKey ? raceList : raceVampireList;

		for (std::uint32_t i = 0; i < raceList->forms.size(); i++) {
			auto key = keyList->forms[i] ? keyList->forms[i]->As<RE::TESRace>() : nullptr;
			auto value = valueList->forms[i] ? valueList->forms[i]->As<RE::TESRace>() : nullptr;
			if (key && value) {
				raceMap.emplace(key, value);
			}
			
		}

		if (!keyList->scriptAddedTempForms || !valueList->scriptAddedTempForms) {
			return raceMap;
		}

		auto keyFormList = keyList->scriptAddedTempForms;
		auto valueFormList = valueList->scriptAddedTempForms;

		const auto& [allForms, allFormsLock] = RE::TESForm::GetAllForms();
		if (!allForms) {
			return raceMap;
		}
		const RE::BSReadLockGuard lock{ allFormsLock };
		auto lookupRace = [&](RE::FormID a_formID) {
			const auto found = allForms->find(a_formID);
			return found != allForms->end() && found->second ? found->second->As<RE::TESRace>() : nullptr;
		};

		const auto count = std::min({ raceList->scriptAddedFormCount, keyFormList->size(), valueFormList->size() });
		for (std::uint32_t i = 0; i < count; i++) {
			auto key = lookupRace((*keyFormList)[i]);
			auto value = lookupRace((*valueFormList)[i]);
			if (key && value) {
				raceMap.emplace(key, value);
			}
		}

		return raceMap;
	}

	static inline RE::TESRace* ConvertRace(RE::TESRace* a_race, bool toVampire)
	{
		if (!a_race) {
			return a_race;
		}
		auto isVampire = a_race->HasKeywordID(0xA82BB);  // Vampire keyword
		if (isVampire == toVampire) {
			return a_race;
		}

		// First attempt, use editor ID manipulation to find the vampire race as this is the least likely to produce false positives
		auto editorID = utils::GetFormEditorID(a_race);
		auto newEditorID = editorID + "";
		if (toVampire) {
			newEditorID = editorID + "Vampire";
		} else {
			auto vampireIndex = editorID.find("Vampire");
			if (vampireIndex != std::string::npos) {
				newEditorID = editorID.erase(vampireIndex);
			} else {
				newEditorID = "N/A";
			}
		}
		auto race = RE::TESForm::LookupByEditorID(newEditorID);
		if (race && race->As<RE::TESRace>()) {
			return race->As<RE::TESRace>();
		}

		// Second attempt, use race compatibility list to lookup vampire -> non-vampire swaps
		std::map<RE::TESRace*, RE::TESRace*> map = GetRaceCompatibilityMap(!toVampire);
		if (map.contains(a_race)) {
			return map[a_race];
		}		

		// Fallback, return original race
		return a_race;		
	}

	RE::TESRace* AsNonVampireRace(RE::TESRace* a_race)
	{
		return ConvertRace(a_race, false);
	}

	RE::TESRace* AsVampireRace(RE::TESRace* a_race)
	{
		return ConvertRace(a_race, true);
	}

	bool IsRaceWerewolfOrVampire(RE::TESRace* a_race) 
	{
		if (!a_race) {
			return false;
		}
		if (a_race->formID == 0xCDD84) {
			return true;
		}
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		const auto vampireRace = dataHandler ? dataHandler->LookupForm<RE::TESRace>(0x283A, "Dawnguard.esm") : nullptr;
		return vampireRace && a_race == vampireRace;
	}
}
