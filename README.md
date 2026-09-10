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

### HMI responsiveness

Periodic road matching runs on a value-snapshot worker, with at most one active
job and one replaceable pending job. Superseded results are rejected. Tile-derived
road metadata expires three seconds after its last accepted publication; current
route and external values are preserved independently.

The journal reports `HMI latency:` when the GUI timer is at least 50 ms late or
synchronization-to-swap time reaches 100 ms. Reports are limited to one per ten
seconds and do not request extra frames. Swap gaps include idle time and alone do
not indicate a stall. `RoadMatch:` reports aged/rejected snapshots and replaced
pending work under load.

Roundabout street tile reads, gzip/protobuf decoding and geographic line extraction
run on a separate low-priority worker, not on the GUI or periodic matcher worker.
The service prefetches the **current leading roundabout pair** when navigation
publishes its render geometry, irrespective of distance or TBT creation; the icon
still activates at the existing 500 m threshold. It does not scan all later turns.
One completed result is cached by exact render geometry, bbox, path and map generation;
activation reuses that cache or promotes the matching in-flight prefetch. Visible
demand replaces pending speculation. There is at most one active job/result and
one replaceable pending request, and route/map changes discard obsolete work/results.
Icon destruction cancels its consumer without waiting.

Cold, missing or slow tiles do not delay TBT or a valid route-derived ring/arrow.
Until usable ring geometry exists the schematic icon remains available; partial
same-turn geometry survives missing-tile replies. Active icons retry incomplete
queries every two seconds, without overlapping requests. Icon queries cover at
most 3×3 zoom-14 tiles, cap each compressed/decoded tile at 4/16 MiB and the returned
geometry at 2,048 lines / 32,768 points. Oversized output falls back rather than
publishing a truncated ring. SQLite lock waiting is limited to 100 ms on tile
workers; this is **not** a deadline for storage reads.

Sound device discovery, asset loading, playback and effect cleanup run on a separate
audio thread. Each effect retains its normal restart behavior; different effects
can overlap. A bounded mailbox holds at most one pending request per cue, with only
the latest pending vehicle-state cue and blinker edge retained. Pulses delayed by
100 ms or more are discarded rather than replayed off-beat. Calls before an effect
is created are dropped, as before. Under overload repeated pending cues coalesce;
other cue types retain their relative submission order. Audio cannot block the GUI
through backend calls, but actual sound onset is still subject to backend delays.

Ordinary road/audio-service destruction requests cancellation without waiting.
Final process teardown joins these workers before destroying Qt services;
pathological storage reads, indivisible parsing or audio-driver calls can still
delay shutdown.
Circle fitting, arm selection/sorting, pixel projection and Shape/Canvas updates
remain on the GUI thread, bounded by the returned street geometry but not a frame
time budget; size changes reuse streets without another tile query. Synchronous
address lookup and other HMI work remain outside this coverage.
Prefetch is best effort: late route publication or slow storage can still require
the immediate fallback. This is not a hard real-time guarantee or CPU-core reservation.

## License

This project is licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](LICENSE).

Made with ❤️ by the Librescoot community
