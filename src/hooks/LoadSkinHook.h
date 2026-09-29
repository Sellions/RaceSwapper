#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct LoadSkinHook
{
	// Based off 1.5.97 TESObjectARMA::ContainsRace_140226D70(v9[i], a_race)
	// We swap the race being passed to be what the NPC's new appearance is
	// We also swap the armor skin to make sure it is correct
	static void thunk(RE::TESObjectARMO* a_skin, RE::TESRace* a_race, RE::BipedAnim** a_anim, bool isFemale)
	{
		if (!a_skin || !a_anim || !*a_anim) {
			return;
		}
		const auto reference = (*a_anim)->actorRef.get();
		const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
		if (!actor || !actor->GetActorBase()) {
			func(a_skin, a_race, a_anim, isFemale);
			return;
		}
		if (utils::IsRaceWerewolfOrVampire(a_race)) {
			// Ignore werewolve and vampire form
			logger::info("LoadSkin hook called for vampire/werewolf for skin {} {:x}", utils::GetFormEditorID(a_skin), a_skin->formID);
			func(a_skin, a_race, a_anim, isFemale);
			return;
		}

		auto race = a_race;
		auto NPC = actor->GetActorBase();
		auto skin = a_skin;
		auto loadFemale = isFemale;

		const auto appearance = ApplyAppearanceToNPC(NPC);
		if (appearance != nullptr && appearance->isNPCSwapped) {
			// Swap to new appearance's race and skin
			race = appearance->alteredNPCData.race;
			skin = appearance->alteredNPCData.skin ? appearance->alteredNPCData.skin : (race ? race->skin : nullptr);
			if (!skin) {
				func(a_skin, a_race, a_anim, isFemale);
				return;
			}
			if (appearance->alteredNPCData.sex == RE::SEX::kMale ||
				appearance->alteredNPCData.sex == RE::SEX::kFemale) {
				loadFemale = appearance->alteredNPCData.sex == RE::SEX::kFemale;
			}
			logger::debug("LoadSkinHook: Swap occurred!");
		}
		logger::debug("LoadSkinHook: Loading {} {:x} for NPC {} {:x}",
			utils::GetFormEditorID(skin).c_str(), skin->formID,
			utils::GetFormEditorID(NPC).c_str(), NPC->formID);
		func(skin, race, a_anim, loadFemale);
	}

	// This hook seems to be the best earliest hook spot for applying appearance as the NPC loads
	static NPCAppearance::Ptr ApplyAppearanceToNPC(RE::TESNPC* a_npc)
	{
		if (!a_npc) {
			return nullptr;
		}
		logger::debug("Loading {:x} appearance from LoadSkinHook hook", a_npc->formID);

		auto appearance = NPCAppearance::GetOrCreateNPCAppearance(a_npc);

		if (appearance && !appearance->isNPCSwapped) {
			appearance->ApplyNewAppearance(true);
		}

		return appearance;
	}

	// TESObjectARMO::AddToBiped(...)
	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(15676), 0x359 };
		stl::write_thunk_call<LoadSkinHook>(target.address(), REL::ID(17792), "LoadSkin");

		logger::info("LoadSkinHook hooked at address {:x}", target.address());
		logger::info("LoadSkinHook hooked at offset {:x}", target.offset());
	}
};
