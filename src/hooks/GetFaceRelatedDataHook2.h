#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct GetFaceRelatedDataHook2
{
	static void thunk(std::uint64_t a_unk, std::uint64_t a_unk1, std::uint64_t a_unk2, RE::TESNPC* a_npc)
	{
		if (!a_npc || !a_npc->race) {
			func(a_unk, a_unk1, a_unk2, a_npc);
			return;
		}
		if (utils::IsRaceWerewolfOrVampire(a_npc->race)) {
			// Ignore werewolve and vampire form
			func(a_unk, a_unk1, a_unk2, a_npc);
			return;
		}

		const auto appearance = NPCAppearance::GetNPCAppearance(a_npc);
		if (!appearance || !appearance->isNPCSwapped) {
			func(a_unk, a_unk1, a_unk2, a_npc);
			return;
		}

		// The called engine routine reads the FaceRelatedData through TESRace. Keep
		// this temporary shared mutation serialized and restore it on every exit.
		const std::lock_guard lock(temporaryMutationLock);
		auto& faceRelatedData = a_npc->race->faceRelatedData[a_npc->GetSex()];
		const auto original = faceRelatedData;
		faceRelatedData = appearance->alteredNPCData.faceRelatedData;
		const stl::scope_exit restore([&]() { faceRelatedData = original; });
		func(a_unk, a_unk1, a_unk2, a_npc);
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static inline std::recursive_mutex temporaryMutationLock;

	// Install our hook at the specified address
	static inline void Install()
	{
		// TODO: The function is used elsewhere, may need to hook that too?
		REL::Relocation<std::uintptr_t> target{ REL::ID(26837), 0x7D };

		stl::write_thunk_call<GetFaceRelatedDataHook2>(target.address(), REL::ID(26838), "GetFaceRelatedData2");

		logger::info("GetFaceRelatedDataHook2 hooked at address {:x}", target.address());
		logger::info("GetFaceRelatedDataHook2 hooked at offset {:x}", target.offset());
	}
};
