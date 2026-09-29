// Tests the production parser/selection helpers without loading Skyrim or SKSE.
#include "configuration/ConfigParsing.h"
#include "configuration/WeightedSelection.h"
#include "DeterministicRandom.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
	int checks = 0;
	void Check(bool condition, const char* label)
	{
		++checks;
		if (!condition) {
			throw std::runtime_error(label);
		}
	}
}

int main()
{
	using namespace configparse;
	try {
		Check(Trim(" \t\r\n").empty(), "whitespace-only lines");
		Check(Trim("\tNordRace \r") == "NordRace", "trim Windows line endings");
		Check(!SplitSections("# comment"), "comment-only lines");
		Check(!SplitSections("  "), "empty lines");
		Check(!SplitSections("swap=Male match=NordRace"), "swap before match at offset zero");
		Check(!SplitSections("match=NordRace exclude=Player swap=Male"), "exclude before swap");
		Check(!SplitSections("match=NordRace swap=Male exclude="), "empty exclusion is invalid");
		Check(!SplitSections("match= swap=Male"), "empty match");
		Check(!SplitSections("match=NordRace swap="), "empty swap");
		Check(!SplitSections("match=NordRace swap=Male swap=Female"), "duplicate swap");
		Check(!SplitSections("match=NordRace match=BretonRace swap=Male"), "duplicate match");
		Check(!SplitSections("match=NordRace swap=Male exclude=Player exclude=Nazeem"), "duplicate exclude");
		const auto rule = SplitSections(" \tmatch=NordRace | 50%\tswap=KhajiitRace | 100 exclude=Player\r # comment");
		Check(rule.has_value(), "valid rule with whitespace/comments");
		Check(rule->match == "match=NordRace | 50%", "match section boundaries");
		Check(rule->swap == "swap=KhajiitRace | 100", "swap section boundaries");
		Check(rule->exclude == "exclude=Player", "exclude section boundaries");
		const auto simple = SplitSections("match=NordRace swap=Male # swap=ignored");
		Check(simple && simple->exclude.empty(), "optional exclusion and commented keywords");

		Check(Unsigned(" 0x0013746 ", 16) == 0x13746U, "hex form ID");
		Check(Unsigned("0XFF", 16) == 255U, "uppercase hex prefix");
		Check(Unsigned("13746", 16) == 0x13746U, "bare hex form ID");
		Check(Unsigned("0") == 0U, "zero is a valid value");
		Check(Unsigned("4294967295") == std::numeric_limits<std::uint32_t>::max(), "maximum uint32");
		for (const auto bad : { "", "-1", "+1", "100junk", "4294967296", "1.5", "  \t" }) {
			Check(!Unsigned(bad), "malformed unsigned number rejected");
		}
		for (const auto bad : { "0x", "0x123oops", "100000000", "-0x1" }) {
			Check(!Unsigned(bad, 16), "malformed/overflowing hex rejected");
		}
		Check(Percentage("0%") == 0U, "zero-percent rule");
		Check(Percentage(" 50%\r") == 50U, "trim percentage");
		Check(Percentage("999%") == 100U, "clamp percentage to 100");
		for (const auto bad : { "-1%", "50%junk", "%", "5%%", "4294967296%", "50" }) {
			Check(!Percentage(bad), "malformed percentage rejected");
		}

		Check(!WeightedIndex({}, 10), "no matching rules");
		Check(!WeightedIndex(std::array<std::uint32_t, 1>{ 0 }, 0), "single zero-weight rule disabled");
		Check(!WeightedIndex(std::array<std::uint32_t, 3>{ 0, 0, 0 }, 999), "all-zero weights avoid division by zero");
		const std::array<std::uint32_t, 5> weights{ 0, 10, 0, 20, 0 };
		std::array<int, 5> selected{};
		for (std::uint64_t roll = 0; roll < 30; ++roll) {
			const auto index = WeightedIndex(weights, roll);
			Check(index.has_value() && *index < selected.size(), "selection remains in bounds");
			++selected[*index];
		}
		Check(selected == std::array<int, 5>{ 0, 10, 0, 20, 0 }, "interval lengths match positive weights");
		Check(WeightedIndex(weights, 30) == 1U, "roll wraps at total");
		const auto max = std::numeric_limits<std::uint32_t>::max();
		const std::array<std::uint32_t, 2> large{ max, max };
		Check(TotalWeight(large) == 8589934590ULL, "cumulative weight does not overflow uint32");
		Check(WeightedIndex(large, max) == 1U, "selection across uint32 boundary");

		using deterministic_random::Generate;
		using deterministic_random::HashString;
		Check(HashString("") == 14695981039346656037ULL, "FNV-1a empty vector");
		Check(HashString("RaceSwapper") == 0xBFDC4E63CAADAA2AULL, "FNV-1a stable vector");
		Check(HashString("RaceSwapper") == HashString("RaceSwapper"), "hash is repeatable");
		Check(HashString("RaceSwapper") != HashString("raceswapper"), "hash distinguishes input");
		Check(Generate(0, 0) == 0xE220A8397B1DCDAFULL, "SplitMix64 stable vector");
		Check(Generate(42, 0) == Generate(42, 0), "random output is repeatable");
		Check(Generate(42, 0) != Generate(42, 1), "random streams are distinct");
		std::cout << checks << " portable regression checks passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "Regression failed: " << error.what() << '\n';
		return 1;
	}
}
