#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct SaveNPC
{
	// Revert any swaps before saving to prevent presistence
	static void thunk(RE::TESNPC* a_self, RE::BGSSaveFormBuffer* a_buffer)
	{
		auto appearance = NPCAppearance::GetNPCAppearance(a_self);
		if (!appearance) {
			// No appearance data means no need to revert anything
			return func(a_self, a_buffer);
		}
		bool appliedSwap = appearance->isNPCSwapped;
		if (appearance && appliedSwap) {
			logger::info("Reverting NPC for save: {:x}", a_self->formID);
			appearance->RevertNewAppearance();
		}
		func(a_self, a_buffer);

		if (appearance && appliedSwap) {
			appearance->ApplyNewAppearance();
		}
	}

	static inline REL::Relocation<decltype(thunk)> func;

	static inline std::uint32_t idx = 0xE;

	// Install our hook at the specified address
	static inline void Install()
	{
		stl::write_vfunc<RE::TESNPC, 0, SaveNPC>(REL::ID(24777), "TESNPC::SaveGame");

		logger::info("SaveNPC hook set");
	}
};
