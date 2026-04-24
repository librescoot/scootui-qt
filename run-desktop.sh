#!/bin/bash

# Optional local overrides (QT_DIR, QMAPLIBRE_DIR, etc.)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ -f "$SCRIPT_DIR/.env" ] && . "$SCRIPT_DIR/.env"

QT_DIR="${QT_DIR:-$HOME/Qt/6.9.3/gcc_64}"
QMAPLIBRE_DIR="${QMAPLIBRE_DIR:-/tmp/qmaplibre-install}"

CMAKE_PREFIX="$QT_DIR"
if [ -d "$QMAPLIBRE_DIR/lib/cmake/QMapLibre" ]; then
    CMAKE_PREFIX="$CMAKE_PREFIX;$QMAPLIBRE_DIR"
fi

cmake -B build-desktop -DDESKTOP_MODE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX"
cmake --build build-desktop -j$(nproc)

export LD_LIBRARY_PATH="$QT_DIR/lib${QMAPLIBRE_DIR:+:$QMAPLIBRE_DIR/lib}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}:/usr/local/lib"
export QT_PLUGIN_PATH="$QT_DIR/plugins${QMAPLIBRE_DIR:+:$QMAPLIBRE_DIR/plugins}:/usr/local/plugins"
export QML_IMPORT_PATH="$QT_DIR/qml${QMAPLIBRE_DIR:+:$QMAPLIBRE_DIR/qml}"

SCOOTUI_REDIS_HOST="${SCOOTUI_REDIS_HOST:-none}" ./build-desktop/bin/scootui "$@"
