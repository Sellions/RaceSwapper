#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct GetBodyPartDataHook
{
	static RE::BGSBodyPartData* thunk(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}

		if (utils::IsRaceWerewolfOrVampire(a_actor->GetRace())) {
			// Ignore werewolve and vampire form
			return func(a_actor->GetRace());
		}
		logger::debug("Getting body part data for {:x}", a_actor->formID);
		auto appearance = NPCAppearance::GetNPCAppearance(a_actor->GetActorBase());
		if (appearance && appearance->isNPCSwapped) {
			logger::debug("Using new NPC body part data");
			return appearance->alteredNPCData.bodyPartData;
		}

		if (!a_actor->GetRace()) {
			logger::debug("Returning no body part data for {:x}", a_actor->formID);
			return nullptr;
		}

		logger::debug("Returning default body part data for {:x}", a_actor->formID);
		return func(a_actor->GetRace());
	}

	using func_t = RE::BGSBodyPartData*(RE::TESRace*);
	static inline REL::Relocation<func_t> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> load3DTarget{ REL::ID(37177), 0x57 };
		// Remove call to replace RCX (actor) with actor's race. This lets our hook have access to the actor data
		stl::require_bytes(load3DTarget.address() - 0x3, { 0x48, 0x8B, 0xCA }, "GetBodyPartData argument");
		const std::array useActorInstructions{ std::byte{ 0x48 }, std::byte{ 0x89 }, std::byte{ 0xD9 } };  // mov rcx, rbx
		REL::safe_write(load3DTarget.address() - 0x3, useActorInstructions.data(), useActorInstructions.size());

		stl::write_thunk_call<GetBodyPartDataHook>(load3DTarget.address(), REL::ID(25304), "GetBodyPartData call");

		// TODO: May need to hook other areas?
		logger::info("GetBodyPartData hooked at address {:x}", load3DTarget.address());
		logger::info("GetBodyPartData hooked at offset {:x}", load3DTarget.offset());
	}
};
