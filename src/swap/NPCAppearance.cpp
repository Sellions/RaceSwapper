#pragma once

#include "NPCAppearance.h"
#include "configuration/Configuration.h"
#include "NPCSwap.h"
#include "RaceSwap.h"
#include "SexSwap.h"
#include "Utils.h"

static void UpdateLoadedActors(RE::TESNPC* a_npc) {
	const auto processes = RE::ProcessLists::GetSingleton();
	if (!processes) {
		return;
	}
	for (auto actorHandle : processes->highActorHandles) {
		const auto actor = actorHandle.get();
		if (actor && actor->GetActorBase() == a_npc && actor->Is3DLoaded() && actor->GetActorRuntimeData().currentProcess) {
			logger::info("Updated loaded actor {:x} NPC {}{:x}",
				actor->formID,
				utils::GetFormEditorID(a_npc),
				a_npc->formID
			);
			actor->GetActorRuntimeData().currentProcess->Update3DModel(actor.get());
		}
	}
}

bool NPCAppearance::ApplyNewAppearance(bool updateLoadedActors)
{
	if (isNPCSwapped || isSwapDisabled) {
		return false;
	}

	ApplyAppearance(&alteredNPCData);
	// A model refresh can re-enter the appearance hooks.
	isNPCSwapped = true;
	if (updateLoadedActors) {
		UpdateLoadedActors(npc);
	}
	
	return true;
}

bool NPCAppearance::RevertNewAppearance(bool updateLoadedActors)
{
	if (!isNPCSwapped) {
		return false;
	}

	ApplyAppearance(&originalNPCData);
	isNPCSwapped = false;
	if (updateLoadedActors) {
		const auto wasDisabled = isSwapDisabled;
		isSwapDisabled = true;  // LoadSkinHook must not reapply during this refresh.
		const stl::scope_exit restoreDisabled([&]() { isSwapDisabled = wasDisabled; });
		UpdateLoadedActors(npc);
	}

	return true;
}

void NPCAppearance::ApplyAppearance(NPCData* a_data)
{
	ReleasePreviousAppliedAllocations();

	npc->height = a_data->height;
	npc->weight = a_data->weight;
	if (a_data->sex == RE::SEX::kFemale) {
		npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
	} else {
		npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kFemale);
	}
	npc->bodyTintColor = a_data->bodyTintColor;
	npc->skin = a_data->skin;
	npc->farSkin = a_data->farSkin;

	// skeletonModel applied from hooks for race
	// faceRelatedData applied from hooks for race
	// isBeastRace keyword applied from hooks for race

	npc->tintLayers = utils::CopyTintLayers(a_data->tintLayers);
	appliedAllocations.tintLayers = npc->tintLayers;

	npc->faceNPC = a_data->faceNPC;
	if (npc->faceNPC == npc) {
		npc->faceNPC = nullptr;
	}

	npc->headRelatedData = utils::CopyHeadRelatedData(a_data->headRelatedData);
	appliedAllocations.headRelatedData = npc->headRelatedData;

	npc->numHeadParts = static_cast<std::int8_t>(a_data->numHeadParts);
	npc->headParts = utils::CopyHeadParts(a_data->headParts, a_data->numHeadParts);
	appliedAllocations.headParts = npc->headParts;

	// TODO: Check for default face struct here?
	npc->faceData = utils::DeepCopyFaceData(a_data->faceData);
	appliedAllocations.faceData = npc->faceData;
}

void NPCAppearance::ReleasePreviousAppliedAllocations()
{
	// The original game data may use shared/static storage, so only release buffers
	// that this instance installed and that the NPC still points at.
	if (appliedAllocations.tintLayers && npc->tintLayers == appliedAllocations.tintLayers) {
		utils::FreeTintLayers(npc->tintLayers);
	}
	if (appliedAllocations.headRelatedData && npc->headRelatedData == appliedAllocations.headRelatedData) {
		RE::free(npc->headRelatedData);
		npc->headRelatedData = nullptr;
	}
	if (appliedAllocations.headParts && npc->headParts == appliedAllocations.headParts) {
		RE::free(npc->headParts);
		npc->headParts = nullptr;
		npc->numHeadParts = 0;
	}
	if (appliedAllocations.faceData && npc->faceData == appliedAllocations.faceData) {
		RE::free(npc->faceData);
		npc->faceData = nullptr;
	}
	appliedAllocations = {};
}

void NPCAppearance::InitializeNPCData(NPCData* a_data)
{
	a_data->baseNPC = npc;
	a_data->faceNPC = npc->faceNPC ? npc->faceNPC : npc;
	a_data->race = npc->race;

	a_data->skin = npc->skin;
	a_data->farSkin = npc->farSkin;

	a_data->weight = npc->weight;
	a_data->height = npc->height;
	a_data->sex = npc->GetSex();

	a_data->bodyTintColor = npc->bodyTintColor;

	a_data->tintLayers = utils::CopyTintLayers(npc->tintLayers);
	CopyFaceData(a_data);
	a_data->skeletonModel = &npc->race->skeletonModels[npc->GetSex()];

	a_data->isBeastRace = npc->HasKeywordID(constants::Keyword_IsBeastRace) ||
	                         npc->race->HasKeywordID(constants::Keyword_IsBeastRace);

	a_data->bodyPartData = npc->race->bodyPartData;

	a_data->bodyTextureModel = &npc->race->bodyTextureModels[npc->GetSex()];
	a_data->behaviorGraph = &npc->race->behaviorGraphs[npc->GetSex()];

}

