"""Files in a Fallout 4 Data folder, looked up the way the game does:
loose files first, then BA2 archives.
"""

from __future__ import annotations

from pathlib import Path
from typing import Iterator

from . import ba2


class GameData:
    def __init__(self, data_dir: str | Path, archives: list[str | Path] | None = None):
        self.root = Path(data_dir)
        paths = [Path(p) for p in archives] if archives is not None else sorted(self.root.glob("*.ba2"))
        self.archives: list[ba2.Archive] = []
        for p in paths:
            try:
                self.archives.append(ba2.Archive(p))
            except ba2.BA2Error:
                continue  # texture archives (DX10) and the like: nothing for us
        self._index: dict[str, ba2.Archive] = {}
        for archive in self.archives:  # later archives win, like load order
            for entry in archive.entries:
                self._index[entry.path] = archive

    def close(self) -> None:
        for a in self.archives:
            a.close()

    def __enter__(self) -> "GameData":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def _resolve(self, path: str) -> Path | None:
        """Path under the Data folder, matched case-insensitively (file or folder)."""
        p = self.root / path
        if p.exists():
            return p
        cur = self.root
        for part in Path(path).parts:
            if not cur.is_dir():
                return None
            match = next((c for c in cur.iterdir() if c.name.lower() == part.lower()), None)
            if match is None:
                return None
            cur = match
        return cur

    def _loose(self, path: str) -> Path | None:
        p = self._resolve(path)
        return p if p is not None and p.is_file() else None

    def exists(self, path: str) -> bool:
        path = _norm(path)
        return path in self._index or self._loose(path) is not None

    def read(self, path: str) -> bytes:
        path = _norm(path)
        loose = self._loose(path)
        if loose is not None:
            return loose.read_bytes()
        if path in self._index:
            return self._index[path].read(path)
        raise FileNotFoundError(path)

    def list(self, prefix: str, suffix: str = "") -> Iterator[str]:
        """Paths under prefix from archives and loose files, each once."""
        prefix = _norm(prefix)
        suffix = suffix.lower()
        seen: set[str] = set()
        for path in self._index:
            if path.startswith(prefix) and path.endswith(suffix):
                seen.add(path)
        loose_dir = self._resolve(prefix.rstrip("/")) if prefix else self.root
        if loose_dir is not None and loose_dir.is_dir():
            for f in loose_dir.rglob("*"):
                rel = _norm(str(f.relative_to(self.root)))
                if f.is_file() and rel.endswith(suffix):
                    seen.add(rel)
        yield from sorted(seen)


def _norm(path: str) -> str:
    return path.replace("\\", "/").lower().lstrip("/")
