"""Minimal reader for Fallout 4 plugins (.esm/.esp): just enough to pull
dialogue responses (INFO records) and their text.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

_RECORD = struct.Struct("<4sIII4xHH")  # type, data size, flags, form ID, (vc info), version, unknown
_GROUP_SIZE = 24
_SUB = struct.Struct("<4sH")

FLAG_COMPRESSED = 0x00040000
FLAG_LOCALIZED = 0x00000080  # on the TES4 header record


class PluginError(Exception):
    pass


@dataclass
class Response:
    number: int          # 1-based; matches the "_N" in the voice file name
    emotion: int = 0     # emotion keyword form ID (TRDA), 0 if none
    text: str | None = None      # inline text (non-localized plugins)
    string_id: int | None = None  # ILSTRINGS ID (localized plugins)


@dataclass
class Info:
    form_id: int
    responses: list[Response] = field(default_factory=list)


class Plugin:
    def __init__(self, path: str | Path):
        self.path = Path(path)
        self._data = self.path.read_bytes()
        rtype, size, flags, _, _, _ = _RECORD.unpack_from(self._data, 0)
        if rtype != b"TES4":
            raise PluginError(f"{self.path}: not a Bethesda plugin")
        self.localized = bool(flags & FLAG_LOCALIZED)
        self.masters = [
            payload.rstrip(b"\0").decode("cp1252")
            for sub, payload in _subrecords(self._data[_RECORD.size : _RECORD.size + size])
            if sub == b"MAST"
        ]
        self._body_start = _RECORD.size + size

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def own_index(self) -> int:
        """High byte of form IDs this plugin defines (its masters come first)."""
        return len(self.masters)

    def infos(self) -> Iterator[Info]:
        data = self._data
        pos = self._body_start
        end = len(data)
        while pos + _RECORD.size <= end:
            rtype = data[pos : pos + 4]
            if rtype == b"GRUP":
                # Group contents follow the header directly; a flat scan visits them all.
                pos += _GROUP_SIZE
                continue
            _, size, flags, form_id, _, _ = _RECORD.unpack_from(data, pos)
            body = pos + _RECORD.size
            if rtype == b"INFO":
                payload = data[body : body + size]
                if flags & FLAG_COMPRESSED:
                    payload = zlib.decompress(payload[4:])
                yield _parse_info(form_id, payload, self.localized)
            pos = body + size


def _subrecords(payload: bytes) -> Iterator[tuple[bytes, bytes]]:
    pos = 0
    big_size: int | None = None
    while pos + _SUB.size <= len(payload):
        sub, size = _SUB.unpack_from(payload, pos)
        pos += _SUB.size
        if sub == b"XXXX":
            # The next subrecord is larger than 64 KB; its real size is here.
            (big_size,) = struct.unpack_from("<I", payload, pos)
            pos += size
            continue
        if big_size is not None:
            size, big_size = big_size, None
        yield sub, payload[pos : pos + size]
        pos += size


def _parse_info(form_id: int, payload: bytes, localized: bool) -> Info:
    info = Info(form_id)
    current: Response | None = None
    for sub, data in _subrecords(payload):
        if sub == b"TRDA" and len(data) >= 5:
            emotion, number = struct.unpack_from("<IB", data, 0)
            current = Response(number=number, emotion=emotion)
            info.responses.append(current)
        elif sub == b"NAM1":
            if current is None or current.text is not None or current.string_id is not None:
                # Text without its own TRDA: number it by position.
                current = Response(number=len(info.responses) + 1)
                info.responses.append(current)
            if localized and len(data) == 4:
                (current.string_id,) = struct.unpack("<I", data)
            else:
                current.text = data.rstrip(b"\0").decode("cp1252", errors="replace")
    return info
