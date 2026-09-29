#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace configparse
{
	inline std::uint64_t TotalWeight(std::span<const std::uint32_t> weights)
	{
		std::uint64_t total = 0;
		for (const auto weight : weights) {
			total += weight;
		}
		return total;
	}

	inline std::optional<std::size_t> WeightedIndex(std::span<const std::uint32_t> weights, std::uint64_t roll)
	{
		const auto total = TotalWeight(weights);
		if (total == 0) {
			return std::nullopt;
		}
		roll %= total;
		for (std::size_t index = 0; index < weights.size(); ++index) {
			if (roll < weights[index]) {
				return index;
			}
			roll -= weights[index];
		}
		return std::nullopt;
	}
}
