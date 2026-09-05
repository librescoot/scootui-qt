# Librescoot ScootUI Qt

Part of the [Librescoot](https://librescoot.org/) open-source platform.

## Overview

ScootUI Qt is the Qt 6/QML dashboard application for Librescoot DBC targets. It
presents vehicle telemetry, controls, navigation, and system status from the
Redis-compatible vehicle datastore. The desktop build includes an optional
simulator panel for development without a vehicle.

## Capabilities

- Renders cluster, map, navigation setup, fault, maintenance, update, and
  system-information screens.
- Synchronizes vehicle, motor, battery, GPS, navigation, connectivity, OTA,
  and settings state from the datastore.
- Provides vector-map rendering, offline map metadata and update handling, road
  information, routing, rerouting, saved locations, and recent destinations.
- Supports light, dark, and automatic themes; localization; dashboard input;
  fault and connection feedback; dashboard sound cues; and a desktop simulator.
- Publishes dashboard readiness and map metadata to the datastore when a
  connection is available.

## Operation and interfaces

On a deployed DBC, ScootUI Qt is started by `scootui-qt.service`, normally under
`dbc-dispatcher`. It uses a Redis-compatible datastore for state and settings,
and a Valhalla endpoint for routing. The application also listens on the
`scootui:command` channel for these map commands:

| Payload | Action |
| --- | --- |
| `map-check` | Check for map updates |
| `map-download` | Start a map download |
| `map-cancel` | Cancel a map download |
| `map-reload` | Reload installed map tiles |

Use the normal dashboard UI or platform tooling for routine operations. The
commands above are intended for controlled operational automation.

## Configuration

### Environment

| Variable | Default | Meaning |
| --- | --- | --- |
| `SCOOTUI_REDIS_HOST` | `192.168.7.1:6379` | Datastore host, optionally with port; `none` selects the in-memory backend |
| `SCOOTUI_SIMULATOR` | follows backend | `1`, `true`, `on`, or `yes` enables the simulator panel; `0`, `false`, `off`, or `no` disables it |
| `SCOOTUI_RESOLUTION` | `480x480` | Positive `WIDTHxHEIGHT` display resolution; the UI scales from the default size |

With `SCOOTUI_REDIS_HOST=none`, the in-memory backend is used and the simulator
panel is enabled by default. `SCOOTUI_SIMULATOR=1` can instead show the panel
while using a real datastore; it does not automatically seed that datastore.

### Runtime settings and data

Runtime preferences are read from the `settings` hash, including dashboard
mode, theme, language, display options, map source and rendering options,
routing endpoint, and map update preferences. Vehicle state is read from the
corresponding datastore hashes and channels; ScootUI Qt does not replace the
services that own that state. The source settings store is the implementation
reference for the keys consumed by a given release.

## Build and test

CMake requires C++17, CMake 3.16 or newer, Qt 6.4 or newer with Quick, QML,
SVG, Network, SQL, Concurrent, and Multimedia, plus `pkg-config`, hiredis, zlib, zstd, and
QMapLibre. QMapLibre is required for a target-equivalent build; desktop mode
can run without it, with map rendering disabled.

```sh
# Desktop development build and simulator-backed launch
./run-desktop.sh

# Explicit desktop build and test suite
cmake -S . -B build -DDESKTOP_MODE=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The Makefile configures a Debug build in `build/`; `make build`, `make run`,
and `make clean` are convenience targets. `cross-build.sh [Release|Debug]`
performs the repository's Docker-based ARM cross-build workflow.

For on-device visual testing and an isolated datastore workflow, see
[TESTING.md](TESTING.md).

## Deployment and runtime dependencies

The Yocto recipe installs the executable as `/usr/bin/scootui-qt`, map glyph
assets under `/usr/share/scootui/glyphs`, and a disabled-by-default systemd
unit. The i.MX6 unit uses Qt's `eglfs_kms` integration and
`/etc/scootui-qt-kms.json`; the Raspberry Pi 4 variant sets a 1024 × 600
resolution. The deployed application requires a suitable Qt 6 runtime,
QMapLibre, Qt Multimedia with an available audio output, hiredis, zlib, zstd, a Redis-compatible datastore, and, for routing,
a reachable Valhalla endpoint and appropriate map data.

Operate it through systemd on the target:

```sh
systemctl enable --now scootui-qt.service
journalctl -u scootui-qt.service -f
```

## Operational notes

The service runs as root and owns the display through EGLFS/KMS. Do not run a
second instance against the same display. Map-update automation depends on
connectivity, installed map data, and the relevant runtime settings; monitor the
service journal and datastore state when diagnosing it. Use an isolated
Redis-compatible instance for development experiments rather than writing to a
live vehicle datastore.

## License

This project is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](LICENSE).

Made with ❤️ by the Librescoot community
