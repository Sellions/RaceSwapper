#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct GetTESModelHook
{
	static RE::TESModel* GetTESModel(RE::TESNPC* a_npc)
	{
		if (!a_npc || !a_npc->race) {
			return nullptr;
		}
		if (utils::IsRaceWerewolfOrVampire(a_npc->GetRace())) {
			// Ignore werewolve and vampire form
			return OriginalModel(a_npc);
		}

		const auto appearance = NPCAppearance::GetNPCAppearance(a_npc);
		if (appearance != nullptr && appearance->isNPCSwapped) {
			return appearance->alteredNPCData.skeletonModel;
		}

		return OriginalModel(a_npc);
	}

	static RE::TESModel* OriginalModel(RE::TESNPC* a_npc)
	{
		if (!a_npc || !a_npc->race) {
			return nullptr;
		}
		// Original logic
		if (!a_npc->race->skeletonModels[a_npc->GetSex()].model.empty()) {
			return &a_npc->race->skeletonModels[a_npc->GetSex()];
		} else {
			return a_npc->race->skeletonModels;
		}
	}

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(19749), 0x6B };
		stl::require_bytes(target.address(), {
			0xE8, 0x60, 0x12, 0x0C, 0x00,
			0x48, 0x63, 0xC8,
			0x48, 0x8D, 0x04, 0x8D, 0x13, 0x00, 0x00, 0x00,
			0x48, 0x03, 0xC1,
			0x48, 0x8D, 0x1C, 0xC5, 0x00, 0x00, 0x00, 0x00,
			0x48, 0x03, 0xDD,
			0x48, 0x8D, 0x4B, 0x08,
			0xE8, 0x0E, 0x27, 0xBC, 0x00,
			0x85, 0xC0,
			0x75, 0x07,
			0x48, 0x8D, 0x9D, 0x98, 0x00, 0x00, 0x00
		}, "GetTESModel");

		// Steam 1.7.104 inlines the gender and skeleton selection here. Replace the
		// full sequence and leave its expected model pointer in RBX.
		REL::safe_fill(target.address(), REL::NOP, 0x32);

		auto& trampoline = SKSE::GetTrampoline();
		trampoline.write_call<5>(target.address(), reinterpret_cast<uintptr_t>(GetTESModel));

		const std::array fixReturnValue{ std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0xC3 } };  // mov rbx, rax
		REL::safe_write(target.address() + 0x5, fixReturnValue.data(), fixReturnValue.size());

		logger::info("GetTESModelHook hooked at address {:x}", target.address());
		logger::info("GetTESModelHook hooked at offset {:x}", target.offset());
	}
};
