# RaceSwapper: Steam 1.7.104 compatibility review

Reviewed 29 September 2026 (UTC).

## Outcome

The source port to **Steam SkyrimSE 1.7.104.0** is complete at the offline
verification level. Runtime metadata is restricted to 1.7.104, the dependency
stack understands Address Library format 5, and every installed native hook is
guarded by exact byte or target validation derived from the supplied executable.

It is **not yet a tested binary release**. This Linux workspace has no MSVC or
Windows SDK, so no DLL was produced, and Skyrim was not launched. The included
GitHub Actions workflow provides the remaining reproducible Windows build and
package step; an actual run plus the in-game checklist below are still required
before publishing the DLL as release-ready.

Base repository: [Nightfallstorm/RaceSwapper](https://github.com/Nightfallstorm/RaceSwapper),
commit `ce58ac4d4c19e38dc05a5e10a5a935d88594d3b3` (version 1.5.8).
Working branch: `update/steam-1.7.104-audit` (version 1.6.0).

## Compatibility boundary

| Environment | Status |
| --- | --- |
| Steam SkyrimSE 1.7.104.0 | Targeted and byte-audited; Windows build and game test pending |
| SKSE 2.3.1 + `versionlib-1-7-104-0.bin` | Required |
| Earlier/later Steam runtimes | Rejected by plugin metadata and runtime check |
| GOG, Epic, or Skyrim VR | Unsupported |
| Dynamic Armor Variants | Unsupported; detected DLL causes a fail-fast error |
| Devious Devices NG | Unsupported; detected DLL causes a fail-fast error |

The user confirmed that neither unsupported armor-hook mod is installed, so the
ordinary armor path is the intended test configuration.

## Audited inputs

The runtime files were used only for offline verification and are not included
in the source patch or review bundle.

| Input | Verified property |
| --- | --- |
| `SkyrimSE.exe` | PE32+ AMD64, version 1.7.104.0, image base `0x140000000`, SHA-256 `c06a66c7d640458cbdf4b26a389357a8bb37d2aa5165a3f743c2ac91355b9bd9` |
| `versionlib-1-7-104-0.bin` | Address Library format 5, runtime 1.7.104.0, 8-byte pointers, dense format, 565,759 IDs, SHA-256 `8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842` |

`tools/verify_runtime_1_7_104.py` rejects inputs with different hashes before
checking any patch site.

## Dependency and build migration

- CommonLibSSE-NG was updated from commit
  `f77c621758ce61f6b7ece0237c9517aa07332aab` to release **9.2.0**, commit
  `6ef06fb40fc475680f188469f83a162ac64b5039`.
- The submodule URL now points to `alandtse/CommonLibSSE-NG` and includes nested
  submodules.
- C++23, AE-only support, Xbyak, and Address Library format 5 are enabled.
- The vcpkg manifest matches CommonLib's baseline and dependency versions.
- Plugin metadata declares version 1.6.0, requires SKSE 2.3.1, and lists only
  `RUNTIME_SSE_1_7_104` as compatible.
- CMake can stage an installable `Data/SKSE/Plugins/RaceSwapper.dll` layout.
- `.github/workflows/build-steam-1-7-104.yml` performs portable regressions,
  builds Release with Visual Studio 2022, and packages a ZIP plus SHA-256.

## Native patch verification

The verifier confirmed the expected instruction bytes and original call targets
at all inline sites, then confirmed each hooked vtable entry points to the
expected Address Library function. The plugin repeats equivalent checks at
startup and refuses to patch a mismatch.

| Hook | Verified 1.7.104 location |
| --- | --- |
| Get TES model | ID 19749 `+0x6B`, 50-byte overwrite |
| Face-related data | ID 24730 `+0x5A`, 16-byte overwrite |
| Second face path | ID 26837 `+0x7D`, call to ID 26838 |
| Body-part data | ID 37177 `+0x54/+0x57`, argument rewrite and call to ID 25304 |
| Movement types | Five argument/call sites in IDs 37450, 37601, 37942, 37945, and 38029; each calls ID 25307 |
| Armor loading | IDs 24736 `+0x302`, 24737 `+0x78`, and 24741 `+0xEE`; each calls ID 17792 |
| Armor add-on routine | ID 17759 entry bytes |
| Skin loading | ID 15676 `+0x359`, call to ID 17792 |
| Height | ID 24763 entry bytes |
| Set race | ID 37925 `+0xCC`, call to ID 24669 |
| Overlay decision | ID 24790 `+0xC9`; verified 23-byte overwrite and `+0xE0` continuation |
| TESNPC vtable | Destructor, save, load, revert, copy, and secondary copy-from-template entries |
| Character vtable | Keyword and animation-graph entries |
| Tint-layer layout | Allocation at RVA `0x3BCB17` and free at `0x3BC040` both use `0x0C` bytes |

The special tint allocation intentionally uses the runtime's verified 12-byte
size even though CommonLib's C++ structure includes four trailing padding bytes
and has `sizeof == 0x10`.

## Correctness and safety changes

| Area | Result |
| --- | --- |
| Patch safety | Exact instructions, call targets, and vtable functions are checked before every write. CommonLib's generic non-blocking patch check is disabled because these stricter checks are authoritative. |
| Hook ABIs | Void, bool, integer-width, save/load-buffer, and opaque graph arguments were corrected from disassembly and CommonLib declarations. |
| Overlay logic | The branch now continues at the verified success epilogue so the hook's return value is not overwritten. |
| Movement | All five verified 3D construction/update call sites now use the swapped appearance race. |
| Armor and skin | Null handles/add-ons are guarded, armor-parent cycles terminate, shared armor slots are restored with RAII, and the swapped sex is passed to the engine routines. |
| DAV/DDNG | The unsafe third-party instruction-restoration path was removed and replaced with explicit rejection. |
| Appearance ownership | Snapshots own their copied buffers, previously applied buffers are tracked, cached appearances use shared ownership, and erased entries cannot become immediate dangling pointers in active hooks. |
| Shared mutations | Armor slots, face-race data, and animation-graph race changes are serialized and restored on every exit. |
| NPC lifecycle | Copy, template, save, load, revert, form deletion, and destruction paths clear or preserve cached state deliberately. Face-template and armor-parent cycles are detected. |
| Head and tint data | Counts are bounded to the signed 8-bit engine field, allocations are checked, missing presets/data are skipped safely, and tint layers use the verified runtime allocation size. |
| Configuration | Section order, trimming, BOMs, numeric overflow/trailing text, exclusions, male-only targets, zero weights, and regular-file enumeration were corrected. |
| Selection/RNG | Weighted totals use 64 bits and deterministic local hashing/SplitMix64 avoids mutating the game's global C RNG state. |
| Settings | Feature masks no longer overlap, so debug logging cannot silently change randomization or strict-headpart settings. |

## Verification performed here

| Check | Result |
| --- | --- |
| Exact executable and Address Library hashes/headers | Passed |
| Inline instructions and original call targets | Passed |
| Five movement call sites | Passed |
| Eight vtable targets | Passed |
| Runtime tint allocation/free size | Passed (`0x0C`) |
| Portable production-helper regressions | Passed (86 checks) |
| GCC 13.3, C++23, `-Wall -Wextra -Werror -pedantic` | Passed |
| UndefinedBehaviorSanitizer | Passed for portable checks |
| Address/LeakSanitizer | Not claimed; this sandbox denies the `/proc` access LeakSanitizer needs at startup |
| Python verifier syntax and JSON/YAML parsing | Passed |
| Patch whitespace | Passed |
| MSVC/Windows DLL build | Not available in this workspace; automated workflow added but not run here |
| Skyrim startup/gameplay/save testing | Not performed |

Portable tests cover production parser, weighted-selection, and deterministic
RNG helpers. They do not execute SKSE, native hooks, rendering, or engine memory
ownership.

## Required Windows and in-game acceptance test

1. Run the checked-in Windows workflow and confirm both jobs pass and the ZIP
   contains `Data/SKSE/Plugins/RaceSwapper.dll`.
2. Use a separate mod-manager profile containing Steam 1.7.104, SKSE 2.3.1,
   matching Address Library, powerofthree's Tweaks, RaceSwapper, and no DAV/DDNG.
3. Confirm startup reaches `Loaded Plugin` with no patch-mismatch or unsupported
   runtime error in `RaceSwapper.log`.
4. Exercise race, NPC, male/female, custom-race, armor/skin, creature movement,
   vampire/werewolf transformation, and missing-face-data cases.
5. Save and reload without relaunching, then relaunch and load again. Check NPC
   appearance stability, inventory icons, animation graphs, and tint/face/body
   consistency.
6. Test a new game, a disposable existing save, revert/reset paths, and mod
   removal only on a disposable save.

Until that checklist passes, publish the source as a **1.7.104 port candidate**,
not a fully game-tested binary release.
