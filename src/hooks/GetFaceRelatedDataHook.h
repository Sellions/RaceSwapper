#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct GetFaceRelatedDataHook
{
	static RE::TESRace::FaceRelatedData* GetFaceData(RE::TESNPC* a_npc)
	{
		if (!a_npc || !a_npc->race) {
			return nullptr;
		}
		if (utils::IsRaceWerewolfOrVampire(a_npc->race)) {
			// Ignore werewolve and vampire form
			return a_npc->race->faceRelatedData[a_npc->GetSex()];
		}

		const auto appearance = NPCAppearance::GetNPCAppearance(a_npc);
		if (appearance != nullptr && appearance->isNPCSwapped) {
			return appearance->alteredNPCData.faceRelatedData;
		}

		// Original logic
		return a_npc->race->faceRelatedData[a_npc->GetSex()];
	}

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(24730), 0x5A };
		stl::require_bytes(target.address(), {
			0xE8, 0x91, 0xC4, 0xFE, 0xFF,
			0x48, 0x63, 0xC8,
			0x4D, 0x8B, 0xB4, 0xCE, 0xA8, 0x04, 0x00, 0x00
		}, "GetFaceRelatedData");
		REL::safe_fill(target.address(), REL::NOP, 0x10);  // Fill with NOP
		const std::array newInstructions{ std::byte{ 0x49 }, std::byte{ 0x89 }, std::byte{ 0xC6 } };  // mov r14, rax
		REL::safe_write(target.address() + 0x5, newInstructions.data(), newInstructions.size());

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_call<5>(target.address(), reinterpret_cast<uintptr_t>(GetFaceData));

		logger::info("GetFaceRelatedDataHook hooked at address {:x}", target.address());
		logger::info("GetFaceRelatedDataHook hooked at offset {:x}", target.offset());
	}
};
