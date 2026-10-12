import struct

import pytest

from fo4voice import ba2, esm, fuz, strings
from fo4voice.gamedata import GameData

from . import fakegame


def test_ba2_reads_compressed_and_stored(tmp_path):
    files = {"Sound/Voice/A.fuz": b"alpha" * 100, "strings/b.ilstrings": b"beta"}
    for compress in (True, False):
        path = tmp_path / f"t{compress}.ba2"
        fakegame.ba2(path, files, compress=compress)
        with ba2.Archive(path) as a:
            assert [e.path for e in a.entries] == ["sound/voice/a.fuz", "strings/b.ilstrings"]
            assert a.read("SOUND\\Voice\\a.FUZ") == b"alpha" * 100
            assert "strings/B.ILSTRINGS" in a
            assert [e.path for e in a.glob("sound/", ".fuz")] == ["sound/voice/a.fuz"]


def test_ba2_rejects_texture_archives(tmp_path):
    path = tmp_path / "tex.ba2"
    path.write_bytes(struct.pack("<4sI4sIQ", b"BTDX", 1, b"DX10", 0, 24))
    with pytest.raises(ba2.BA2Error, match="only GNRL"):
        ba2.Archive(path)


def test_strings_tables():
    data = fakegame.ilstrings({7: "Hello there.", 9: "Café time."})
    assert strings.parse(data, "ilstrings") == {7: "Hello there.", 9: "Café time."}

    plain = struct.pack("<II", 1, 4) + struct.pack("<II", 3, 0) + b"Hi!\0"
    assert strings.parse(plain, ".STRINGS") == {3: "Hi!"}


def test_esm_reads_infos_localized(tmp_path):
    path = tmp_path / "Fallout4.esm"
    big = fakegame.sub("NAM3", b"x" * 70000)  # forces an XXXX size override before the text
    fakegame.plugin(
        path,
        [
            fakegame.info(0x00010001, [(1, 100), (2, 101)], localized=True),
            fakegame.info(0x00010002, [(1, 102)], localized=True, compress=True, extra=big),
        ],
        localized=True,
    )
    p = esm.Plugin(path)
    assert p.localized and p.masters == [] and p.own_index == 0
    infos = {i.form_id: i for i in p.infos()}
    assert set(infos) == {0x00010001, 0x00010002}
    assert [(r.number, r.string_id) for r in infos[0x00010001].responses] == [(1, 100), (2, 101)]
    assert infos[0x00010001].responses[0].emotion == 0x000A1B2C
    assert [(r.number, r.string_id) for r in infos[0x00010002].responses] == [(1, 102)]


def test_esm_reads_inline_text_and_masters(tmp_path):
    path = tmp_path / "DLCCoast.esm"
    fakegame.plugin(path, [fakegame.info(0x01000ABC, [(1, "Fog's rolling in.")], localized=False)],
                    localized=False, masters=["Fallout4.esm"])
    p = esm.Plugin(path)
    assert p.masters == ["Fallout4.esm"] and p.own_index == 1
    (i,) = list(p.infos())
    assert i.responses[0].text == "Fog's rolling in."


def test_fuz_round_trip():
    data = fuz.pack(b"LIPDATA", b"XWMAAUDIO")
    assert data[:4] == b"FUZE"
    assert fuz.unpack(data) == (b"LIPDATA", b"XWMAAUDIO")
    assert fuz.unpack(fuz.pack(b"", b"A")) == (b"", b"A")
    with pytest.raises(fuz.FuzError):
        fuz.unpack(b"RIFF0000000000")


def test_gamedata_loose_files_win(tmp_path):
    fakegame.ba2(tmp_path / "a.ba2", {"sound/voice/x/one.fuz": b"archive", "sound/voice/x/two.fuz": b"archive2"})
    loose = tmp_path / "Sound" / "Voice" / "X"
    loose.mkdir(parents=True)
    (loose / "ONE.fuz").write_bytes(b"loose")
    (loose / "three.fuz").write_bytes(b"loose3")
    with GameData(tmp_path) as data:
        assert data.read("sound/voice/x/one.fuz") == b"loose"
        assert data.read("SOUND/VOICE/X/TWO.FUZ") == b"archive2"
        assert list(data.list("sound/voice/x/", ".fuz")) == [
            "sound/voice/x/one.fuz", "sound/voice/x/three.fuz", "sound/voice/x/two.fuz"]
        with pytest.raises(FileNotFoundError):
            data.read("sound/voice/x/missing.fuz")
