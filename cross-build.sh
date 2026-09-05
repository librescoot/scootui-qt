#!/bin/bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="scootui-crossbuild"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="$SCRIPT_DIR/build-armhf"
DEPLOY_DIR="$SCRIPT_DIR/deploy-armhf"
CONFIG_LOG="$BUILD_DIR/configure.log"

# The image builds QMapLibre from source, which is most of its build time. Set
# SCOOTUI_ALLOW_MISSING_QMAPLIBRE=1 to build against an image that does not have
# it: you get a binary whose map screen is a placeholder, which is fine for
# startup or cluster work and not fine for anything else.
CMAKE_MAPS_FLAG=""
if [ "${SCOOTUI_ALLOW_MISSING_QMAPLIBRE:-0}" != "0" ]; then
    CMAKE_MAPS_FLAG="-DSCOOTUI_ALLOW_MISSING_QMAPLIBRE=ON"
fi

echo "=== Building cross-compilation Docker image ==="
docker build \
    --platform linux/amd64 \
    -t "$IMAGE_NAME" \
    -f "$SCRIPT_DIR/docker/Dockerfile.crossbuild" \
    "$SCRIPT_DIR/docker"

# Every dependency lives in the image, and a find_package miss is cached as
# NOTFOUND forever. So when the image changes, the old cache is worse than no
# cache: it keeps reporting the packages that image now has as missing.
IMAGE_ID="$(docker image inspect -f '{{.Id}}' "$IMAGE_NAME")"
STAMP_FILE="$BUILD_DIR/.crossbuild-image-id"
if [ -f "$BUILD_DIR/CMakeCache.txt" ] && [ "$(cat "$STAMP_FILE" 2>/dev/null)" != "$IMAGE_ID" ]; then
    echo "=== Cross-build image changed, discarding stale CMake cache ==="
    # Trees left by older revisions of this script are root-owned, because the
    # build container ran as root. Fall back to deleting from inside a
    # container, which still is.
    rm -rf "$BUILD_DIR" 2>/dev/null || \
        docker run --rm -v "$BUILD_DIR:/build" "$IMAGE_NAME" \
            bash -c 'rm -rf /build/* /build/.[!.]*'
fi
mkdir -p "$BUILD_DIR"

echo "=== Cross-compiling scootui for armhf (i.MX6) ==="
docker run --rm \
    --platform linux/amd64 \
    --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$SCRIPT_DIR:/src:ro" \
    -v "$BUILD_DIR:/build" \
    "$IMAGE_NAME" \
    bash -c "
        cmake -S /src -B /build \
            -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=/src/cmake/CrossCompile-armhf.cmake \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            $CMAKE_MAPS_FLAG \
            -Wno-dev 2>&1 | tee /build/configure.log && \
        cmake --build /build -j\$(nproc)
    "
echo "$IMAGE_ID" > "$STAMP_FILE"

if grep -q "QMapLibre found" "$CONFIG_LOG" 2>/dev/null; then
    HAS_MAPS=1
else
    HAS_MAPS=0
fi

echo "=== Bundling shared libraries ==="
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR/lib"

# Copy binary
cp "$BUILD_DIR/bin/scootui" "$DEPLOY_DIR/"

