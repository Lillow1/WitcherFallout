from fo4voice import lines


def test_plain_line_is_ok():
    p = lines.prepare("  Let's get   moving. ")
    assert (p.text, p.status) == ("Let's get moving.", lines.OK)


def test_stage_directions_are_removed_and_flagged():
    p = lines.prepare("{Sarcastic} Oh, great. [laughs] Just great.")
    assert p.text == "Oh, great. Just great."
    assert p.status == lines.REVIEW


def test_nonverbal_parentheses_removed_but_real_asides_kept():
    assert lines.prepare("(Sighs) Fine.").text == "Fine."
    p = lines.prepare("Take the left path (the one by the river).")
    assert p.text == "Take the left path (the one by the river)."
    assert p.status == lines.OK


def test_game_tokens_need_review():
    p = lines.prepare("Thanks, <Alias=Settler>. I owe you.")
    assert p.text == "Thanks. I owe you."
    assert p.status == lines.REVIEW


def test_token_at_start_leaves_no_stray_comma():
    assert lines.prepare("<Alias=Settler>, get inside.").text == "Get inside."


def test_grunts_and_blank_lines_are_skipped():
    for raw in ["", None, "   ", "{Pain grunt}", "(Laughs)", "...", "[Exertion]"]:
        assert lines.prepare(raw).status == lines.SKIP, raw


def test_pronunciation_is_whole_word_and_case_insensitive():
    p = lines.prepare("Vault-Tec made this. vault-tec made that. Vault-Teccy isn't a word.", {"Vault-Tec": "Vault Tek"})
    assert p.text == "Vault Tek made this. Vault Tek made that. Vault-Teccy isn't a word."
    assert p.status == lines.OK
