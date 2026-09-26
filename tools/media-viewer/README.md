# DBC media viewer

Standalone fullscreen mock-up viewer for local MP4, JPEG and PNG files. It does not connect to Redis or start scooter services. Images are displayed until stopped; video loops by default. Aspect ratio is preserved with black bars unless `--stretch` is supplied. MP4 decoding uses the `ffmpeg` executable on the DBC, so the available codecs depend on its build. Press Ctrl-C to exit from the launching terminal.

## Build

For a desktop with Qt 6 Widgets development files:

```sh
cmake -S tools/media-viewer -B /tmp/dbc-media-viewer-build
cmake --build /tmp/dbc-media-viewer-build
```

For the ARMv7 cross-build container used by `cross-build.sh`:

```sh
docker build --platform linux/amd64 -t scootui-crossbuild -f docker/Dockerfile.crossbuild docker
mkdir -p /tmp/dbc-media-viewer-arm-build
docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$PWD:/src:ro" -v /tmp/dbc-media-viewer-arm-build:/build scootui-crossbuild \
  sh -c 'cmake -S /src/tools/media-viewer -B /build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/src/cmake/CrossCompile-armhf.cmake -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build -j4'
```

Run `dbc-media-viewer [--once] [--stretch] /path/to/mockup.mp4` (or `.jpg`, `.jpeg`, `.png`). For an MP4, use H.264/yuv420p for broad DBC compatibility. The native display is 480×480.

## Run on the DBC

Copy the ARM binary and media to `/data/` on the DBC. The application needs exclusive display access; stop `dbc-dispatcher` **before** stopping `scootui-qt`, so it does not restart the dashboard. In a DBC shell:

```sh
systemctl stop dbc-dispatcher scootui-qt
LANG=C.UTF-8 XDG_RUNTIME_DIR=/run/user/0 XDG_CACHE_HOME=/var/volatile/cache \
  QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_INTEGRATION=eglfs_kms \
  QT_QPA_EGLFS_KMS_CONFIG=/etc/scootui-qt-kms.json \
  QT_QPA_EGLFS_KMS_ATOMIC=1 QT_QPA_EGLFS_DISABLE_INPUT=1 \
  /data/dbc-media-viewer /data/mockup.mp4
# After exiting the viewer:
systemctl start dbc-dispatcher
```

Do not start a second EGLFS application while the viewer owns the display. If the SSH session is lost, stop the viewer with `pkill -f '^/data/dbc-media-viewer '` before restarting the dispatcher.

### Without the viewer binary

The DBC's `ffmpeg` can write directly to `/dev/fb0`. This is sufficient for quick mock-ups, but has no image controls or application-level error handling. Stop the dispatcher and dashboard as above. The DRM framebuffer on this DBC accepts `bgra` (not `rgb565le`):

```sh
# Loop an MP4 until Ctrl-C:
ffmpeg -hide_banner -loglevel error -nostdin -re -stream_loop -1 \
  -i /data/mockup.mp4 -an \
  -vf 'scale=480:480:force_original_aspect_ratio=decrease,pad=480:480:(ow-iw)/2:(oh-ih)/2' \
  -pix_fmt bgra -f fbdev /dev/fb0

# Hold a PNG or JPEG until Ctrl-C:
ffmpeg -hide_banner -loglevel error -nostdin -loop 1 -framerate 1 -re \
  -i /data/mockup.png \
  -vf 'scale=480:480:force_original_aspect_ratio=decrease,pad=480:480:(ow-iw)/2:(oh-ih)/2' \
  -pix_fmt bgra -f fbdev /dev/fb0

systemctl start dbc-dispatcher
```
