#!/usr/bin/env python3
"""Generate main/utils/bip39_zh_pinyin_data.c from the BIP39 Chinese
(simplified) wordlist plus pypinyin.

Each of the 2048 words in the Chinese BIP39 wordlist is a single Han
character. This script looks up its most common toneless pinyin reading
(e.g. "zhong") and emits a C array indexed the same way as the compiled
wordlist in components/libwally-core/upstream/src/data/wordlists/
chinese_simplified.c, so `bip39_zh_pinyin[i]` and `bip39_get_word_by_index
(kern_bip39_zh_wordlist(), i)` always refer to the same word.

Usage:
    python3 -m pip install pypinyin
    python3 tools/gen_bip39_zh_pinyin.py
"""
import sys
from pathlib import Path

try:
    from pypinyin import pinyin, Style
except ImportError as err:
    raise SystemExit(
        "pypinyin is required. Install it with: python3 -m pip install pypinyin"
    ) from err

REPO_ROOT = Path(__file__).resolve().parent.parent
WORDLIST_PATH = (
    REPO_ROOT
    / "components/libwally-core/upstream/src/data/wordlists/chinese_simplified.txt"
)
OUTPUT_PATH = REPO_ROOT / "main/utils/bip39_zh_pinyin_data.c"

EXPECTED_WORD_COUNT = 2048


def load_words():
    with WORDLIST_PATH.open(encoding="utf-8") as f:
        words = [line.strip() for line in f if line.strip()]
    if len(words) != EXPECTED_WORD_COUNT:
        raise SystemExit(
            f"expected {EXPECTED_WORD_COUNT} words in {WORDLIST_PATH}, "
            f"got {len(words)}"
        )
    for idx, w in enumerate(words):
        if len(w) != 1:
            raise SystemExit(f"word at index {idx} is not a single character: {w!r}")
    return words


def to_pinyin(words):
    result = []
    max_len = 0
    for w in words:
        py = pinyin(w, style=Style.NORMAL, heteronym=False)[0][0]
        if not py.isascii() or not py.isalpha():
            raise SystemExit(f"unexpected pinyin for {w!r}: {py!r}")
        max_len = max(max_len, len(py))
        result.append(py)
    return result, max_len


def write_c_file(words, pinyins, max_len):
    lines = []
    lines.append("/* Generated file - do not edit by hand!")
    lines.append(" * Regenerate with: python3 tools/gen_bip39_zh_pinyin.py")
    lines.append(" *")
    lines.append(" * Toneless pinyin reading for each word in the BIP39 Chinese")
    lines.append(" * (simplified) wordlist, indexed identically to")
    lines.append(
        " * components/libwally-core/upstream/src/data/wordlists/chinese_simplified.c"
    )
    lines.append(" * (word i here <-> word i in kern_bip39_zh_wordlist()).")
    lines.append(" *")
    lines.append(" * Source: pypinyin (https://github.com/mozillazg/python-pinyin),")
    lines.append(" * default (most common) reading per character, tone marks removed.")
    lines.append(" */")
    lines.append("")
    lines.append('#include "bip39_filter_zh.h"')
    lines.append("")
    lines.append(f"_Static_assert(BIP39_ZH_MAX_PINYIN_LEN >= {max_len},")
    lines.append('               "BIP39_ZH_MAX_PINYIN_LEN too small for generated data");')
    lines.append("")
    lines.append(
        f"const char *const bip39_zh_pinyin[BIP39_ZH_WORDLIST_SIZE] = {{"
    )
    for i in range(0, len(pinyins), 8):
        chunk = pinyins[i : i + 8]
        line = "    " + ", ".join(f'"{p}"' for p in chunk) + ","
        lines.append(line)
    lines.append("};")
    lines.append("")

    OUTPUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUTPUT_PATH} ({len(pinyins)} entries, max pinyin len {max_len})")


def main():
    words = load_words()
    pinyins, max_len = to_pinyin(words)
    write_c_file(words, pinyins, max_len)


if __name__ == "__main__":
    main()
