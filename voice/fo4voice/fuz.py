"""The .fuz voice file: lip-sync data and xWMA audio in one file.

Layout: b"FUZE", u32 version (1), u32 lip size, lip bytes, audio bytes.
"""

from __future__ import annotations

import struct

_HEADER = struct.Struct("<4sII")
VERSION = 1


class FuzError(Exception):
    pass


def pack(lip: bytes, audio: bytes) -> bytes:
    return _HEADER.pack(b"FUZE", VERSION, len(lip)) + lip + audio


def unpack(data: bytes) -> tuple[bytes, bytes]:
    """Returns (lip, audio)."""
    if len(data) < _HEADER.size:
        raise FuzError("too short to be a .fuz")
    magic, _, lip_size = _HEADER.unpack_from(data, 0)
    if magic != b"FUZE":
        raise FuzError(f"not a .fuz (magic {magic!r})")
    start = _HEADER.size
    if start + lip_size > len(data):
        raise FuzError("lip data runs past the end of the file")
    return data[start : start + lip_size], data[start + lip_size :]
