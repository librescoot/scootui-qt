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

## Unified notifications

The dashboard uses one production notification registry and attention policy for
navigation, warnings, connection health, faults, coverage, and transient
feedback. Active conditions are updated and resolved by their owning producer;
transient events expire by a monotonic freshness deadline and are retained in a
bounded in-memory history. Priority is **error/critical > warning > navigation >
success > info > debug**, independent of maneuver distance. Navigation stays
primary while success/info/debug notifications use the secondary slot, including
while ready-to-drive. Errors and warnings preempt navigation; valid navigating or
arrived guidance remains as a compact TurnByTurnWidget beneath the alert.

Equal-priority notifications rotate every 5 monotonic seconds within the highest
eligible group in each notification slot. Updates do not restart the current
dwell. Rotation and preemption/resumption do not replay cues or extend event
freshness. Additional queued notifications are counted by severity on the topmost
banner (including a topmost TurnByTurnWidget), inline rather than in an extra row:
red error/critical, amber warning, green success, blue info, grey debug. Neither visible slot is counted; counts
follow dismissal, expiry and surface eligibility. Error and critical share a
count. Navigation itself is not a queued notification.
Speed, blinkers, telltales, and pairing/UMS/system workflows remain independent
protected surfaces.

Attention overlays the normal full-size cluster and map; it does not reserve
instrument space or change the map camera or vehicle anchor. Only blinkers move
below the overlay to stay visible. QML renders notifications and navigation with
dedicated widgets from the shared selection payload, including actionable turn
instructions and a compact navigation companion when an alert takes priority.
When both slots are visible, the actual shared navigation widget uses its compact
layout in either slot. Notification cards use the same translucent dark/light
background as navigation, without a left accent stripe; icons, text and queued
counts retain severity colors. Their titles are slightly larger/bolder than the
navigation text. Two-slot cards limit body text to one elided line and titles to
two lines so the overlay stays clear of the unchanged speed readout.
Events still expire while queued: a sufficiently large or higher-priority queue
can outlast an event's TTL; displaying it never renews its deadline.

Calculating/recalculating uses the shared navigation banner, navigation icon and
loading indicator. Arrival retains the route's actual final maneuver icon and
uses localized present-tense destination wording, including its typed left/right
side (or an unsided fallback). A once-per-route “You have arrived” success event
lasts 10 seconds from publication. GPS updates, recalculation and store reconnects
do not republish it. Guidance remains until the normal route-clear/end lifecycle;
an explicit new destination/route resets arrival and clears the old success event.

### External notifications over Redis

Publish a JSON object on `scootui:notification` on the Redis instance used by the
dashboard. Shell scripts, services and third-party tools use the same interface:

```sh
redis-cli PUBLISH scootui:notification \
  '{"source":"my-script","id":"job","title":"Download complete","severity":"success","ttl_ms":10000}'

# Same source + id updates the existing event and renews its expiry.
redis-cli PUBLISH scootui:notification \
  '{"source":"my-script","id":"job","title":"Download failed","body":"Please try again","severity":"warning","ttl_ms":15000}'

redis-cli PUBLISH scootui:notification \
  '{"source":"my-script","id":"job","action":"dismiss"}'
```

| Field | Contract |
|---|---|
| `id` | Required, 1–64 ASCII letters, digits, `.`, `_`, `-`; first character alphanumeric. |
| `source` | Same format as `id`; defaults to `external`. Namespaces sender IDs. |
| `action` | `show` (default) or `dismiss`. Dismiss accepts only action, source and id. |
| `title` | Required for show, nonblank, at most 120 UTF-16 code units. |
| `body` | Optional plain text, at most 512 UTF-16 code units. |
| `severity` | `debug`, `info` (default), `success`, `warning`, `error`, or `critical` (error alias). |
| `ttl_ms` | Integer 1000–60000; defaults to 10000. Starts at receipt, not first display. |

Messages are limited to 4096 UTF-8 bytes. Unknown fields, wrong types and invalid
values are rejected without changing notifications; rejection reasons appear in
the dashboard journal. Text is rendered literally, not as HTML.

These are transient events, not persistent fault conditions. Debug uses priority
5, info 4, success 3, warning 2, and error/critical 0. Existing critical and
numeric-priority producer APIs remain available; severity names normalize their
classification before arbitration. They share arbitration,
five-second equal-priority cycling, cue deduplication and the bounded event
registry with built-in notifications. Higher-priority items can keep an event
hidden until it expires; an update extends its freshness but does not restart
its current cycling dwell. Dismiss is idempotent. External IDs cannot dismiss or
replace built-in entries.

Redis pub/sub is fire-and-forget: it does not queue requests while the dashboard
is disconnected or off, wake the display, or acknowledge actual presentation.
The `PUBLISH` return value counts subscribers, not accepted/displayed messages.
Tools use their existing Redis client; no additional socket listener is needed.

Desktop simulator notification buttons call the same registry API through a
local-only test source. Enable the simulator explicitly with
`SCOOTUI_SIMULATOR=1`; notification entries stay in the local UI registry and
never write simulated faults or real source IDs to a connected datastore:

```sh
cmake -S . -B build -DDESKTOP_MODE=ON -DBUILD_TESTING=ON
cmake --build build
SCOOTUI_REDIS_HOST=none SCOOTUI_SIMULATOR=1 ./build/bin/scootui
# A live backend is also supported for notification preview:
SCOOTUI_REDIS_HOST=192.168.7.1 SCOOTUI_SIMULATOR=1 ./build/bin/scootui
```

For headless captures, use the existing screenshot hook with the production
registry (the map renderer still requires QMapLibre on a target-equivalent
build).

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
