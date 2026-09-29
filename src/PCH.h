#pragma once

#define NOMMNOSOUND

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#pragma warning(disable: 4100)

#pragma warning(push)
#include <SimpleIni.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <xbyak/xbyak.h>
#pragma warning(pop)

namespace logger = SKSE::log;

using namespace std::literals;

namespace constants
{
	inline RE::FormID Keyword_ActorTypeNPC = 0x13794;
	inline RE::FormID Keyword_IsBeastRace = 0xD61D1;

	// debug
	inline RE::FormID Maiq = 0x954BF;
	inline RE::FormID Nazeem = 0x13BBF;
	inline RE::FormID Urog = 0x1B078;
	inline RE::FormID MQ101Alduin = 0x32B94;
	inline RE::FormID DefaultRace = 0x19;
	inline RE::FormID DebugNPCToTest = 0x954BF;
	inline RE::FormID KhajiitRace = 0x13745;
	inline RE::FormID ArgonianRace = 0x13740;
	inline RE::FormID RedguardRace = 0x13748;
	inline RE::FormID NordRace = 0x13746;
	inline RE::FormID OrcRace = 0x13747;
	inline RE::FormID CowRace = 0x4E785;
	inline RE::FormID DragonRace = 0x12E82;
	inline RE::FormID BretonRace = 0x13741;
}

namespace stl
{
	using namespace SKSE::stl;

	[[noreturn]] inline void patch_mismatch(std::string_view a_name, std::uintptr_t a_address)
	{
		auto message = fmt::format(
			"RaceSwapper refused to install the '{}' hook because SkyrimSE.exe does not match the verified Steam 1.7.104 instructions at 0x{:X}.",
			a_name,
			a_address);
		logger::critical("{}", message);
		report_and_fail(message);
	}

	inline void require_bytes(
		std::uintptr_t a_address,
		std::initializer_list<std::uint8_t> a_expected,
		std::string_view a_name)
	{
		const auto* actual = reinterpret_cast<const std::uint8_t*>(a_address);
		if (!std::equal(a_expected.begin(), a_expected.end(), actual)) {
			patch_mismatch(a_name, a_address);
		}
	}

	inline void require_address(
		std::uintptr_t a_actual,
		std::uintptr_t a_expected,
		std::string_view a_name,
		std::uintptr_t a_patchAddress)
	{
		if (a_actual != a_expected) {
			logger::critical(
				"Hook '{}' expected target 0x{:X}, but found 0x{:X}",
				a_name,
				a_expected,
				a_actual);
			patch_mismatch(a_name, a_patchAddress);
		}
	}

	inline void require_call(std::uintptr_t a_address, REL::ID a_expected, std::string_view a_name)
	{
		require_bytes(a_address, { 0xE8 }, a_name);

		std::int32_t displacement = 0;
		std::memcpy(std::addressof(displacement), reinterpret_cast<const void*>(a_address + 1), sizeof(displacement));
		const auto actual = static_cast<std::uintptr_t>(
			static_cast<std::intptr_t>(a_address + 5) + displacement);
		const REL::Relocation<std::uintptr_t> expected{ a_expected };
		require_address(actual, expected.address(), a_name, a_address);
	}

	void asm_replace(std::uintptr_t a_from, std::size_t a_size, std::uintptr_t a_to);

	template <class T>
	void asm_replace(std::uintptr_t a_from)
	{
		asm_replace(a_from, T::size, reinterpret_cast<std::uintptr_t>(T::func));
	}

	template <class T>
	void write_thunk_call(std::uintptr_t a_src, REL::ID a_expected, std::string_view a_name)
	{
		require_call(a_src, a_expected, a_name);
		auto& trampoline = SKSE::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, size_t offset, class T>
	void write_vfunc(REL::ID a_expected, std::string_view a_name)
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[offset] };
		const auto current = reinterpret_cast<const std::uintptr_t*>(vtbl.address())[T::idx];
		const REL::Relocation<std::uintptr_t> expected{ a_expected };
		require_address(current, expected.address(), a_name, vtbl.address() + T::idx * sizeof(std::uintptr_t));
		T::func = vtbl.write_vfunc(T::idx, T::thunk);
	}

	template <class F, class T>
	void write_vfunc(REL::ID a_expected, std::string_view a_name)
	{
		write_vfunc<F, 0, T>(a_expected, a_name);
	}

	inline std::string as_string(std::string_view a_view)
	{
		return { a_view.data(), a_view.size() };
	}
}

#ifdef SKYRIM_AE
#	define REL_ID(se, ae) REL::ID(ae)
#	define OFFSET(se, ae) ae
#	define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
#	define REL_ID(se, ae) REL::ID(se)
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) vr
#else
#	define REL_ID(se, ae) REL::ID(se)
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) se
#endif

#define DLLEXPORT __declspec(dllexport)

#include "Version.h"
