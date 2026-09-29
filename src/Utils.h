#pragma once

namespace utils
{
	using _GetFormEditorID = const char* (*)(std::uint32_t);

	// Skyrim 1.7.104 allocates and frees TESNPC tint layers as 0x0C-byte blocks.
	// CommonLib includes an additional trailing padding field in sizeof(Layer), so
	// allocations for objects handed back to the game must use the runtime size.
	inline constexpr std::size_t kTintLayerRuntimeSize = 0x0C;
	static_assert(sizeof(RE::TESNPC::Layer) >= kTintLayerRuntimeSize);
	static_assert(offsetof(RE::TESNPC::Layer, tintColor) == 0x00);
	static_assert(offsetof(RE::TESNPC::Layer, tintIndex) == 0x04);
	static_assert(offsetof(RE::TESNPC::Layer, preset) == 0x06);
	static_assert(offsetof(RE::TESNPC::Layer, interpolationValue) == 0x08);

	RE::TESNPC::Layer* AllocateTintLayer();

	void FreeTintLayers(RE::BSTArray<RE::TESNPC::Layer*>*& a_tintLayers);

	std::string UniqueStringFromForm(RE::TESForm* a_form_seed);

	size_t HashForm(RE::TESForm* a_form_seed);

	std::uint64_t HashString(std::string_view a_value);

	std::uint64_t StableRandom(std::uint64_t a_seed, std::uint64_t a_stream = 0);

	RE::BSTArray<RE::TESNPC::Layer*>* CopyTintLayers(RE::BSTArray<RE::TESNPC::Layer*>* a_tintLayers);

	RE::TESNPC::HeadRelatedData* CopyHeadRelatedData(RE::TESNPC::HeadRelatedData* a_data);

	RE::BGSHeadPart** CopyHeadParts(RE::BGSHeadPart** a_parts, std::uint32_t a_numHeadParts);

	RE::TESNPC::FaceData* DeepCopyFaceData(RE::TESNPC::FaceData* a_faceData);

	RE::TESNPC* GetRootFaceNPCSafe(RE::TESNPC* a_npc);

	bool IsVampire(RE::TESNPC* a_npc);

	RE::TESRace* AsNonVampireRace(RE::TESRace* a_race);

	RE::TESRace* AsVampireRace(RE::TESRace* a_race);

	std::vector<std::string> split_string(std::string& a_string, char a_delimiter);

	std::string GetFormEditorID(const RE::TESForm* a_form);

	bool IsRaceWerewolfOrVampire(RE::TESRace* a_race);

	template <class T>
	bool is_amongst(const std::vector<T>& item_list, const T& elem)
	{
		return std::find(item_list.begin(), item_list.end(), elem) != item_list.end();
	}

	template <class T>
	bool is_amongst(const RE::BSTArray<T>& item_list, const T& elem)
	{
		return std::find(item_list.begin(), item_list.end(), elem) != item_list.end();
	}

	template <class _First_T, class _Second_T>
	bool is_amongst(const std::unordered_map<_First_T, _Second_T>& item_map, const _First_T& elem)
	{
		return item_map.find(elem) != item_map.end();
	}

	RE::TESRace* GetValidRaceForArmorRecursive(RE::TESObjectARMO* a_armor, RE::TESRace* a_race);
}
