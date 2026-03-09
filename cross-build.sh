#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="scootui-crossbuild"
BUILD_TYPE="${1:-Release}"
DEPLOY_DIR="$SCRIPT_DIR/deploy-armhf"

echo "=== Building cross-compilation Docker image ==="
docker build \
    --platform linux/amd64 \
    -t "$IMAGE_NAME" \
    -f "$SCRIPT_DIR/docker/Dockerfile.crossbuild" \
    "$SCRIPT_DIR/docker"

echo "=== Cross-compiling scootui for armhf (i.MX6) ==="
docker run --rm \
    --platform linux/amd64 \
    -v "$SCRIPT_DIR:/src:ro" \
    -v "$SCRIPT_DIR/build-armhf:/build" \
    "$IMAGE_NAME" \
    bash -c "
        cmake -S /src -B /build \
            -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=/src/cmake/CrossCompile-armhf.cmake \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -Wno-dev && \
        cmake --build /build -j\$(nproc)
    "

echo "=== Bundling shared libraries ==="
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR/lib"

# Copy binary
cp "$SCRIPT_DIR/build-armhf/bin/scootui" "$DEPLOY_DIR/"

# Recursively resolve and copy ALL shared library dependencies from the Docker image
docker run --rm \
    --platform linux/amd64 \
    -v "$DEPLOY_DIR:/deploy" \
    "$IMAGE_NAME" \
    bash -c '
        LIB_DIR="/usr/lib/arm-linux-gnueabihf"
        QT_PLUGIN_DIR="$LIB_DIR/qt6/plugins"
        QML_DIR="$LIB_DIR/qt6/qml"

        # Libs that are guaranteed to exist on any Linux target (skip these)
        SYSTEM_LIBS="linux-vdso|ld-linux|libc.so|libm.so|libpthread|libdl.so|librt.so|libgcc_s|libstdc\+\+"

        # Recursively collect all needed .so files
        collect_deps() {
            local file="$1"
            arm-linux-gnueabihf-objdump -p "$file" 2>/dev/null | grep NEEDED | awk "{print \$2}" | while read lib; do
                # Skip system libs
                echo "$lib" | grep -qE "$SYSTEM_LIBS" && continue
                # Skip if already collected
                [ -f "/deploy/lib/$lib" ] && continue
                # Find and copy
                local src=$(find $LIB_DIR /usr/lib /lib -maxdepth 2 -name "$lib" 2>/dev/null | head -1)
                if [ -n "$src" ] && [ -f "$src" ]; then
                    cp -L "$src" /deploy/lib/
                    echo "  $lib"
                    # Recurse into this lib'\''s deps
                    collect_deps "/deploy/lib/$lib"
                fi
            done
        }

        echo "Resolving binary dependencies..."
        collect_deps /deploy/scootui

        # Copy Qt6 platform plugins
        echo "Copying plugins..."
        for dir in platforms egldeviceintegrations imageformats sqldrivers; do
            if [ -d "$QT_PLUGIN_DIR/$dir" ]; then
                mkdir -p /deploy/plugins/$dir
                cp -L "$QT_PLUGIN_DIR/$dir/"*.so /deploy/plugins/$dir/ 2>/dev/null
                echo "  plugins/$dir/*"
            fi
        done

        # Resolve plugin dependencies too
        echo "Resolving plugin dependencies..."
        find /deploy/plugins -name "*.so" | while read plugin; do
            collect_deps "$plugin"
        done

        # Copy QML runtime modules
        echo "Copying QML modules..."
        mkdir -p /deploy/lib/qml
        for mod in QtQuick QtQml; do
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

        # Resolve QML plugin dependencies
        echo "Resolving QML plugin dependencies..."
        find /deploy/lib/qml -name "*.so" | while read qmlplugin; do
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
echo "To deploy: scp -r $DEPLOY_DIR/* target:/opt/scootui/"
echo "To run:    /opt/scootui/run-scootui.sh"
