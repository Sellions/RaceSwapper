#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct LoadTESObjectARMOHook
{
	// We swap the race being passed to be what the NPC's new appearance is
	static void thunk(RE::TESObjectARMO* a_armor, RE::TESRace* a_race, RE::BipedAnim** a_anim, bool isFemale)
	{
		if (!a_armor || !a_race || !a_anim || !*a_anim) {
			return;
		}
		const auto reference = (*a_anim)->actorRef.get();
		const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
		if (!actor) {
			logger::warn("LoadTESObjectARMOHook: Failed to grab actor; using Skyrim's original armor path");
			return func(a_armor, a_race, a_anim, isFemale);
		}

		if (utils::IsRaceWerewolfOrVampire(a_race)) {
			// Ignore werewolve and vampire form
			return AttachToBiped(a_armor, actor, a_race, a_anim, isFemale);
		}
		auto race = a_race;
		auto loadFemale = isFemale;

		auto NPC = actor->GetActorBase();
		if (!NPC) {
			return AttachToBiped(a_armor, actor, a_race, a_anim, isFemale);
		}

		logger::debug("LoadTESObjectARMOHook: Loading {} {:x} for NPC {} {:x}",
			utils::GetFormEditorID(a_armor), a_armor->formID,
			utils::GetFormEditorID(NPC), NPC->formID);

		const auto appearance = NPCAppearance::GetNPCAppearance(NPC);
		if (appearance != nullptr && appearance->isNPCSwapped) {
			// Swap to new appearance's race
			race = appearance->alteredNPCData.race;
			if (appearance->alteredNPCData.sex == RE::SEX::kMale ||
				appearance->alteredNPCData.sex == RE::SEX::kFemale) {
				loadFemale = appearance->alteredNPCData.sex == RE::SEX::kFemale;
			}
		}
		return AttachToBiped(a_armor, actor, race, a_anim, loadFemale);
	}

	using BipedObjectSlot = REX::EnumSet<RE::BGSBipedObjectForm::BipedObjectSlot, std::uint32_t>;
	
		
	// Overwrite TESObjectARMO::AttachToBiped functionality. This hook will let us load the armor with
	// the closest valid race
	static void AttachToBiped(RE::TESObjectARMO* a_armor, RE::Actor* a_actor, RE::TESRace* a_race, RE::BipedAnim** a_anim, bool isFemale)
	{
		RE::TESRace* race = utils::GetValidRaceForArmorRecursive(a_armor, a_race);
		if (!race) {
			// Race can't load anything from this armor
			logger::warn("Race {} {:x} cannot load armor {} {:x}",
				utils::GetFormEditorID(a_race), a_race->formID,
				utils::GetFormEditorID(a_armor), a_armor->formID);

			return;
		}

		std::scoped_lock lock(armorSlotLock);

		// Preserve SOS slot
		BipedObjectSlot ppSlot = a_armor->bipedModelData.bipedObjectSlots & RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary;
		const auto origSlots = a_armor->bipedModelData.bipedObjectSlots;
		struct RestoreSlots
		{
			RE::TESObjectARMO* armor;
			BipedObjectSlot slots;
			~RestoreSlots() { armor->bipedModelData.bipedObjectSlots = slots; }
		} restoreSlots{ a_armor, origSlots };

		a_armor->bipedModelData.bipedObjectSlots = GetCorrectBipedSlots(a_armor, race);

		// Preserver SOS slot for TNG and similar mods
		if (ppSlot == RE::BGSBipedObjectForm::BipedObjectSlot::kNone) {
			// Remove SOS slot since it was never present
			a_armor->bipedModelData.bipedObjectSlots.reset(RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);
		} else {
			// Keep SOS slot since it was present before corrections
			a_armor->bipedModelData.bipedObjectSlots.set(RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary);
		}

		logger::debug("Loading {:x} as race {} {:x} with new slots {:x}, old slots {:x}",
			a_armor->formID,
			utils::GetFormEditorID(race),
			race->formID,
			a_armor->bipedModelData.bipedObjectSlots.underlying(),
			origSlots.underlying());

		for (auto addon : a_armor->armorAddons) {
			if (addon && (addon->race == race || utils::is_amongst(addon->additionalRaces, race))) {
				AddToBiped(addon, a_armor, a_anim, isFemale);
			}
		}
	}

	// Take the armor's biped slots, and remove the slots that no valid addon for the race supports
	static BipedObjectSlot GetCorrectBipedSlots(RE::TESObjectARMO* a_armor, RE::TESRace* a_race)
	{
		BipedObjectSlot addonSlots = RE::BGSBipedObjectForm::BipedObjectSlot::kNone;
		BipedObjectSlot armorSlots = a_armor->bipedModelData.bipedObjectSlots;
		for (auto addon : a_armor->armorAddons) {
			if (addon && (addon->race == a_race || utils::is_amongst(addon->additionalRaces, a_race))) {
				addonSlots |= addon->bipedModelData.bipedObjectSlots;
			}
		}

		return armorSlots & addonSlots;
	}

	static void AddToBiped(RE::TESObjectARMA* a_addon, RE::TESObjectARMO* a_armor, RE::BipedAnim** a_anim, bool isFemale)
	{
		addToBiped(a_addon, a_armor, a_anim, isFemale);
	}

	// TESObjectARMO::AddToBiped(...)
	static inline REL::Relocation<decltype(thunk)> func;

	// TESObjectARMO::AddToBiped(...)
	static inline REL::Relocation<decltype(AddToBiped)> addToBiped;

	static inline std::recursive_mutex armorSlotLock;

	// Install our hook at the specified address
	static inline void Install()
	{
		if (GetModuleHandleA("DynamicArmorVariants") || GetModuleHandleA("DeviousDevices")) {
			stl::report_and_fail(
				"This RaceSwapper 1.7.104 build does not support Dynamic Armor Variants or Devious Devices NG.");
		}

		addToBiped = { REL::ID(17759) };
		stl::require_bytes(addToBiped.address(), { 0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55 }, "TESObjectARMA::AddToBiped");

		REL::Relocation<std::uintptr_t> target{ REL::ID(24736), 0x302 };
		stl::write_thunk_call<LoadTESObjectARMOHook>(target.address(), REL::ID(17792), "LoadTESObjectARMO main");
		logger::info("LoadTESObjectARMOHook hooked at address {:x}", target.address());
		logger::info("LoadTESObjectARMOHook hooked at offset {:x}", target.offset());

		REL::Relocation<std::uintptr_t> target2{ REL::ID(24737), 0x78 };
		stl::write_thunk_call<LoadTESObjectARMOHook>(target2.address(), REL::ID(17792), "LoadTESObjectARMO secondary");

		REL::Relocation<std::uintptr_t> target3{ REL::ID(24741), 0xEE };
		stl::write_thunk_call<LoadTESObjectARMOHook>(target3.address(), REL::ID(17792), "LoadTESObjectARMO inventory");


		logger::info("LoadTESObjectARMOHook hooked at address {:x}", target2.address());
		logger::info("LoadTESObjectARMOHook hooked at offset {:x}", target2.offset());

		logger::info("LoadTESObjectARMOHook hooked at address {:x}", target3.address());
		logger::info("LoadTESObjectARMOHook hooked at offset {:x}", target3.offset());
	}
};
