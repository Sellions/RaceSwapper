#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

namespace configparse
{
	inline std::string_view Trim(std::string_view text)
	{
		constexpr auto whitespace = " \t\f\v\n\r";
		const auto first = text.find_first_not_of(whitespace);
		if (first == std::string_view::npos) {
			return {};
		}
		return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
	}

	struct Sections
	{
		std::string_view match;
		std::string_view swap;
		std::string_view exclude;
	};

	// Views refer to the caller's line. Invalid ordering must not broaden a rule.
	inline std::optional<Sections> SplitSections(std::string_view line)
	{
		line = Trim(line.substr(0, line.find('#')));
		if (!line.starts_with("match=")) {
			return std::nullopt;
		}
		const auto swap = line.find("swap=");
		const auto exclude = line.find("exclude=");
		if (swap == std::string_view::npos || swap <= 6 ||
			(exclude != std::string_view::npos && exclude <= swap)) {
			return std::nullopt;
		}
		if (line.find("match=", 6) != std::string_view::npos ||
			line.find("swap=", swap + 5) != std::string_view::npos ||
			(exclude != std::string_view::npos && line.find("exclude=", exclude + 8) != std::string_view::npos)) {
			return std::nullopt;
		}
		const auto matchText = Trim(line.substr(0, swap));
		const auto swapText = Trim(line.substr(swap, exclude == std::string_view::npos ? exclude : exclude - swap));
		const auto excludeText = exclude == std::string_view::npos ? std::string_view{} : Trim(line.substr(exclude));
		if (Trim(matchText.substr(6)).empty() || Trim(swapText.substr(5)).empty() ||
			(!excludeText.empty() && Trim(excludeText.substr(8)).empty())) {
			return std::nullopt;
		}
		return Sections{ matchText, swapText, excludeText };
	}

	inline std::optional<std::uint32_t> Unsigned(std::string_view text, int base = 10)
	{
		text = Trim(text);
		if (base == 16 && (text.starts_with("0x") || text.starts_with("0X"))) {
			text.remove_prefix(2);
		}
		if (text.empty()) {
			return std::nullopt;
		}
		std::uint32_t value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
		if (error != std::errc{} || end != text.data() + text.size()) {
			return std::nullopt;
		}
		return value;
	}

	inline std::optional<std::uint32_t> Percentage(std::string_view text)
	{
		text = Trim(text);
		if (!text.ends_with('%')) {
			return std::nullopt;
		}
		const auto value = Unsigned(text.substr(0, text.size() - 1));
		return value ? std::optional{ std::min(*value, 100U) } : std::nullopt;
	}
}
