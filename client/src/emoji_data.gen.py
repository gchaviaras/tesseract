#!/usr/bin/env python3
"""
Generate client/src/emoji_data.inc from a Unicode emoji-test.txt file.

Input format is the official UTS #51 keyboard-display test data; we use it
because it's the canonical source for the picker layout (groups + subgroups
+ CLDR short names). We emit a single C++ initializer list — `tesseract::
emoji::all()` returns a static vector built from it.

Filters applied:
  * status == "fully-qualified" only (drops minimally-qualified duplicates
    and unqualified entries that won't render with proper emoji presentation).
  * Sequences containing a skin-tone modifier (1F3FB..1F3FF) are dropped —
    they are emitted separately by --skin-tones (see below) and offered
    through the picker's long-press tone menu instead.
  * Hair-component sequences (1F9B0..1F9B3) are dropped for the same reason.
  * Subgroups "country-flag", "subdivision-flag" are kept; "skin-tone" and
    "hair-style" components are dropped.
  * Group "Component" is dropped entirely (not user-pickable emoji).

Re-run when bumping Unicode emoji version. The committed .inc is the source
of truth at build time; this script is committed for reproducibility only.
When bumping, keep ui/windows/fonts/NotoColorEmoji.ttf at least at the same
Emoji version: Windows trusts it to render the whole table, while the other
platforms detect their font's version at runtime (tk/emoji_support.h).

Usage:
    python3 emoji_data.gen.py path/to/emoji-test.txt > emoji_data.inc
    python3 emoji_data.gen.py --skin-tones path/to/emoji-test.txt > emoji_skin_tones.inc

--skin-tones emits one row per base entry that has all five uniform
skin-tone variants: { base, { light, medium-light, medium, medium-dark,
dark } }. Variants are matched by CLDR name ("<base>: <tone> skin tone"; for
a base name that already has a colon, "<base>, <tone> skin tone" or
"<head>: <tone> skin tone, <qualifier>", e.g. "person: blond hair") rather
than by inserting modifier codepoints, which gets VS16 bases (☝️ → ☝🏻) and
ZWJ sequences right for free. Mixed-tone pairs are not offered.
"""
import re
import sys

# Map Unicode group names → our enum order. Drop "Component" outright.
GROUP_MAP = {
    "Smileys & Emotion":  "SmileysPeople",
    "People & Body":      "SmileysPeople",
    "Animals & Nature":   "AnimalsNature",
    "Food & Drink":       "FoodDrink",
    "Travel & Places":    "TravelPlaces",
    "Activities":         "Activities",
    "Objects":            "Objects",
    "Symbols":            "Symbols",
    "Flags":              "Flags",
}

# Skin-tone modifiers (Fitzpatrick 1-2 through 6) and hair-style components.
DROP_CODEPOINTS = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF,
                   0x1F9B0, 0x1F9B1, 0x1F9B2, 0x1F9B3}

# Subgroups whose entries are not useful as picker items.
DROP_SUBGROUPS = {"skin-tone", "hair-style"}

LINE_RE = re.compile(
    r'^([0-9A-Fa-f ]+?)\s*;\s*fully-qualified\s*#\s*(\S+)\s+E(\d+)\.(\d+)\s+(.+?)\s*$'
)

def version_of(m):
    """Emoji version of a LINE_RE match as major*10 + minor (E12.1 → 121)."""
    return int(m.group(3)) * 10 + int(m.group(4))

def parse(path):
    out = []
    group = subgroup = None
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('# group:'):
                group = line.split(':', 1)[1].strip()
                continue
            if line.startswith('# subgroup:'):
                subgroup = line.split(':', 1)[1].strip()
                continue
            m = LINE_RE.match(line)
            if not m:
                continue
            if group not in GROUP_MAP:
                continue
            if subgroup in DROP_SUBGROUPS:
                continue
            cps = [int(x, 16) for x in m.group(1).split()]
            if any(cp in DROP_CODEPOINTS for cp in cps):
                continue
            glyph = m.group(2)
            name  = m.group(5).strip()
            out.append((GROUP_MAP[group], subgroup, glyph, name, version_of(m)))
    return out

def c_string(s):
    """Emit a C++ string literal for a UTF-8 sequence (or plain ASCII name).

    Non-ASCII bytes pass through verbatim — the source file is UTF-8 and the
    project compiles with the default narrow execution charset (also UTF-8
    under GCC/Clang/MSVC's modern defaults), so a plain "..." literal yields
    the right bytes. We deliberately do NOT use the u8 prefix because in
    C++20 that produces `const char8_t[N]`, which won't bind to the
    `std::string_view` ctor we want.
    """
    esc = s.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{esc}"'

