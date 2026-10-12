"""Voice recipe: how the player voice is made. See voices/*.toml.

A recipe designs an original voice by blending stock TTS voices and shaping
the result (speed, pitch, EQ). There is intentionally no way to feed in a
recording of a real person: this tool doesn't clone voices.
"""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class BlendPart:
    voice: str     # stock voice name (e.g. "bf_isabella") or a path to a .pt voicepack you made
    weight: float


@dataclass
class Recipe:
    name: str
    backend: str = "kokoro"
    lang: str = "b"          # Kokoro language code: "b" British English, "a" American
    speed: float = 1.0
    blend: list[BlendPart] = field(default_factory=list)
    pitch: float = 1.0       # < 1 lowers the voice, > 1 raises it (duration is kept)
    filters: str = ""        # extra ffmpeg audio filters applied after pitch
    pronounce: dict[str, str] = field(default_factory=dict)
    path: Path | None = None


def load(path: str | Path) -> Recipe:
    path = Path(path)
    with open(path, "rb") as f:
        doc = tomllib.load(f)
    voice = doc.get("voice", {})
    post = doc.get("post", {})

    blend = [BlendPart(str(b["voice"]), float(b.get("weight", 1.0))) for b in voice.get("blend", [])]
    for b in blend:
        # Custom voicepacks are found relative to the recipe file.
        if b.voice.endswith(".pt") and not Path(b.voice).is_absolute():
            b.voice = str((path.parent / b.voice).resolve())

    recipe = Recipe(
        name=str(voice.get("name", path.stem)),
        backend=str(voice.get("backend", "kokoro")),
        lang=str(voice.get("lang", "b")),
        speed=float(voice.get("speed", 1.0)),
        blend=blend,
        pitch=float(post.get("pitch", 1.0)),
        filters=str(post.get("filters", "")),
        pronounce={str(k): str(v) for k, v in doc.get("pronounce", {}).items()},
        path=path,
    )
    validate(recipe)
    return recipe


def validate(r: Recipe) -> None:
    if r.backend == "kokoro" and not r.blend:
        raise ValueError(f"{r.name}: a kokoro recipe needs at least one [[voice.blend]] entry")
    if any(b.weight <= 0 for b in r.blend):
        raise ValueError(f"{r.name}: blend weights must be positive")
    if not 0.5 <= r.speed <= 2.0:
        raise ValueError(f"{r.name}: speed {r.speed} is outside 0.5-2.0")
    if not 0.7 <= r.pitch <= 1.4:
        raise ValueError(f"{r.name}: pitch {r.pitch} is outside 0.7-1.4")
