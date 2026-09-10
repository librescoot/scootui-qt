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
When navigation is primary with a secondary notification, its normal 96px banner
and 64px maneuver glyph remain; next-preview and trip-summary rows are hidden.
Higher-priority alerts use compact navigation beneath them. Notification cards
use 80%-opaque severity-tinted backgrounds (red, amber, green, blue, neutral),
without a left stripe. Queued counts wrap within 112px, preserving exact numbers
and severity/accessibility labels.

Notification text uses tight padding, natural height within a 156px overlay
budget, and 20px semibold titles that adapt to 18px when needed; bodies remain
18px. The English and German native low-12V warning fits fully without scrolling,
including navigation and queued counts. Unusually long titles and bodies retain
their complete plain-text strings and wrap even unbroken words, with no ellipsis.
If they still exceed the safe space above the speed glyphs, the text automatically
scrolls vertically at 20px/second after a 1.2-second pause, pauses 1.6 seconds at
the end, and repeats. A small position indicator marks overflow. Scrolling resets
on entry/content changes and stops when hidden; it does not change slot cycling,
cue deduplication, or expiry. The primary maneuver data does not scroll.

Events still expire while queued or scrolling: a sufficiently large or
higher-priority queue can outlast an event's TTL, and an unusually long message
with a short TTL (including the external 1-second minimum) can expire before its
full scroll completes. Displaying or scrolling never renews the deadline.
External events default to 10 seconds, native events to 4 seconds, adapted toasts
to 3 seconds (5 for errors), and arrival feedback to 10 seconds.

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
