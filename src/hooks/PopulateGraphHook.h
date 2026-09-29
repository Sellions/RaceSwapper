#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct PopulateGraphHook
{
	static bool thunk(RE::Actor* a_actor, std::uint64_t a_unk1, std::uint64_t a_unk2)
	{
		if (!a_actor) {
			return func(a_actor, a_unk1, a_unk2);
		}
		if (utils::IsRaceWerewolfOrVampire(a_actor->GetRace())) {
			// Ignore werewolve and vampire form
			return func(a_actor, a_unk1, a_unk2);
		}

		auto npc = a_actor->GetActorBase();
		if (!npc) {
			return func(a_actor, a_unk1, a_unk2);
		}
		const auto appearance = NPCAppearance::GetNPCAppearance(npc);
		if (!appearance || !appearance->isNPCSwapped || !appearance->alteredNPCData.race) {
			return func(a_actor, a_unk1, a_unk2);
		}

		const std::lock_guard lock(temporaryMutationLock);
		const auto originalRace = npc->race;
		npc->race = appearance->alteredNPCData.race;
		const stl::scope_exit restore([&]() { npc->race = originalRace; });
		return func(a_actor, a_unk1, a_unk2);
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static inline std::recursive_mutex temporaryMutationLock;

	static inline std::uint32_t idx = 0x72;
	// Install our hook at the specified address
	static inline void Install()
	{
		stl::write_vfunc<RE::Character, PopulateGraphHook>(REL::ID(38058), "Character::PopulateGraph");

		logger::info("PopulateGraphHook hooked!");
	}
};
