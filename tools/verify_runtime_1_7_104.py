#!/usr/bin/env python3
"""Verify the exact Steam 1.7.104 executable and Address Library hook sites.

This script uses only Python's standard library. It does not modify either input.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


EXPECTED_EXE_SHA256 = "c06a66c7d640458cbdf4b26a389357a8bb37d2aa5165a3f743c2ac91355b9bd9"
EXPECTED_LIBRARY_SHA256 = "8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842"
EXPECTED_VERSION = (1, 7, 104, 0)
EXPECTED_ENTRY_COUNT = 565_759


class VerificationError(RuntimeError):
    pass


@dataclass(frozen=True)
class Section:
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int


class PEImage:
    def __init__(self, data: bytes) -> None:
        self.data = data
        if data[:2] != b"MZ":
            raise VerificationError("SkyrimSE.exe has no MZ header")
        pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise VerificationError("SkyrimSE.exe has no PE signature")

        coff = pe_offset + 4
        machine, section_count = struct.unpack_from("<HH", data, coff)
        if machine != 0x8664:
            raise VerificationError(f"expected AMD64 PE machine 0x8664, got 0x{machine:04X}")
        optional_size = struct.unpack_from("<H", data, coff + 16)[0]
        optional = coff + 20
        if struct.unpack_from("<H", data, optional)[0] != 0x20B:
            raise VerificationError("SkyrimSE.exe is not a PE32+ image")
        self.image_base = struct.unpack_from("<Q", data, optional + 24)[0]

        table = optional + optional_size
        sections: list[Section] = []
        for index in range(section_count):
            entry = table + index * 40
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", data, entry + 8
            )
            sections.append(Section(virtual_address, virtual_size, raw_offset, raw_size))
        self.sections = sections

    def read_rva(self, rva: int, size: int) -> bytes:
        for section in self.sections:
            extent = max(section.virtual_size, section.raw_size)
            if section.virtual_address <= rva and rva + size <= section.virtual_address + extent:
                offset = section.raw_offset + rva - section.virtual_address
                result = self.data[offset : offset + size]
                if len(result) != size:
                    break
                return result
        raise VerificationError(f"RVA 0x{rva:X} (size 0x{size:X}) is outside the PE sections")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def load_address_library(data: bytes) -> tuple[int, ...]:
    require(len(data) >= 96, "Address Library file is shorter than its format 5 header")
    format_version, *runtime = struct.unpack_from("<5I", data, 0)
    module = data[20:84].split(b"\0", 1)[0]
    pointer_size, data_format, count = struct.unpack_from("<III", data, 84)

    require(format_version == 5, f"expected Address Library format 5, got {format_version}")
    require(tuple(runtime) == EXPECTED_VERSION, f"expected library version {EXPECTED_VERSION}, got {tuple(runtime)}")
    require(module == b"SkyrimSE.exe", f"unexpected Address Library module {module!r}")
    require(pointer_size == 8, f"expected 8-byte pointers, got {pointer_size}")
    require(data_format == 0, f"expected dense format 0, got {data_format}")
    require(count == EXPECTED_ENTRY_COUNT, f"expected {EXPECTED_ENTRY_COUNT} entries, got {count}")
    require(len(data) == 96 + count * 4, "Address Library length does not match its entry count")
    return struct.unpack_from(f"<{count}I", data, 96)


def verify_bytes(image: PEImage, rva: int, expected: bytes, name: str) -> None:
    actual = image.read_rva(rva, len(expected))
    require(
        actual == expected,
        f"{name} mismatch at RVA 0x{rva:X}: expected {expected.hex(' ')}, got {actual.hex(' ')}",
    )
    print(f"[OK] {name}: RVA 0x{rva:X}")


def verify_call(image: PEImage, offsets: tuple[int, ...], site_rva: int, target_id: int, name: str) -> None:
    instruction = image.read_rva(site_rva, 5)
    require(instruction[0] == 0xE8, f"{name} at RVA 0x{site_rva:X} is not a relative call")
    displacement = struct.unpack_from("<i", instruction, 1)[0]
    actual = site_rva + 5 + displacement
    expected = offsets[target_id]
    require(
        actual == expected,
        f"{name} targets RVA 0x{actual:X}; Address Library ID {target_id} is RVA 0x{expected:X}",
    )
    print(f"[OK] {name}: RVA 0x{site_rva:X} -> ID {target_id}")


def verify_vtable(
    image: PEImage,
    offsets: tuple[int, ...],
    vtable_id: int,
    slot: int,
    function_id: int,
    name: str,
) -> None:
    raw = image.read_rva(offsets[vtable_id] + slot * 8, 8)
    actual = struct.unpack("<Q", raw)[0] - image.image_base
    expected = offsets[function_id]
    require(
        actual == expected,
        f"{name} slot 0x{slot:X} points to RVA 0x{actual:X}; expected ID {function_id} at 0x{expected:X}",
    )
    print(f"[OK] {name}: vtable ID {vtable_id} slot 0x{slot:X} -> ID {function_id}")


def verify(executable: Path, address_library: Path) -> None:
    exe_data = executable.read_bytes()
    library_data = address_library.read_bytes()
    require(sha256(exe_data) == EXPECTED_EXE_SHA256, "SkyrimSE.exe SHA-256 is not the audited Steam 1.7.104 executable")
    require(
        sha256(library_data) == EXPECTED_LIBRARY_SHA256,
        "Address Library SHA-256 is not the audited versionlib-1-7-104-0.bin",
    )
    print("[OK] input SHA-256 values match the audited files")

    offsets = load_address_library(library_data)
    image = PEImage(exe_data)
    require(image.image_base == 0x140000000, f"unexpected PE image base 0x{image.image_base:X}")
    print("[OK] Address Library format 5 header and PE image metadata")

    get_model = offsets[19749] + 0x6B
    verify_bytes(
        image,
        get_model,
        bytes.fromhex(
            "e8 60 12 0c 00 48 63 c8 48 8d 04 8d 13 00 00 00 "
            "48 03 c1 48 8d 1c c5 00 00 00 00 48 03 dd 48 8d 4b 08 "
            "e8 0e 27 bc 00 85 c0 75 07 48 8d 9d 98 00 00 00"
        ),
        "GetTESModel overwrite",
    )
    verify_bytes(
        image,
        offsets[24730] + 0x5A,
        bytes.fromhex("e8 91 c4 fe ff 48 63 c8 4d 8b b4 ce a8 04 00 00"),
        "GetFaceRelatedData overwrite",
    )
    verify_call(image, offsets, offsets[26837] + 0x7D, 26838, "GetFaceRelatedData2")
    verify_bytes(image, offsets[37177] + 0x54, bytes.fromhex("48 8b ca"), "GetBodyPartData argument")
    verify_call(image, offsets, offsets[37177] + 0x57, 25304, "GetBodyPartData")
    for caller_id, call_offset, argument_offset, argument, name in (
        (37450, 0xCB, -0x9, "48 8b 8b f8 01 00 00", "GetBaseMoveTypes 1"),
        (37601, 0x30, -0x3, "48 8b ce", "GetBaseMoveTypes 2"),
        (37942, 0x1C, -0xC, "48 8b 89 f8 01 00 00", "GetBaseMoveTypes 3"),
        (37945, 0xCB, -0xE, "48 8b 8e f8 01 00 00", "GetBaseMoveTypes 4"),
        (38029, 0xEB, -0xE, "48 8b 8e f8 01 00 00", "GetBaseMoveTypes 5"),
    ):
        call_rva = offsets[caller_id] + call_offset
        verify_bytes(image, call_rva + argument_offset, bytes.fromhex(argument), f"{name} argument")
        verify_call(image, offsets, call_rva, 25307, f"{name} call")
    verify_bytes(image, offsets[25307], bytes.fromhex("48 63 c2"), "movement-type signed 32-bit index")
    verify_call(image, offsets, offsets[24736] + 0x302, 17792, "LoadTESObjectARMO main")
    verify_call(image, offsets, offsets[24737] + 0x78, 17792, "LoadTESObjectARMO secondary")
    verify_call(image, offsets, offsets[24741] + 0xEE, 17792, "LoadTESObjectARMO inventory")
    verify_call(image, offsets, offsets[15676] + 0x359, 17792, "LoadSkin")
    verify_bytes(image, offsets[17759], bytes.fromhex("40 55 56 57 41 54 41 55"), "TESObjectARMA::AddToBiped")
    verify_bytes(image, offsets[24763], bytes.fromhex("48 89 5c 24 08"), "Height entry")
    verify_call(image, offsets, offsets[37925] + 0xCC, 24669, "SetRace")
    verify_bytes(image, offsets[37925] + 0xD1, bytes.fromhex("84 c0"), "SetRace boolean result use")
    verify_bytes(
        image,
        offsets[24790] + 0xC9,
        bytes.fromhex("48 8b 83 e8 01 00 00 48 85 c0 74 11 48 3b 83 58 01 00 00 74 08 b0 01"),
        "HasOverlays overwrite",
    )
    verify_bytes(image, offsets[24790] + 0xE0, bytes.fromhex("48 83 c4 20 5b c3"), "HasOverlays continuation")

    for vtable_id, slot, function_id, name in (
        (195816, 0x00, 24888, "TESNPC destructor"),
        (195816, 0x0E, 24777, "TESNPC::SaveGame"),
        (195816, 0x0F, 24778, "TESNPC::LoadGame"),
        (195816, 0x12, 24779, "TESNPC::Revert"),
        (195816, 0x2F, 24664, "TESNPC::Copy"),
        (195818, 0x04, 24720, "TESNPC::CopyFromTemplate"),
        (207886, 0x48, 37183, "Character::HasKeyword"),
        (207886, 0x72, 38058, "Character::PopulateGraph"),
    ):
        verify_vtable(image, offsets, vtable_id, slot, function_id, name)

    verify_bytes(image, offsets[38058] + 0x19, bytes.fromhex("49 8b f0"), "animation-graph opaque R8 argument")
    verify_bytes(image, offsets[38058] + 0xEB, bytes.fromhex("b0 01"), "animation-graph true result")
    verify_bytes(image, offsets[38058] + 0xFD, bytes.fromhex("32 c0"), "animation-graph false result")

    # Runtime evidence for the special 12-byte tint-layer allocation size.
    verify_bytes(image, 0x3BCB17, bytes.fromhex("b9 0c 00 00 00"), "tint-layer allocation size")
    verify_bytes(image, 0x3BC040, bytes.fromhex("ba 0c 00 00 00"), "tint-layer free size")

    print("\nAll Steam 1.7.104 runtime, hook, vtable, and tint-layout checks passed.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="path to Steam 1.7.104 SkyrimSE.exe")
    parser.add_argument("address_library", type=Path, help="path to versionlib-1-7-104-0.bin")
    args = parser.parse_args()
    try:
        verify(args.executable, args.address_library)
    except (OSError, VerificationError, struct.error) as error:
        print(f"FAILED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
