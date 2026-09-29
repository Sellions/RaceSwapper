#pragma once

#include <cstdint>
#include <string_view>

namespace deterministic_random
{
	inline constexpr std::uint64_t HashString(std::string_view a_value) noexcept
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto character : a_value) {
			hash ^= static_cast<std::uint8_t>(character);
			hash *= 1099511628211ULL;
		}
		return hash;
	}

	inline constexpr std::uint64_t Generate(std::uint64_t a_seed, std::uint64_t a_stream = 0) noexcept
	{
		auto value = a_seed + 0x9E3779B97F4A7C15ULL * (a_stream + 1);
		value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
		value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
		return value ^ (value >> 31);
	}
}
