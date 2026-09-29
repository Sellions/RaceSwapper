#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct GetBaseMoveTypes
{
	static RE::BGSMovementType* thunk(RE::Actor* a_actor, std::int32_t a_type)
	{
		if (!a_actor) {
			return nullptr;
		}

		auto* race = a_actor->GetRace();
		if (!race) {
			return nullptr;
		}

		if (!utils::IsRaceWerewolfOrVampire(race)) {
			const auto appearance = NPCAppearance::GetNPCAppearance(a_actor->GetActorBase());
			if (appearance && appearance->isNPCSwapped && appearance->alteredNPCData.race) {
				race = appearance->alteredNPCData.race;
			}
		}

		// Preserve the game's fallback to the default movement type when the race's
		// requested slot is empty.
		return func(race, a_type);
	}

	using func_t = RE::BGSMovementType*(RE::TESRace*, std::int32_t);
	static inline REL::Relocation<func_t> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		auto patchArgument = [](std::uintptr_t a_address,
		                         std::initializer_list<std::uint8_t> a_expected,
		                         std::initializer_list<std::uint8_t> a_replacement,
		                         std::string_view a_name) {
			stl::require_bytes(a_address, a_expected, a_name);
			REL::safe_fill(a_address, REL::NOP, a_expected.size());
			REL::safe_write(a_address, a_replacement.begin(), a_replacement.size());
		};

		// These are the five Steam 1.7.104 call sites that resolve an Actor's
		// movement type while constructing or updating its 3D. Each originally
		// replaces RCX with Actor::race immediately before calling ID 25307. Keep
		// the Actor in RCX so the hook can select the appearance race instead.
		REL::Relocation<std::uintptr_t> site1{ REL::ID(37450), 0xCB };
		patchArgument(site1.address() - 0x9,
			{ 0x48, 0x8B, 0x8B, 0xF8, 0x01, 0x00, 0x00 },
			{ 0x48, 0x89, 0xD9 },  // mov rcx, rbx
			"GetBaseMoveTypes argument 1");
		stl::write_thunk_call<GetBaseMoveTypes>(site1.address(), REL::ID(25307), "GetBaseMoveTypes call 1");

		REL::Relocation<std::uintptr_t> site2{ REL::ID(37601), 0x30 };
		patchArgument(site2.address() - 0x3,
			{ 0x48, 0x8B, 0xCE },
			{ 0x48, 0x89, 0xF9 },  // mov rcx, rdi
			"GetBaseMoveTypes argument 2");
		stl::write_thunk_call<GetBaseMoveTypes>(site2.address(), REL::ID(25307), "GetBaseMoveTypes call 2");

		REL::Relocation<std::uintptr_t> site3{ REL::ID(37942), 0x1C };
		patchArgument(site3.address() - 0xC,
			{ 0x48, 0x8B, 0x89, 0xF8, 0x01, 0x00, 0x00 },
			{ 0x48, 0x89, 0xD9 },  // mov rcx, rbx
			"GetBaseMoveTypes argument 3");
		stl::write_thunk_call<GetBaseMoveTypes>(site3.address(), REL::ID(25307), "GetBaseMoveTypes call 3");

		REL::Relocation<std::uintptr_t> site4{ REL::ID(37945), 0xCB };
		patchArgument(site4.address() - 0xE,
			{ 0x48, 0x8B, 0x8E, 0xF8, 0x01, 0x00, 0x00 },
			{ 0x48, 0x89, 0xF1 },  // mov rcx, rsi
			"GetBaseMoveTypes argument 4");
		stl::write_thunk_call<GetBaseMoveTypes>(site4.address(), REL::ID(25307), "GetBaseMoveTypes call 4");

		REL::Relocation<std::uintptr_t> site5{ REL::ID(38029), 0xEB };
		patchArgument(site5.address() - 0xE,
			{ 0x48, 0x8B, 0x8E, 0xF8, 0x01, 0x00, 0x00 },
			{ 0x48, 0x89, 0xF1 },  // mov rcx, rsi
			"GetBaseMoveTypes argument 5");
		stl::write_thunk_call<GetBaseMoveTypes>(site5.address(), REL::ID(25307), "GetBaseMoveTypes call 5");

		logger::info("GetBaseMoveTypes hooked at five verified Steam 1.7.104 call sites");
	}
};
