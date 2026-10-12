"""Read Fallout 4 "General" BA2 archives (BTDX / GNRL).

Only reading is supported. For output we write loose files, which the game
(and MO2) load ahead of archives; pack them with the Creation Kit's
Archive2.exe if you want a BA2.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Iterator

_HEADER = struct.Struct("<4sI4sIQ")  # magic, version, type, file count, name table offset
_RECORD = struct.Struct("<I4sIIQIII")  # name hash, ext, dir hash, flags, offset, packed, unpacked, align

# 1 = original release, 7/8 = 2024 next-gen update. Same GNRL layout.
SUPPORTED_VERSIONS = {1, 7, 8}


class BA2Error(Exception):
    pass


@dataclass(frozen=True)
class Entry:
    path: str  # lowercase, forward slashes
    offset: int
    packed_size: int  # 0 = stored uncompressed
    size: int


class Archive:
    """A GNRL BA2 opened for reading. Use as a context manager."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self._file: BinaryIO = open(self.path, "rb")
        try:
            self.entries = self._read_index()
        except Exception:
            self._file.close()
            raise
        self._by_path = {e.path: e for e in self.entries}

    def __enter__(self) -> "Archive":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        self._file.close()

    def _read_index(self) -> list[Entry]:
        f = self._file
        raw = f.read(_HEADER.size)
        if len(raw) < _HEADER.size:
            raise BA2Error(f"{self.path}: too short to be a BA2")
        magic, version, kind, count, names_at = _HEADER.unpack(raw)
        if magic != b"BTDX":
            raise BA2Error(f"{self.path}: not a BA2 (magic {magic!r})")
        if kind != b"GNRL":
            raise BA2Error(f"{self.path}: {kind.decode(errors='replace')} archive, only GNRL is supported")
        if version not in SUPPORTED_VERSIONS:
            raise BA2Error(f"{self.path}: unsupported BA2 version {version}")

        records = [_RECORD.unpack(f.read(_RECORD.size)) for _ in range(count)]

        f.seek(names_at)
        entries = []
        for rec in records:
            (length,) = struct.unpack("<H", f.read(2))
            name = f.read(length).decode("utf-8", errors="replace")
            _, _, _, _, offset, packed, size, _ = rec
            entries.append(Entry(_norm(name), offset, packed, size))
        return entries

    def __contains__(self, path: str) -> bool:
        return _norm(path) in self._by_path

    def read(self, path: str | Entry) -> bytes:
        entry = path if isinstance(path, Entry) else self._by_path[_norm(path)]
        self._file.seek(entry.offset)
        if entry.packed_size:
            data = zlib.decompress(self._file.read(entry.packed_size))
        else:
            data = self._file.read(entry.size)
        if len(data) != entry.size:
            raise BA2Error(f"{entry.path}: expected {entry.size} bytes, got {len(data)}")
        return data

    def glob(self, prefix: str, suffix: str = "") -> Iterator[Entry]:
        prefix = _norm(prefix)
        suffix = suffix.lower()
        for e in self.entries:
            if e.path.startswith(prefix) and e.path.endswith(suffix):
                yield e


def _norm(path: str) -> str:
    return path.replace("\\", "/").lower().lstrip("/")
