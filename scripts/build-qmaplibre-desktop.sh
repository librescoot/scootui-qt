#!/bin/bash
# Builds the production-pinned QMapLibre for desktop development, so the map
# screen renders instead of falling back to a placeholder.
#
#   scripts/build-qmaplibre-desktop.sh [--clean]
#
# The device image builds its own QMapLibre from the same recipe; this only
# prepares a host install. Upstream QMapLibre renders black: scootui-qt needs
# the patches that add maplibre.map.style_json, styleLoadingStarted and
# firstStyledFrameRendered, and needs the pre-OpenGL-ES-3 revision.
set -euo pipefail

QMAPLIBRE_REV=10c6d828bab661330cf9cb08d4d3bb9defb74582
RECIPE_REV=f0c3da131988a421e24a8be099a888cb2cbf5b20
QMAPLIBRE_REPO="${QMAPLIBRE_REPO:-https://github.com/maplibre/maplibre-native-qt.git}"
META_REPO="${META_REPO:-librescoot/meta-librescoot}"

QT_DIR="${QT_DIR:-$HOME/Qt/6.9.3/gcc_64}"
ROOT="${QMAPLIBRE_ROOT:-$HOME/.local/share/qmaplibre-pinned}"
SRC="${QMAPLIBRE_SRC_DIR:-$ROOT/src}"
PREFIX="${QMAPLIBRE_DIR:-$ROOT/install}"
JOBS="${JOBS:-$(nproc)}"
if [ "$JOBS" -gt 8 ]; then JOBS=8; fi

PATCHES=(
    0001-disable-tests
    0002-fill-extrusion-position-only-depth-prepass
    0003-signal-the-first-styled-map-frame
    0004-accept-inline-style-json
)
PATCH_SUBDIR=recipes-graphics/qmaplibre/files

die() { echo "error: $*" >&2; exit 1; }

CLEAN=0
if [ "${1:-}" = "--clean" ]; then CLEAN=1; fi

[ -d "$QT_DIR" ] || die "Qt not found at $QT_DIR (set QT_DIR)"
command -v cmake >/dev/null || die "cmake not found"
command -v git >/dev/null || die "git not found"

# Patches come from the device recipe, so there is one source of truth. A local
# meta-librescoot checkout avoids the download and picks up local patch edits.
PATCH_DIR="$ROOT/patches"
if [ -n "${META_LIBRESCOOT_DIR:-}" ]; then
    PATCH_DIR="$META_LIBRESCOOT_DIR/$PATCH_SUBDIR"
    [ -d "$PATCH_DIR" ] || die "no $PATCH_SUBDIR in $META_LIBRESCOOT_DIR"
else
    mkdir -p "$PATCH_DIR"
    for p in "${PATCHES[@]}"; do
        if [ -s "$PATCH_DIR/$p.patch" ]; then continue; fi
        url="https://raw.githubusercontent.com/$META_REPO/$RECIPE_REV/$PATCH_SUBDIR/$p.patch"
        curl -fsSL "$url" -o "$PATCH_DIR/$p.patch" \
            || die "could not download $p.patch from $url"
    done
fi

if [ "$CLEAN" = 1 ]; then
    echo "== clean"
    rm -rf "$SRC"
fi

if [ ! -d "$SRC/.git" ]; then
    echo "== clone $QMAPLIBRE_REPO"
    rm -rf "$SRC"
    git clone --quiet "$QMAPLIBRE_REPO" "$SRC"
fi

# Always start from pristine sources so repeated runs are byte-identical.
# The cleans are needed because some patches add files: a force checkout reverts
# tracked edits but leaves those behind, and 0002 adds one inside the submodule.
# build/ is excluded so a rebuild stays incremental.
cd "$SRC"
git fetch --quiet origin "$QMAPLIBRE_REV" 2>/dev/null || true
git checkout --force --quiet "$QMAPLIBRE_REV"
git clean -fdq -e build
git submodule update --init --recursive --force --quiet
git submodule foreach --recursive --quiet 'git clean -fdq'
echo "== source at $(git rev-parse --short HEAD), vendored $(git -C vendor/maplibre-native rev-parse --short HEAD 2>/dev/null || echo '?')"

echo "== apply recipe patches"
for p in "${PATCHES[@]}"; do
    git apply "$PATCH_DIR/$p.patch" || die "$p.patch did not apply"
done

echo "== configure"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DQT_FIND_PRIVATE_MODULES=ON \
    -DMLN_QT_WITH_LOCATION=ON \
    -DMLN_QT_WITH_WIDGETS=OFF \
    -DMLN_WITH_OPENGL=ON \
    -DBUILD_TESTING=OFF >/dev/null

echo "== build (-j$JOBS)"
cmake --build build --parallel "$JOBS" >/dev/null

echo "== install to $PREFIX"
cmake --install build >/dev/null

[ -f "$PREFIX/lib/cmake/QMapLibre/QMapLibreConfig.cmake" ] \
    || die "install is missing QMapLibreConfig.cmake"
# Guards against an unpatched build, which starts fine and renders black.
for s in styleLoadingStarted firstStyledFrameRendered; do
    grep -rqs "$s" "$PREFIX/qml" || die "installed plugin lacks $s: not the patched revision"
done

echo
echo "QMapLibre ready at $PREFIX"
echo "To pin it explicitly instead of relying on the default:"
echo "  echo 'QMAPLIBRE_DIR=$PREFIX' >> .env"
