#pragma once
#include <memory>
#include "configuration/Configuration.h"

class NPCAppearance
{
public:
	using Ptr = std::shared_ptr<NPCAppearance>;

	// Appearance information owned by RaceSwapper and used to swap and revert.
	// This is a data snapshot, not a binary overlay of TESNPC.
	struct NPCData
	{
		RE::TESNPC* baseNPC{ nullptr };
		RE::TESModel* skeletonModel{ nullptr };
		RE::SEX sex{ RE::SEX::kNone };
		bool isBeastRace{ false };
		RE::TESObjectARMO* skin{ nullptr };
		RE::TESObjectARMO* farSkin{ nullptr };
		RE::TESRace::FaceRelatedData* faceRelatedData{ nullptr };
		RE::BGSBodyPartData* bodyPartData{ nullptr };
		RE::BGSTextureModel* bodyTextureModel{ nullptr };
		RE::BGSBehaviorGraphModel* behaviorGraph{ nullptr };
		RE::TESNPC::HeadRelatedData* headRelatedData{ nullptr };
		RE::TESRace* race{ nullptr };
		RE::TESNPC* faceNPC{ nullptr };
		float height{ 1.0F };
		float weight{ 0.0F };
		RE::BGSHeadPart** headParts{ nullptr };
		std::uint8_t numHeadParts{ 0 };
		RE::Color bodyTintColor{};
		RE::TESNPC::FaceData* faceData{ nullptr };
		RE::BSTArray<RE::TESNPC::Layer*>* tintLayers{ nullptr };
	};

	NPCData originalNPCData{};
	NPCData alteredNPCData{};

	bool isNPCSwapped = false;

	bool isSwapDisabled = false; 

	bool ApplyNewAppearance(bool updateLoadedActors = false);

	bool RevertNewAppearance(bool updateLoadedActors = false);

	static Ptr GetOrCreateNPCAppearance(RE::TESNPC* a_npc);

	static Ptr GetNPCAppearance(RE::TESNPC* a_npc);

	static void EraseNPCAppearance(RE::TESNPC* a_npc);

	static void EraseNPCAppearance(RE::FormID a_formID);

	~NPCAppearance();

	TES_HEAP_REDEFINE_NEW();

private:
	struct AppliedAllocations
	{
		RE::TESNPC::HeadRelatedData* headRelatedData{ nullptr };
		RE::BGSHeadPart** headParts{ nullptr };
		RE::TESNPC::FaceData* faceData{ nullptr };
		RE::BSTArray<RE::TESNPC::Layer*>* tintLayers{ nullptr };
	};

	RE::TESNPC* npc{ nullptr };

	std::unique_ptr<AppearanceConfiguration> config;

	AppliedAllocations appliedAllocations;

	NPCAppearance(RE::TESNPC* a_npc, std::unique_ptr<AppearanceConfiguration> a_config);

	void InitializeNPCData(NPCData* a_data);

	void SetupNewAppearance();

	void CopyFaceData(NPCData* a_data);

	void ApplyAppearance(NPCData* a_data);

	void ReleasePreviousAppliedAllocations();

	static inline std::recursive_mutex appearanceMapLock;

	static inline std::map<RE::FormID, Ptr> appearanceMap;
};
