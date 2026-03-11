#!/bin/bash

cmake -B build -DDESKTOP_MODE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build -j$(nproc)
LD_LIBRARY_PATH=/usr/local/lib QT_PLUGIN_PATH=/usr/local/plugins SCOOTUI_REDIS_HOST=none ./build/bin/scootui
