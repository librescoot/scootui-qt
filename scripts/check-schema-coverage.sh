#!/usr/bin/env bash
#
# Check that user-visible dashboard settings have a corresponding
# // @schema <key> annotation in the source.
#
# The check is one-directional. Annotating a setting that is not user-visible
# is fine and is not reported: the marker records which key backs a field, and
# a field can back a key nobody is meant to change. Only an annotation naming
# a key the schema does not have at all is worth a word, since it covers
# nothing and is usually a typo or a key that has since been removed.
#
set -euo pipefail

usage() {
    echo "Usage: $0 --schema <path-to-settings.schema.json> [--src <path-to-src>]"
    exit 1
}

SCHEMA=""
SRC="./src"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --schema) SCHEMA="$2"; shift 2 ;;
        --src)    SRC="$2";    shift 2 ;;
        *)        usage ;;
    esac
done

[[ -z "$SCHEMA" ]] && usage
[[ ! -f "$SCHEMA" ]] && { echo "Error: schema file not found: $SCHEMA"; exit 1; }
[[ ! -d "$SRC" ]] && { echo "Error: source directory not found: $SRC"; exit 1; }

# This local-only security setting is visible in schema clients but cannot be
# changed through the dashboard. Keep the exception limited to its read-only form.
mapfile -t VISIBLE_KEYS < <(
    jq -r 'to_entries[]
        | select(.value["user-visible"] == true)
        | select((.key == "bluetooth.data-browser" and .value["read-only"] == true) | not)
        | .key' "$SCHEMA" | sort
)

# Every key the schema defines, whatever its visibility.
mapfile -t ALL_KEYS < <(jq -r 'keys[]' "$SCHEMA" | sort)

# Collect annotated keys from source
mapfile -t ANNOTATED_KEYS < <(
    grep -rh '// @schema ' "$SRC" \
        | sed 's|.*// @schema ||' \
        | tr -d '\r' \
        | sort -u
)

ERRORS=0
WARNINGS=0

# Check for uncovered visible keys
for key in "${VISIBLE_KEYS[@]}"; do
    found=false
    for ak in "${ANNOTATED_KEYS[@]}"; do
        if [[ "$ak" == "$key" ]]; then
            found=true
            break
        fi
    done
    if ! $found; then
        echo "ERROR: user-visible setting '$key' has no @schema annotation in $SRC"
        # Use `:` so set -e doesn't exit when post-increment returns 0
        # (((x++)) evaluates to the PRE-increment value; under set -e a 0
        # return status aborts the script before we finish counting).
        : $((ERRORS++))
    fi
done

# Check for annotations naming a key the schema does not define. Annotating a
# non-user-visible key is deliberate and not reported.
for ak in "${ANNOTATED_KEYS[@]}"; do
    found=false
    for key in "${ALL_KEYS[@]}"; do
        if [[ "$key" == "$ak" ]]; then
            found=true
            break
        fi
    done
    if ! $found; then
        echo "WARNING: @schema annotation '$ak' names no key in the schema"
        : $((WARNINGS++))
    fi
done

echo ""
echo "Schema coverage: ${#VISIBLE_KEYS[@]} dashboard settings required, ${#ANNOTATED_KEYS[@]} annotated keys"
echo "Errors: $ERRORS, Warnings: $WARNINGS"

[[ $ERRORS -gt 0 ]] && exit 1
exit 0