def keywords_from(name, subgroup):
    """Derive a small keyword list from the CLDR short name + subgroup."""
    # Tokenise on whitespace + non-alphanumeric; lowercase; dedupe.
    toks = re.split(r'[^a-zA-Z0-9]+', name.lower() + ' ' + subgroup.replace('-', ' '))
    seen, out = set(), []
    for t in toks:
        if not t or t in seen:
            continue
        seen.add(t)
        out.append(t)
    return ' '.join(out)

TONE_CODEPOINTS = [0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF]
TONE_NAMES = ["light", "medium-light", "medium", "medium-dark", "dark"]
HAIR_CODEPOINTS = {0x1F9B0, 0x1F9B1, 0x1F9B2, 0x1F9B3}

def parse_skin_tones(path):
    """Return [(base_glyph, [5 (variant glyph, version)])] in base display order."""
    bases = [(glyph, name) for _, _, glyph, name, _ in parse(path)]
    by_name = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = LINE_RE.match(line.rstrip('\n'))
            if not m:
                continue
            cps = [int(x, 16) for x in m.group(1).split()]
            tones = {cp for cp in cps if cp in TONE_CODEPOINTS}
            if len(tones) != 1 or any(cp in HAIR_CODEPOINTS for cp in cps):
                continue
            by_name[m.group(5).strip()] = (m.group(2), version_of(m))
    out = []
    def variant_names(name, tone):
        if ':' not in name:
            return [f"{name}: {tone} skin tone"]
        # "kiss: woman, man" → "kiss: woman, man, light skin tone", but
        # "person: blond hair" → "person: light skin tone, blond hair".
        head, qualifier = name.split(': ', 1)
        return [f"{name}, {tone} skin tone",
                f"{head}: {tone} skin tone, {qualifier}"]

    for glyph, name in bases:
        variants = [next((by_name[n] for n in variant_names(name, t) if n in by_name), None)
                    for t in TONE_NAMES]
        if all(variants):
            out.append((glyph, variants))
    return out

def main_skin_tones(path):
    rows = parse_skin_tones(path)
    sys.stdout.write(
        "// AUTO-GENERATED by client/src/emoji_data.gen.py --skin-tones — DO NOT EDIT BY HAND.\n"
        "// Source: Unicode emoji-test.txt (UTS #51, fully-qualified entries only).\n"
        "//\n"
        "// Format: { base_glyph, { {light, version}, {medium_light, version}, ... {dark, version} } }.\n"
        "// Versions are Emoji versions as major*10 + minor (E14.0 → 140); a\n"
        "// variant can be newer than its base.\n"
        "// Count: " + str(len(rows)) + " entries.\n"
        "\n"
    )
    sys.stdout.write("// clang-format off\n")
    for base, variants in rows:
        vs = ", ".join(f"{{{c_string(g)}, {v}}}" for g, v in variants)
        sys.stdout.write(f"  {{{c_string(base)}, {{{vs}}}}},\n")
    sys.stdout.write("// clang-format on\n")

def main(argv):
    if len(argv) == 3 and argv[1] == "--skin-tones":
        main_skin_tones(argv[2])
        return
    if len(argv) != 2:
        sys.stderr.write(__doc__)
        sys.exit(2)
    entries = parse(argv[1])
    sys.stdout.write(
        "// AUTO-GENERATED by client/src/emoji_data.gen.py — DO NOT EDIT BY HAND.\n"
        "// Source: Unicode emoji-test.txt (UTS #51, fully-qualified entries only).\n"
        "// Re-run the generator to refresh; see the script for filters applied.\n"
        "//\n"
        "// Format: { glyph_utf8, name, keywords, Category, shortcode, version }.\n"
        "// version is the Emoji version as major*10 + minor (E12.1 → 121).\n"
        "// Count: " + str(len(entries)) + " entries.\n"
        "\n"
    )
    sys.stdout.write("// clang-format off\n")
    for cat, subgroup, glyph, name, version in entries:
        kw = keywords_from(name, subgroup)
        sc = re.sub(r'_+', '_', re.sub(r'[^a-z0-9_]', '_', name.lower())).strip('_')
        sys.stdout.write(
            f"  {{{c_string(glyph)}, {c_string(name)}, {c_string(kw)}, "
            f"Category::{cat}, {c_string(sc)}, {version}}},\n"
        )
    sys.stdout.write("// clang-format on\n")

if __name__ == "__main__":
    main(sys.argv)
