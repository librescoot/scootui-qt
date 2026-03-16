#!/bin/bash

QT_DIR="${QT_DIR:-$HOME/Qt/6.9.3/gcc_64}"

cmake -B build-desktop -DDESKTOP_MODE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build build-desktop -j$(nproc)

export LD_LIBRARY_PATH="$QT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$QT_DIR/plugins"
export QML_IMPORT_PATH="$QT_DIR/qml"

SCOOTUI_REDIS_HOST="${SCOOTUI_REDIS_HOST:-none}" ./build-desktop/bin/scootui "$@"
