#!/bin/bash
# Subset the bundled fonts down to only the glyphs we actually use.
#
# Reads the pristine originals from assets/fonts/ and writes subsetted
# copies to assets/fonts/subset/. The QRC resource references the
# subset/ copies, so the binary only carries the smaller versions
# (~200 KB total instead of ~2.7 MB).
#
# Re-run after an upstream font update or after adding a new
# MaterialIcon codepoint — the subset/ directory is meant to be kept
# in git alongside the originals so the build doesn't need pyftsubset.
#
# Upstream sources:
#   Roboto, RobotoCondensed: https://fonts.google.com/specimen/Roboto
#                            https://github.com/googlefonts/roboto
#   MaterialIcons:           https://github.com/google/material-design-icons
#                            (android/sources/MaterialIcons-Regular.otf for
#                            the legacy PUA codepoint mapping used in QML)
#
# Dependencies: pyftsubset (fonttools). Install with `pip install fonttools`
#   or `apt install python3-fonttools`.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FONTS_DIR="${REPO_ROOT}/assets/fonts"
SUBSET_DIR="${FONTS_DIR}/subset"
MATERIAL_QML="${REPO_ROOT}/qml/widgets/components/MaterialIcon.qml"

if ! command -v pyftsubset >/dev/null; then
    echo "error: pyftsubset not found on PATH (install fonttools)" >&2
    exit 1
fi

if [[ ! -f "${MATERIAL_QML}" ]]; then
    echo "error: ${MATERIAL_QML} not found — cannot extract MaterialIcon codepoints" >&2
    exit 1
fi

# Extract MaterialIcon codepoints from the QML singleton. Both formats used:
#   String.fromCodePoint(0xfXXXX)  — supplementary plane
#   "\ueXXX"                       — BMP
# We normalise both to uppercase hex without prefix.
MATERIAL_CODEPOINTS=$(
    grep -oE '0x[0-9a-fA-F]+|\\u[0-9a-fA-F]{4}' "${MATERIAL_QML}" \
        | sed -E 's/^0x//I; s/^\\u//' \
        | tr '[:lower:]' '[:upper:]' \
        | sort -u
)

if [[ -z "${MATERIAL_CODEPOINTS}" ]]; then
    echo "error: no MaterialIcon codepoints extracted from ${MATERIAL_QML}" >&2
    exit 1
fi

MATERIAL_COUNT=$(echo "${MATERIAL_CODEPOINTS}" | wc -l)
# Always include U+0020 (space) as a fallback glyph.
MATERIAL_UNICODES="0020,$(echo "${MATERIAL_CODEPOINTS}" | paste -s -d , -)"

# Roboto/RobotoCondensed:
#   - Basic Latin (ASCII): 0020-007E
#   - Latin-1 Supplement: 00A0-00FF
#       umlauts (ä ö ü ß + caps), common symbols (° © ± ·, etc.)
#   - Curated typography glyphs used in translations and widgets:
#       2013 en-dash (–), 2014 em-dash (—)
#       2018/2019 curly single quotes (‘ ’)
#       201C/201D curly double quotes (“ ”)
#       201E German opening double quote („)
#       2022 bullet (•)
#       2026 horizontal ellipsis (…)
#       203A single right guillemet (›) — address breadcrumb separator
#       2264 less-or-equal (≤) — battery display
#
# Roboto does not ship glyphs for → ↔ ⊥ ␣ ─ (used in arrow/scale widgets);
# those rely on Qt's font fallback at render time.
#
# Keep this as a curated list rather than a wide range (like 2000-206F)
# so the subset stays small. When adding a new non-ASCII glyph to UI
# strings or widgets, check whether Roboto has it (see above), extend
# this list if so, and re-run the subsetter.
#
# If map-data place names in Polish/Czech/Hungarian appear on-screen we'll
# need to add Latin Extended-A (0100-017F, adds ~5 KB/variant).
ROBOTO_UNICODES="0020-007E,00A0-00FF,2013,2014,2018,2019,201C,201D,201E,2022,2026,203A,2264"

echo "=== scootui-qt font subsetter ==="
echo "MaterialIcon codepoints (${MATERIAL_COUNT} used):"
echo "${MATERIAL_CODEPOINTS}" | column
echo

mkdir -p "${SUBSET_DIR}"

subset() {
    local basename="$1" unicodes="$2"
    local input="${FONTS_DIR}/${basename}"
    local output="${SUBSET_DIR}/${basename}"
    if [[ ! -f "${input}" ]]; then
        echo "error: ${input} not found" >&2
        exit 1
    fi
    pyftsubset "${input}" \
        --unicodes="${unicodes}" \
        --no-ignore-missing-glyphs \
        --output-file="${output}"
    local size_before size_after
    if stat -c %s "${input}" >/dev/null 2>&1; then
        # GNU stat
        size_before=$(stat -c %s "${input}")
        size_after=$(stat -c %s "${output}")
    else
        # BSD stat (macOS)
        size_before=$(stat -f %z "${input}")
        size_after=$(stat -f %z "${output}")
    fi
    printf "  %-32s  %8d -> %6d bytes  (%2d%% of original)\n" \
        "${basename}" \
        "${size_before}" "${size_after}" \
        "$(( 100 * size_after / size_before ))"
}

echo "Subsetting Roboto family..."
subset Roboto-Regular.ttf          "${ROBOTO_UNICODES}"
subset Roboto-Bold.ttf             "${ROBOTO_UNICODES}"
subset Roboto-Medium.ttf           "${ROBOTO_UNICODES}"
subset Roboto-Light.ttf            "${ROBOTO_UNICODES}"
subset RobotoCondensed-Regular.ttf "${ROBOTO_UNICODES}"
subset RobotoCondensed-Bold.ttf    "${ROBOTO_UNICODES}"

echo
echo "Subsetting MaterialIcons..."
subset MaterialIcons-Regular.otf "${MATERIAL_UNICODES}"

echo
echo "Subset fonts written to ${SUBSET_DIR}."
echo "Commit both the originals and the subset/ copies; the QRC references"
echo "the subset/ versions."
