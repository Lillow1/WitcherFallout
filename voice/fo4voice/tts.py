"""Text-to-speech backends. Each returns mono 16-bit PCM at its own sample rate."""

from __future__ import annotations

import math
import wave
from array import array
from pathlib import Path
from typing import Protocol

from .recipe import Recipe


class Backend(Protocol):
    sample_rate: int

    def synthesize(self, text: str) -> array: ...


class KokoroBackend:
    """Kokoro-82M (Apache-2.0), running locally. Install: pip install kokoro soundfile."""

    sample_rate = 24000

    def __init__(self, recipe: Recipe, device: str | None = None):
        try:
            import torch
            from kokoro import KPipeline
        except ImportError as e:
            raise RuntimeError("kokoro isn't installed: pip install kokoro soundfile") from e

        self._torch = torch
        self._recipe = recipe
        self._pipeline = KPipeline(lang_code=recipe.lang, device=device)

        # Weighted average of the voicepacks = a new voice that is none of them.
        total = sum(p.weight for p in recipe.blend)
        packs = [self._pipeline.load_single_voice(p.voice) * (p.weight / total) for p in recipe.blend]
        self._voice = torch.stack(packs).sum(dim=0).float()

    def synthesize(self, text: str) -> array:
        chunks = [
            r.audio
            for r in self._pipeline(text, voice=self._voice, speed=self._recipe.speed, split_pattern=None)
            if r.audio is not None
        ]
        if not chunks:
            raise RuntimeError(f"no audio produced for {text!r}")
        audio = self._torch.cat(chunks).clamp(-1.0, 1.0)
        return array("h", (audio * 32767.0).round().to(self._torch.int16).tolist())


class ToneBackend:
    """Stand-in voice for testing the pipeline without a TTS model:
    a soft tone whose length follows the text."""

    sample_rate = 24000

    def __init__(self, recipe: Recipe | None = None):
        self._speed = recipe.speed if recipe else 1.0

    def synthesize(self, text: str) -> array:
        seconds = max(0.4, 0.06 * len(text) / self._speed)
        n = int(seconds * self.sample_rate)
        fade = int(0.02 * self.sample_rate)
        out = array("h")
        for i in range(n):
            env = min(1.0, i / fade, (n - 1 - i) / fade)
            out.append(int(6000 * env * math.sin(2 * math.pi * 220 * i / self.sample_rate)))
        return out


def create(recipe: Recipe, device: str | None = None) -> Backend:
    if recipe.backend == "kokoro":
        return KokoroBackend(recipe, device)
    if recipe.backend == "tone":
        return ToneBackend(recipe)
    raise ValueError(f"unknown TTS backend {recipe.backend!r}")


def write_wav(path: str | Path, samples: array, sample_rate: int) -> None:
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(samples.tobytes())
