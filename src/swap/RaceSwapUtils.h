#pragma once
#include "PCH.h"
#include <optional>
#include <functional>
#include <string>
#include <unordered_map>
#include "Utils.h"

namespace raceutils
{
	template <class T>
	T random_pick(const std::vector<T>& a_items, std::size_t a_random)
	{
		if (a_items.empty()) {
			return T{};
		}
		return a_items[a_random % a_items.size()];
	}

	template <class T>
	T random_pick(const RE::BSTArray<T>& a_items, std::size_t a_random)
	{
		if (a_items.empty()) {
			return T{};
		}
		return a_items[a_random % a_items.size()];
	}

	using HDPTData = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;

	using HeadpartData = std::pair<RE::BGSHeadPart*, HDPTData*>;

	using SkinTextureData = std::uint32_t;
		
	using _likelihood_t = uint16_t;

	HDPTData ExtractKeywords(RE::BGSHeadPart* hdpt);

	_likelihood_t _match(HDPTData dst, HDPTData src);

	std::vector<RE::BGSHeadPart*> MatchHDPTData(HDPTData dst, const std::vector<HeadpartData>& src_hdpts);

	SkinTextureData ExtractKeywords(RE::BGSTextureSet* hdpt);

	_likelihood_t _match(SkinTextureData dst, SkinTextureData src);
	std::vector<RE::BGSTextureSet*> MatchSkinTextureData(
		SkinTextureData dst,
		const std::vector<RE::BGSTextureSet*>& src_hdpts,
		const std::vector<SkinTextureData>& src_data);

	std::string GetHeadPartTypeAsName(RE::BGSHeadPart::HeadPartType a_type);

	RE::BGSColorForm* GetClosestColorForm(RE::BGSColorForm* a_colorForm, RE::BSTArray<RE::BGSColorForm*>* a_colors);

	std::optional<std::uint16_t> GetClosestPresetIdx(RE::Color a_color, const RE::TESRace::FaceRelatedData::TintAsset::Presets& a_presets);

	/* 
	Class for random number generation based on a TESForm.
	Example:
		util::RandomGen<RE::TESForm> generator(some_form_ptr, util::UniqueStringFromForm);
		auto hash = generator(0)
		auto random1 = generator.GetNext();
		auto random2 = generator.GetStableRandom(1);
		return random1 == random2 //true
	*/
	class RandomGen
	{
	public:
		explicit RandomGen(RE::TESForm* a_item_seed) :
			_hash_seed(utils::HashForm(a_item_seed))
		{}

		[[nodiscard]] size_t GetHashSeed() const {
			return _hash_seed;
		}

		//@brief Get the Nth random number generated from the hash seed.
		size_t GetStableRandom(std::uint32_t n_th_random = 1)
		{
			if (n_th_random == 0) {
				_stream = 0;
				return _hash_seed;
			}
			_stream = n_th_random;
			return static_cast<size_t>(utils::StableRandom(_hash_seed, n_th_random - 1));
		}

		//@brief Get the next random number generated from the previous random number.
		size_t GetNext()
		{
			return static_cast<size_t>(utils::StableRandom(_hash_seed, _stream++));
		}

		//@brief Get the Nth random number generated from the hash seed.
		size_t operator()(unsigned int n_th_random)
		{
			return GetStableRandom(n_th_random);
		}

	private:
		RandomGen() = delete;

		size_t _hash_seed;
		std::uint64_t _stream{ 0 };
	};
}
