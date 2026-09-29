#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct RevertNPC
{
	// Revert all swaps, restore factory settings
	static void thunk(RE::TESNPC* a_self, RE::BGSLoadFormBuffer* a_buffer)
	{
		auto appearance = NPCAppearance::GetNPCAppearance(a_self);
		if (!appearance) {
			// No appearance data means no need to revert anything
			return func(a_self, a_buffer);
		}
		bool appliedSwap = appearance->isNPCSwapped;
		if (appearance && appliedSwap) {
			logger::info("Reverting NPC for revert: {:x}", a_self->formID);
			appearance->RevertNewAppearance();
		}
		func(a_self, a_buffer);
		// Skyrim has now rebuilt the NPC's factory data. Discard the cached copies
		// so the next load derives a fresh appearance from that state.
		NPCAppearance::EraseNPCAppearance(a_self);
	}

	static inline REL::Relocation<decltype(thunk)> func;

	static inline std::uint32_t idx = 0x12;

	// Install our hook at the specified address
	static inline void Install()
	{
		stl::write_vfunc<RE::TESNPC, 0, RevertNPC>(REL::ID(24779), "TESNPC::Revert");

		logger::info("RevertNPC hook set");
	}
};
