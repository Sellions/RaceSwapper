#pragma once
#include "Utils.h"
#include "swap/NPCAppearance.h"

struct HasOverlaysHook
{
	struct ASMJmp : Xbyak::CodeGenerator
	{
		ASMJmp(std::uintptr_t func, std::uintptr_t jmpAddr)
		{
			Xbyak::Label funcLabel;

			mov(rcx, rbx);  // Moves the TESNPC to the first argument for our thunk.
			sub(rsp, 0x20);
			call(ptr[rip + funcLabel]);
			add(rsp, 0x20);
			mov(rcx, jmpAddr);
			jmp(rcx);

			L(funcLabel);
			dq(func);
		}
	};

	// When actor has an altered race (eg: Nord -> NordVampire) we will tell the game
	// the actor is NOT altered when it comes to RaceSwapper. This will ensure the actor
	// always uses the headparts we give to it and not from a pre-populated list
	static bool thunk(RE::TESNPC* a_npc)
	{
		if (!a_npc) {
			return false;
		}
		auto originalResult = a_npc->originalRace && a_npc->originalRace != a_npc->race;

		auto appearance = NPCAppearance::GetNPCAppearance(a_npc);
		if (appearance && appearance->isNPCSwapped) {
			return false;  // Tell Skyrim NPC is NOT altered, so it will use our headparts
		} else {
			return originalResult;
		}
	}

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(24790), 0xC9 };
		std::uintptr_t start = target.address();
		// 1.7.104's shared success epilogue starts 0x17 bytes after this site.
		// Returning at +0x1D enters `xor al, al` and discards the thunk result.
		std::uintptr_t end = target.address() + 0x17;
		stl::require_bytes(start, {
			0x48, 0x8B, 0x83, 0xE8, 0x01, 0x00, 0x00,
			0x48, 0x85, 0xC0,
			0x74, 0x11,
			0x48, 0x3B, 0x83, 0x58, 0x01, 0x00, 0x00,
			0x74, 0x08,
			0xB0, 0x01
		}, "HasOverlays");
		stl::require_bytes(end, { 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3 }, "HasOverlays continuation");
		REL::safe_fill(start, REL::NOP, end - start);

		auto jmp = ASMJmp((std::uintptr_t)thunk, end);
		auto& trampoline = SKSE::GetTrampoline();
		auto result = trampoline.allocate(jmp);
		trampoline.write_branch<5>(start, (std::uintptr_t)result);

		logger::info("HasOverlaysHook hooked at address {:x}", target.address());
		logger::info("HasOverlaysHook hooked at offset {:x}", target.offset());
	}
};
