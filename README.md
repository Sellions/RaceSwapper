# Race Swapper

SKSE plugin that dynamically swaps NPC races, sex, and appearance data from
configuration rules.

## Steam 1.7.104 port status

Version **1.6.0** is a source port for the exact Steam runtime
**SkyrimSE 1.7.104.0**. The native calls, overwrite bytes, vtable entries, and
the game's 12-byte tint-layer allocation were checked against the supplied
1.7.104 executable and matching Address Library database. Every native patch
also validates the expected instructions at startup and aborts instead of
patching an unknown executable.

A clean Windows Server 2022 / Visual Studio 2022 GitHub Actions build completed
successfully on 30 September 2026. The generated x64 DLL, SKSE exports, embedded
version/runtime metadata, archive layout, and SHA-256 checksum were inspected.
Skyrim has not been launched with this build, so treat it as a binary port
candidate pending the in-game validation checklist in
[COMPATIBILITY_REVIEW.md](COMPATIBILITY_REVIEW.md).

This build intentionally supports neither **Dynamic Armor Variants** nor
**Devious Devices NG**. It fails fast if either DLL is detected. The ordinary
armor path remains enabled; users who do not install those mods are unaffected.

## Runtime requirements

- Steam Skyrim Special Edition **1.7.104.0**
- SKSE **2.3.1**
- Address Library file `versionlib-1-7-104-0.bin`
- powerofthree's Tweaks, used to resolve editor IDs in configurations

Other Skyrim runtimes, GOG/Epic builds, Skyrim VR, Dynamic Armor Variants, and
Devious Devices NG are not supported by this branch.

## Build requirements

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.22 or newer
- Git with recursive submodule support
- vcpkg checked out at manifest baseline
  `ee12231b20c95013c6638d845d04c91559a1d1ff`
- `VCPKG_ROOT` set to the directory containing
  `scripts/buildsystems/vcpkg.cmake`

The source pins CommonLibSSE-NG 9.2.0 and builds AE support only.

## Build and stage a package

```powershell
git clone --recurse-submodules https://github.com/Nightfallstorm/RaceSwapper
cd RaceSwapper
cmake --preset RaceSwapper
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix staging
```

The staged plugin is
`staging/Data/SKSE/Plugins/RaceSwapper.dll`. The install step also includes the
license and review notes. The checked-in
[`build-steam-1-7-104.yml`](.github/workflows/build-steam-1-7-104.yml) workflow
runs the portable checks, builds on Windows Server 2022, and creates a ZIP plus
SHA-256 file after the branch is pushed to GitHub.

## Offline checks

Portable configuration, weighted-selection, and deterministic-RNG regressions:

```sh
mkdir -p build-review
g++ -std=c++23 -Wall -Wextra -Werror -pedantic \
  -fsanitize=undefined -fno-sanitize-recover=all \
  -Isrc tests/portable_regressions.cpp \
  -o build-review/portable_regressions
./build-review/portable_regressions
```

Exact 1.7.104 runtime audit (the two runtime files are inputs only and must not
be committed or redistributed):

```sh
python3 tools/verify_runtime_1_7_104.py \
  /path/to/SkyrimSE.exe \
  /path/to/versionlib-1-7-104-0.bin
```

## License

[GPL-3.0](LICENSE)
