"""Read localized string tables (.STRINGS, .DLSTRINGS, .ILSTRINGS).

When a plugin is localized, text fields hold a 32-bit string ID instead of
the text. Dialogue lines (INFO NAM1) live in the .ILSTRINGS table.
"""

from __future__ import annotations

import struct


def parse(data: bytes, kind: str) -> dict[int, str]:
    """Parse a string table. kind is the file extension: strings, dlstrings or ilstrings."""
    kind = kind.lower().lstrip(".")
    if kind not in ("strings", "dlstrings", "ilstrings"):
        raise ValueError(f"unknown string table kind {kind!r}")
    count, data_size = struct.unpack_from("<II", data, 0)
    dir_end = 8 + count * 8
    base = dir_end
    if base + data_size > len(data):
        raise ValueError("string table is truncated")

    out: dict[int, str] = {}
    for i in range(count):
        sid, offset = struct.unpack_from("<II", data, 8 + i * 8)
        at = base + offset
        if kind == "strings":
            end = data.index(b"\0", at)
            raw = data[at:end]
        else:
            (length,) = struct.unpack_from("<I", data, at)
            raw = data[at + 4 : at + 4 + length].rstrip(b"\0")
        out[sid] = decode(raw)
    return out


def decode(raw: bytes) -> str:
    # Bethesda's English tables are mostly cp1252; a few modded ones are UTF-8.
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("cp1252", errors="replace")
