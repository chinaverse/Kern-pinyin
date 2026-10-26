#!/usr/bin/env python3
"""Bake a subsetted LVGL bitmap font containing exactly the 2048 unique
Han characters used by the BIP39 Chinese (simplified) wordlist -- not a
general-purpose CJK font. Mirrors tools/bake_icons.py's pure-Python
(Pillow only, no Node/lv_font_conv) glyph-baking approach, generalized
from a small icon set to this much larger but still fixed, known
character set.

Generates one bip39_zh_<size>.c/.h pair per size in SIZES (matching
tools/bake_icons.py's SIZES / tools/derive_font_sizes.py's per-board
font sizes), plus main/ui/assets/bip39_zh_fonts.h with a
bip39_zh_font_for_size() lookup. Each generated font's `.fallback` field
is left NULL: icons_<size> is compiled out per-board (see
tools/ui_font_policy.py), so a size baked here would dangle on any board
that didn't pick that exact icon size. main/ui/theme.c instead takes a
mutable copy of whatever bip39_zh_font_for_size() returns and points
*that* copy's .fallback at this board's actual icon font, extending its
own chain: text font -> CJK subset copy -> icon font.

Usage:
    python3 -m pip install Pillow
    python3 tools/bake_bip39_zh_font.py --font /path/to/NotoSansSC.ttf
"""
import argparse
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as err:
    raise SystemExit(
        "Pillow is required. Install it with: python3 -m pip install Pillow"
    ) from err

REPO_ROOT = Path(__file__).resolve().parent.parent
WORDLIST_PATH = (
    REPO_ROOT
    / "components/libwally-core/upstream/src/data/wordlists/chinese_simplified.txt"
)
OUTPUT_DIR = REPO_ROOT / "main/ui/assets"

# Must match tools/bake_icons.py's SIZES.
SIZES = (16, 22, 24, 28, 30, 32, 34, 40)
BPP = 4


def load_codepoints():
    with WORDLIST_PATH.open(encoding="utf-8") as f:
        words = [line.strip() for line in f if line.strip()]
    if len(words) != 2048:
        raise SystemExit(f"expected 2048 words, got {len(words)}")
    codepoints = sorted({ord(w) for w in words})
    if len(codepoints) != 2048:
        raise SystemExit(
            f"expected 2048 unique characters, got {len(codepoints)} "
            "(duplicate word in wordlist?)"
        )
    return codepoints


def pack_bitmap(image):
    pixels = list(image.getdata())
    packed = []
    for pos in range(0, len(pixels), 2):
        hi = (pixels[pos] + 8) // 17
        lo = (pixels[pos + 1] + 8) // 17 if pos + 1 < len(pixels) else 0
        packed.append((hi << 4) | lo)
    return packed


def render_glyph(font, codepoint):
    char = chr(codepoint)
    left, top, right, bottom = font.getbbox(char, anchor="ls")
    width = max(0, right - left)
    height = max(0, bottom - top)
    advance = int(round(font.getlength(char)))

    if width == 0 or height == 0:
        return {
            "bitmap": [],
            "bitmap_index": 0,
            "adv_w": advance * 16,
            "box_w": 0,
            "box_h": 0,
            "ofs_x": 0,
            "ofs_y": 0,
            "bottom": 0,
        }

    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((-left, -top), char, font=font, fill=255, anchor="ls")

    return {
        "bitmap": pack_bitmap(image),
        "bitmap_index": 0,
        "adv_w": advance * 16,
        "box_w": width,
        "box_h": height,
        "ofs_x": left,
        "ofs_y": -bottom,
        "bottom": bottom,
    }


def comma_hex(values, indent="    "):
    if not values:
        return indent + "0x0"

    lines = []
    line = indent
    for value in values:
        text = f"0x{value:x}, "
        if len(line) + len(text) > 100:
            lines.append(line.rstrip())
            line = indent
        line += text
    if line.strip():
        lines.append(line.rstrip().rstrip(","))
    return "\n".join(lines)