void NPCAppearance::CopyFaceData(NPCData* a_data)
{
	auto* faceSource = utils::GetRootFaceNPCSafe(a_data->faceNPC);
	if (!faceSource || !faceSource->race) {
		faceSource = npc;
	}

	RE::free(a_data->headRelatedData);
	a_data->headRelatedData = utils::CopyHeadRelatedData(faceSource->headRelatedData);

	RE::free(a_data->headParts);
	a_data->numHeadParts = faceSource->numHeadParts > 0 ?
	                           static_cast<std::uint8_t>(faceSource->numHeadParts) :
	                           0;
	a_data->headParts = utils::CopyHeadParts(faceSource->headParts, a_data->numHeadParts);

	// TODO: Check for default face struct here?
	RE::free(a_data->faceData);
	a_data->faceData = utils::DeepCopyFaceData(faceSource->faceData);

	a_data->faceRelatedData = faceSource->race->faceRelatedData[npc->GetSex()];
}

void NPCAppearance::SetupNewAppearance() {
	RaceSwap::applySwap(&alteredNPCData, config->otherRace);
	NPCSwap::applySwap(&alteredNPCData, config->otherNPC);
	SexSwap::applySwap(&alteredNPCData, config->otherSex);
	// TODO add more swaps here
}


NPCAppearance::NPCAppearance(
	RE::TESNPC* a_npc,
	std::unique_ptr<AppearanceConfiguration> a_config) :
	npc(a_npc),
	config(std::move(a_config))
{
	logger::info("	Creating new NPC data");
	InitializeNPCData(&originalNPCData);
	InitializeNPCData(&alteredNPCData);
	logger::info("	Setting up appearance");
	SetupNewAppearance();
}

static void ClearNPCAppearanceData(NPCAppearance::NPCData& a_data) {
	RE::free(a_data.faceData);
	a_data.faceData = nullptr;
	RE::free(a_data.headParts);
	a_data.headParts = nullptr;
	a_data.numHeadParts = 0;
	RE::free(a_data.headRelatedData);
	a_data.headRelatedData = nullptr;
	utils::FreeTintLayers(a_data.tintLayers);
}

NPCAppearance::~NPCAppearance()
{
	ClearNPCAppearanceData(alteredNPCData);
	ClearNPCAppearanceData(originalNPCData);
}

// Filter for only NPCs this swapping can work on
static bool IsNPCValid(RE::TESNPC* a_npc)
{
	return a_npc && a_npc->race &&
	       !a_npc->IsPlayer() &&
	       !a_npc->IsPreset() /* &&
	       a_npc->race->HasKeywordID(constants::Keyword_ActorTypeNPC) */;
}

// Gets or create a new NPC appearance. Will be null if NPC has no altered appearance to take
NPCAppearance::Ptr NPCAppearance::GetOrCreateNPCAppearance(RE::TESNPC* a_npc) {
	if (!IsNPCValid(a_npc)) {
		return nullptr;
	}
	auto faceNPC = utils::GetRootFaceNPCSafe(a_npc);
	if (!faceNPC || !faceNPC->race) {
		return nullptr;
	}
	const std::lock_guard lock(appearanceMapLock);
	if (const auto found = appearanceMap.find(a_npc->formID); found != appearanceMap.end()) {
		return found->second;
	}

	// Template actors are based on a face NPC. Always use face NPC as original appearance to get configuration for
	auto config = ConfigurationDatabase::GetSingleton()->GetConfigurationForNPC(faceNPC); 

	if (config == nullptr) {
		logger::debug("No appearance config for {:x} face NPC: {:x}", a_npc->formID, faceNPC->formID);
		return nullptr;
	}

	logger::debug("NPC {:x} matched entry \"{}\" from file \"{}\"", a_npc->formID, config->entry, config->file);
	logger::info("Creating new appearance for {:x}. Face NPC used for appearance: {:x}", a_npc->formID, faceNPC->formID);
	Ptr appearance(new NPCAppearance(a_npc, std::move(config)));
	appearanceMap.emplace(a_npc->formID, appearance);
	return appearance;
};

// Templated actors rely on the face NPC for swaps, so our appearance data will be based on the faceNPC as well
NPCAppearance::Ptr NPCAppearance::GetNPCAppearance(RE::TESNPC* a_npc) {
	if (!a_npc) {
		return nullptr;
	}
	const std::lock_guard lock(appearanceMapLock);
	if (const auto found = appearanceMap.find(a_npc->formID); found != appearanceMap.end()) {
		return found->second;
	}
	return nullptr;
};

void NPCAppearance::EraseNPCAppearance(RE::TESNPC* a_npc) {
	if (a_npc) {
		EraseNPCAppearance(a_npc->formID);
	}
};

void NPCAppearance::EraseNPCAppearance(RE::FormID a_formID)
{
	Ptr removed;
	{
		const std::lock_guard lock(appearanceMapLock);
		if (const auto found = appearanceMap.find(a_formID); found != appearanceMap.end()) {
			removed = std::move(found->second);
			appearanceMap.erase(found);
		}
	}
};
