"""Turn a dialogue line's text into something a TTS voice should say.

Each line comes out as one of:
  ok      - speak it
  review  - speakable, but something was changed or guessed; listen to it
  skip    - nothing to say (grunts, pain sounds, blank lines); the vanilla file stays
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

OK = "ok"
REVIEW = "review"
SKIP = "skip"

_DIRECTION = re.compile(r"\{[^}]*\}|\[[^\]]*\]")       # {Sarcastic}  [laughs]
_PAREN = re.compile(r"\(([^)]*)\)")                     # (sigh) or a real aside
_TOKEN = re.compile(r"<[^>]+>")                          # <Alias=Settler>  <Global=...>
_SPACE = re.compile(r"\s+")
_SPEAKABLE = re.compile(r"[A-Za-z0-9]")

# Parenthesised words that are performance notes, not dialogue.
_NONVERBAL = {
    "sigh", "sighs", "laugh", "laughs", "laughing", "chuckle", "chuckles", "groan", "groans",
    "grunt", "grunts", "scoff", "scoffs", "gasp", "gasps", "cough", "coughs", "pause",
    "beat", "whisper", "whispering", "shout", "shouting", "yell", "yelling", "sarcastic",
    "angry", "sad", "happy", "scared", "crying", "sniff", "sniffs",
}


@dataclass
class Prepared:
    text: str
    status: str
    reasons: list[str] = field(default_factory=list)


def prepare(raw: str | None, pronounce: dict[str, str] | None = None) -> Prepared:
    if raw is None or not raw.strip():
        return Prepared("", SKIP, ["no text"])

    reasons: list[str] = []
    text = raw

    if _DIRECTION.search(text):
        text = _DIRECTION.sub(" ", text)
        reasons.append("removed stage direction")

    def paren(m: re.Match[str]) -> str:
        inner = m.group(1).strip()
        if inner.lower().rstrip(".!") in _NONVERBAL:
            reasons.append(f"removed ({inner})")
            return " "
        return m.group(0)

    text = _PAREN.sub(paren, text)

    if _TOKEN.search(text):
        text = _TOKEN.sub(" ", text)
        reasons.append("removed text token (name or number filled in by the game)")

    for word, said in (pronounce or {}).items():
        text, n = re.subn(rf"(?<![\w-]){re.escape(word)}(?![\w-])", said, text, flags=re.IGNORECASE)
        if n:
            reasons.append(f"pronunciation: {word}")

    text = _SPACE.sub(" ", text).strip()
    # Tidy what removals leave behind: "Thanks, ." -> "Thanks."
    text = re.sub(r"\s+([,.!?;:])", r"\1", text)
    text = re.sub(r",(?=[.!?])", "", text)
    text = text.lstrip(",;: ")
    if reasons and text[:1].islower():
        text = text[0].upper() + text[1:]

    if not _SPEAKABLE.search(text):
        return Prepared("", SKIP, reasons + ["nothing speakable"])

    # Pronunciation fixes are routine; anything else changed the line's meaning.
    needs_review = any(not r.startswith("pronunciation:") for r in reasons)
    return Prepared(text, REVIEW if needs_review else OK, reasons)