# Recursively resolve and copy ALL shared library dependencies from the Docker image
docker run --rm \
    --platform linux/amd64 \
    --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$DEPLOY_DIR:/deploy" \
    "$IMAGE_NAME" \
    bash -c '
        LIB_DIR="/usr/lib/arm-linux-gnueabihf"
        QT_PLUGIN_DIR="$LIB_DIR/qt6/plugins"
        QML_DIR="$LIB_DIR/qt6/qml"

        # Libs that are guaranteed to exist on any Linux target (skip these)
        SYSTEM_LIBS="linux-vdso|ld-linux|libc.so|libm.so|libpthread|libdl.so|librt.so|libgcc_s|libstdc\+\+"

        # One pass over the library dirs up front. Resolving each NEEDED entry
        # with its own find is what used to make this stage take over an hour:
        # a few hundred plugins times a dozen entries each is thousands of
        # walks over /usr/lib.
        declare -A LIB_INDEX
        while IFS= read -r path; do
            name="${path##*/}"
            [ -n "${LIB_INDEX[$name]:-}" ] || LIB_INDEX["$name"]="$path"
        done < <(find "$LIB_DIR" /usr/lib /lib -maxdepth 2 -name "*.so*" 2>/dev/null)
        echo "Indexed ${#LIB_INDEX[@]} shared libraries"

        # Recursively collect all needed .so files. Runs in this shell, not a
        # pipeline subshell, so the recursion sees what earlier calls copied.
        collect_deps() {
            local file="$1" lib src
            for lib in $(arm-linux-gnueabihf-objdump -p "$file" 2>/dev/null | awk "/NEEDED/ {print \$2}"); do
                echo "$lib" | grep -qE "$SYSTEM_LIBS" && continue
                [ -f "/deploy/lib/$lib" ] && continue
                src="${LIB_INDEX[$lib]:-}"
                if [ -n "$src" ] && [ -f "$src" ]; then
                    cp -L "$src" /deploy/lib/
                    echo "  $lib"
                    collect_deps "/deploy/lib/$lib"
                fi
            done
        }

        echo "Resolving binary dependencies..."
        collect_deps /deploy/scootui

        # Copy Qt6 platform plugins
        echo "Copying plugins..."
        for dir in platforms egldeviceintegrations imageformats multimedia sqldrivers; do
            if [ -d "$QT_PLUGIN_DIR/$dir" ]; then
                mkdir -p /deploy/plugins/$dir
                cp -L "$QT_PLUGIN_DIR/$dir/"*.so /deploy/plugins/$dir/ 2>/dev/null
                echo "  plugins/$dir/*"
            fi
        done

        # QMapLibre installs outside the multiarch tree, in the same place the
        # device has it (/usr/plugins, /usr/qml).
        if [ -d /usr/plugins/geoservices ]; then
            mkdir -p /deploy/plugins/geoservices
            cp -L /usr/plugins/geoservices/*.so /deploy/plugins/geoservices/ 2>/dev/null
            echo "  plugins/geoservices/*"
        fi

        # Resolve plugin dependencies too
        echo "Resolving plugin dependencies..."
        for plugin in $(find /deploy/plugins -name "*.so"); do
            collect_deps "$plugin"
        done

        # Copy QML runtime modules
        echo "Copying QML modules..."
        mkdir -p /deploy/lib/qml
        for mod in QtQuick QtQml QtPositioning QtLocation; do
            if [ -d "$QML_DIR/$mod" ]; then
                cp -rL "$QML_DIR/$mod" /deploy/lib/qml/
                echo "  qml/$mod"
            fi
        done
        # Top-level QML files (qtquick2plugin etc.)
        for f in "$QML_DIR"/*.so "$QML_DIR"/qmldir "$QML_DIR"/plugins.qmltypes; do
            [ -f "$f" ] && cp -L "$f" /deploy/lib/qml/
        done
        # Additional QML modules installed in separate dirs
        for mod in Layouts Shapes Window; do
            if [ -d "$QML_DIR/$mod" ]; then
                cp -rL "$QML_DIR/$mod" /deploy/lib/qml/
                echo "  qml/$mod"
            fi
        done

        # MapLibre.Location, which MapViewContent.qml imports. Landing it in
        # the same import dir keeps the launcher to one QML2_IMPORT_PATH entry,
        # and its RUNPATH ($ORIGIN/../../lib) still resolves to /deploy/lib.
        if [ -d /usr/qml/MapLibre ]; then
            cp -rL /usr/qml/MapLibre /deploy/lib/qml/
            echo "  qml/MapLibre"
        fi

        # Resolve QML plugin dependencies
        echo "Resolving QML plugin dependencies..."
        for qmlplugin in $(find /deploy/lib/qml -name "*.so"); do
            collect_deps "$qmlplugin"
        done

        # Remove EGL/GL/X11/Wayland libs — use the target system libs instead
        echo "Removing GPU/display libs (use target system libs)..."
        rm -f /deploy/lib/libEGL.so* /deploy/lib/libGLESv2.so* \
              /deploy/lib/libGLX.so* /deploy/lib/libOpenGL.so* \
              /deploy/lib/libGLdispatch.so* /deploy/lib/libglapi.so* \
              /deploy/lib/libgbm.so* /deploy/lib/libdrm.so* \
              /deploy/lib/libwayland*.so* \
              /deploy/lib/libX11*.so* /deploy/lib/libxcb*.so* \
              /deploy/lib/libxkb*.so* /deploy/lib/libSM.so* /deploy/lib/libICE.so*

        # Strip everything
        echo "Stripping binaries..."
        arm-linux-gnueabihf-strip --strip-unneeded /deploy/scootui 2>/dev/null
        find /deploy -name "*.so*" -exec arm-linux-gnueabihf-strip --strip-unneeded {} \; 2>/dev/null
        echo "Done"
    '

# Create launcher script
cat > "$DEPLOY_DIR/run-scootui.sh" << 'LAUNCHER'
#!/bin/sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
export QML2_IMPORT_PATH="$SCRIPT_DIR/lib/qml"
export QML_IMPORT_PATH="$SCRIPT_DIR/lib/qml"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-eglfs}"
export QT_QPA_EGLFS_INTEGRATION="${QT_QPA_EGLFS_INTEGRATION:-eglfs_kms}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime-$(id -u)}"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 0700 "$XDG_RUNTIME_DIR"
exec "$SCRIPT_DIR/scootui" "$@"
LAUNCHER
chmod +x "$DEPLOY_DIR/run-scootui.sh"

echo ""
echo "=== Build complete ==="
echo "Deploy directory: $DEPLOY_DIR"
file "$DEPLOY_DIR/scootui"
echo ""
TOTAL=$(du -sh "$DEPLOY_DIR" | cut -f1)
echo "Total deploy size: $TOTAL"
echo ""
if [ "$HAS_MAPS" = "0" ]; then
    echo "!!! REDUCED BUILD: no QMapLibre in the cross-build image, so the map"
    echo "!!! screen falls back to its placeholder. Everything else (startup,"
    echo "!!! cluster, menus, Redis) behaves normally. Do not ship this."
    echo ""
fi
echo "Built against Debian Qt $(docker run --rm --platform linux/amd64 "$IMAGE_NAME" \
    dpkg-query -f '${Version}' -W qt6-base-dev:armhf 2>/dev/null | cut -d+ -f1), not the DBC's Qt."
echo "It runs from its own bundled libs (LD_LIBRARY_PATH), so it does not use"
echo "the DBC's Qt at all. Timing numbers are indicative, not exact."
echo ""
echo "To deploy: scp -r $DEPLOY_DIR/* target:/opt/scootui/"
echo "To run:    /opt/scootui/run-scootui.sh"