def write_font(path, font_path, size, codepoints):
    font_name = f"bip39_zh_{size}"
    font = ImageFont.truetype(str(font_path), size=size)

    glyphs = []
    bitmap = []
    max_above = 0
    max_below = 0
    # SPARSE_TINY is binary-searched by LVGL, so glyphs must be emitted in
    # strictly ascending codepoint order.
    for codepoint in sorted(codepoints):
        glyph = render_glyph(font, codepoint)
        glyph["bitmap_index"] = len(bitmap)
        glyphs.append((codepoint, glyph))
        bitmap.extend(glyph["bitmap"])
        max_above = max(max_above, glyph["box_h"] + glyph["ofs_y"])
        max_below = max(max_below, -glyph["ofs_y"])

    base_line = max_below
    line_height = max(size, max_above + max_below)
    range_start = min(codepoint for codepoint, _ in glyphs)
    offsets = [codepoint - range_start for codepoint, _ in glyphs]
    range_length = max(offsets) + 1
    if range_length > 0xFFFF:
        raise SystemExit(
            f"codepoint range {range_length} exceeds SPARSE_TINY's uint16 offsets"
        )

    text = f"""/*******************************************************************************
 * BIP39 Chinese (simplified) wordlist font -- exactly the {len(glyphs)} Han
 * characters in components/libwally-core/upstream/src/data/wordlists/
 * chinese_simplified.txt, not a general-purpose CJK font.
 * Size: {size} px
 * Bpp: {BPP}
 * Generated locally from a Noto Sans SC source font:
 *   tools/bake_bip39_zh_font.py
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef {font_name.upper()}
#define {font_name.upper()} 1
#endif

#if {font_name.upper()}

/*-----------------
 *    BITMAPS
 *----------------*/

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{comma_hex(bitmap)}
}};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
    {{.bitmap_index = 0,
     .adv_w = 0,
     .box_w = 0,
     .box_h = 0,
     .ofs_x = 0,
     .ofs_y = 0}} /* id = 0 reserved */,
"""

    for idx, (_, glyph) in enumerate(glyphs, start=1):
        suffix = "," if idx < len(glyphs) else ""
        text += f"""    {{.bitmap_index = {glyph["bitmap_index"]},
     .adv_w = {glyph["adv_w"]},
     .box_w = {glyph["box_w"]},
     .box_h = {glyph["box_h"]},
     .ofs_x = {glyph["ofs_x"]},
     .ofs_y = {glyph["ofs_y"]}}}{suffix}
"""

    text += f"""}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {{{", ".join(f"0x{value:x}" for value in offsets)}}};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{.range_start = {range_start},
     .range_length = {range_length},
     .glyph_id_start = 1,
     .unicode_list = unicode_list_0,
     .glyph_id_ofs_list = NULL,
     .list_length = sizeof(unicode_list_0) / sizeof(unicode_list_0[0]),
     .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}}}};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {{
#else
static lv_font_fmt_txt_dsc_t font_dsc = {{
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = {BPP},
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
}};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

#if LVGL_VERSION_MAJOR >= 8
const lv_font_t {font_name} = {{
#else
lv_font_t {font_name} = {{
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -{max(1, size // 12)},
    .underline_thickness = {max(1, size // 24)},
#endif
    .dsc = &font_dsc,
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    /* Left NULL here deliberately: main/ui/theme.c always overwrites this
     * field at runtime on its own mutable copy, re-pointing it at
     * whichever icon font size this board's UI policy actually selected
     * (icons_<N> is compiled out per-board -- see tools/ui_font_policy.py
     * -- so hard-coding a same-size icon font here would dangle whenever
     * that size isn't the one this board picked). */
    .fallback = NULL,
#endif
    .user_data = NULL,
}};

#endif /*#if {font_name.upper()}*/
"""

    path.write_text(text)
    return len(bitmap), len(glyphs)


def write_decl_header(path, size):
    guard = f"BIP39_ZH_{size}_H"
    text = f"""/**
 * BIP39 Chinese (simplified) wordlist font, {size}px, Bpp: {BPP}
 * Generated locally by tools/bake_bip39_zh_font.py
 */

#ifndef {guard}
#define {guard}

#include "lvgl.h"

LV_FONT_DECLARE(bip39_zh_{size});

#endif // {guard}
"""
    path.write_text(text)


def write_lookup_header(path, baked_sizes):
    baked_sizes = sorted(baked_sizes)
    includes = "\n".join(f'#include "bip39_zh_{s}.h"' for s in baked_sizes)
    # Not every board's (small_px, medium_px) pair (see
    # tools/derive_font_sizes.py) necessarily has an exact match among the
    # sizes actually baked here (see this script's --sizes / the tradeoff
    # noted where it's invoked from), so pick the closest one instead of a
    # fixed fallback -- keeps Chinese text close to the surrounding Latin
    # text's size on every board rather than jarringly mismatched on
    # whichever boards fall outside the baked set.
    table = ", ".join(str(s) for s in baked_sizes)
    fonts = ", ".join(f"&bip39_zh_{s}" for s in baked_sizes)
    text = f"""/* Generated by tools/bake_bip39_zh_font.py -- do not edit.
 * Looks up the BIP39-Chinese-wordlist subset font closest to a UI text
 * size, for chaining into a label's font .fallback (see main/ui/theme.c).
 */
#ifndef BIP39_ZH_FONTS_H
#define BIP39_ZH_FONTS_H

#include <stddef.h>
#include <stdint.h>

{includes}

static inline const lv_font_t *bip39_zh_font_for_size(uint16_t size) {{
  static const uint16_t sizes[] = {{{table}}};
  static const lv_font_t *const fonts[] = {{{fonts}}};
  size_t best = 0;
  int best_diff = -1;
  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {{
    int diff = (int)sizes[i] - (int)size;
    if (diff < 0)
      diff = -diff;
    if (best_diff < 0 || diff < best_diff) {{
      best_diff = diff;
      best = i;
    }}
  }}
  return fonts[best];
}}

#endif // BIP39_ZH_FONTS_H
"""
    path.write_text(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", required=True, type=Path)
    parser.add_argument(
        "--sizes",
        type=int,
        nargs="+",
        default=list(SIZES),
        help="Override which sizes to bake (default: all of SIZES)",
    )
    args = parser.parse_args()

    codepoints = load_codepoints()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    total_bytes = 0
    for size in args.sizes:
        bitmap_len, glyph_count = write_font(
            OUTPUT_DIR / f"bip39_zh_{size}.c", args.font, size, codepoints
        )
        write_decl_header(OUTPUT_DIR / f"bip39_zh_{size}.h", size)
        total_bytes += bitmap_len
        print(f"size {size}px: {glyph_count} glyphs, {bitmap_len} bitmap bytes")

    write_lookup_header(OUTPUT_DIR / "bip39_zh_fonts.h", args.sizes)

    print(f"total bitmap bytes across baked sizes: {total_bytes}")


if __name__ == "__main__":
    main()
