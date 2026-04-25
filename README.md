# ScootUI Qt - LibreScoot's User Interface

ScootUI Qt is the dashboard application for the LibreScoot electric scooter firmware. It is a native Qt 6 application targeting the i.MX6-based dashboard computer (DBC).

[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa]

## Features

- **Real-time Telemetry Display**
  - Speed, power output, battery levels, odometer, trip meter
  - GPS status, connectivity indicators, fault display
  - Configurable power readout (kW or amps) and battery readout (% or range estimate)

- **Multiple View Modes**
  - Cluster view with speedometer and vehicle status
  - Map view with 3D-tilted navigation
  - Destination selection with address search, recent destinations, saved locations
  - Navigation setup, OTA update interface, fault list, hop-on flow

- **Navigation**
  - Vector map rendering via QMapLibre
  - Turn-by-turn directions with Valhalla routing
  - Speed limit indicators and road name display
  - Auto-rotating map with heading tracking
  - Off-route detection and automatic rerouting
  - Reverse geocoding for tap-to-navigate
  - Optional traffic overlay; offline + online tile sources

- **System Integration**
  - Connects to the scooter's main data bus (Redis-based MDB)
  - Handles battery, engine, GPS, Bluetooth, and other vehicle systems
  - Support for over-the-air (OTA) updates with multi-mode boot flow
  - Map pack management with optional auto-download

- **Adaptable Design**
  - Light and dark themes with auto-switching based on ambient light
  - Semantic color tokens via `ThemeStore` for fonts, radii, and status colors
  - Configurable blinker overlay styles
  - Multi-language support

## Technology Stack

- **Qt 6 / QML** - UI framework
- **QMapLibre** - Vector map rendering
- **Valhalla** - Routing engine
- **Redis** - Real-time data communication via MDB
- **CMake** - Build system

## Development

### Prerequisites

- Qt 6.4+ with Quick, Qml, Svg, Network, Sql, Concurrent modules
  (Qt 6.9.x recommended; matches the desktop dev script default)
- QMapLibre (optional in desktop mode, required for target)
- CMake 3.16+

### Desktop Build

The desktop build includes a simulator panel for development without physical hardware:

```bash
./run-desktop.sh
```

Or manually:

```bash
cmake -B build -DDESKTOP_MODE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
SCOOTUI_REDIS_HOST=none ./build/bin/scootui
```

Setting `SCOOTUI_REDIS_HOST=none` runs without a Redis connection, using the built-in simulator instead.

### Target Build (Cross-compilation)

Cross-compile for the i.MX6 (armhf) target using Docker:

```bash
./cross-build.sh [Release|Debug]
```

This produces a self-contained `deploy-armhf/` directory with the binary, shared libraries, Qt plugins, and a launcher script. Deploy with:

```bash
scp -r deploy-armhf/* target:/opt/scootui/
ssh target /opt/scootui/run-scootui.sh
```

### Using Make

```bash
make build    # Configure and build
make run      # Build and run
make clean    # Remove build directory
make rebuild  # Clean and rebuild
```

## Project Structure

```
src/
  core/           App configuration, environment setup
  models/         Data models and enums
  repositories/   Data access (Redis MDB, in-memory)
  stores/         QML-exposed state stores (vehicle, engine, battery, GPS, ...)
  services/       System services (navigation, settings, input, map, ...)
  routing/        Valhalla client and route models
  simulator/      Simulator service for desktop development
  l10n/           Translations
  utils/          Utility functions

qml/
  screens/        Main UI screens (cluster, map, destination, OTA, ...)
  widgets/        Reusable UI components, grouped by subsystem
    blinker/       Blinker overlays
    cluster/       Cluster screen widgets
    components/    Shared building blocks (icons, hints, frosted glass, ...)
    indicators/    Status indicators, speed limit, road name
    map/           Map view, vehicle marker, scale bar, north indicator
    navigation/    TBT, route progress, status pill, roundabout icon
    power/         Power display
    shutdown/      Shutdown overlay
    speedometer/   Speedometer arc
    status_bars/   Battery, top/bottom bars
  overlays/       Modal overlays (menu, toast, bluetooth PIN, ...)
  simulator/      Simulator control panel (desktop mode only)
  theme/          Theme definitions

assets/
  icons/          SVG icons
  fonts/          Roboto font family
  routes/         Test route data for simulator
  styles/         MapLibre style definitions
```

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SCOOTUI_REDIS_HOST` | `192.168.7.1` | Redis host (use `none` to disable). Supports `host:port` format |
| `SCOOTUI_RESOLUTION` | `480x480` | Display resolution (`WIDTHxHEIGHT`). UI scales automatically |
| `SCOOTUI_SETTINGS_PATH` | _(none)_ | Path to persistent settings file |

### Runtime Settings (Redis)

Settings are stored in the `settings` Redis hash and can be modified at runtime. The source of truth is the settings schema; each key handled here carries a `// @schema <key>` annotation in `src/stores/SettingsStore.h`, and `scripts/check-schema-coverage.sh` enforces coverage in CI.

#### Dashboard Display

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `dashboard.mode` | `speedometer`, `cluster`, `map` | `speedometer` | Active screen on boot |
| `dashboard.show-raw-speed` | `true`, `false` | `false` | Show uncorrected ECU speed |
| `dashboard.battery-display-mode` | `percentage`, `range` | `percentage` | Battery readout style |
| `dashboard.power-display-mode` | `kw`, `amps` | `kw` | Power readout style |
| `dashboard.show-gps` | `always`, `active-or-error`, `error`, `never` | `error` | GPS icon visibility |
| `dashboard.show-bluetooth` | `always`, `active-or-error`, `error`, `never` | `active-or-error` | Bluetooth icon visibility |
| `dashboard.show-cloud` | `always`, `active-or-error`, `error`, `never` | `error` | Cloud connection icon visibility |
| `dashboard.show-internet` | `always`, `active-or-error`, `error`, `never` | `always` | Cellular icon visibility |
| `dashboard.show-clock` | `always`, `never` | `always` | Clock visibility |
| `dashboard.theme` | `light`, `dark`, `auto` | `auto` | UI theme |
| `dashboard.blinker-style` | `default`, `overlay` | `default` | Blinker indicator style |
| `dashboard.language` | `en`, `de`, ... | `en` | UI language |
| `dashboard.hop-on-combo` | pipe-delimited token list (e.g. `LB|RB|HORN`) | _(unset)_ | Custom hop-on unlock combo; managed by the hop-on UI |

#### Map & Navigation

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `dashboard.map.type` | `online`, `offline` | `offline` | Map tile source |
| `dashboard.map.render-mode` | `vector`, `raster` | `vector` | Map renderer |
| `dashboard.map.traffic-overlay` | `true`, `false` | `false` | Show live traffic overlay |
| `dashboard.maps.check-for-updates` | `true`, `false` | `true` | Periodically check for offline map updates |
| `dashboard.maps.auto-download` | `true`, `false` | `false` | Download map updates without user action |
| `dashboard.valhalla-url` | URL | `http://localhost:8002/` | Valhalla routing endpoint |

**Examples:**
```bash
# Always show GPS indicator
redis-cli hset settings dashboard.show-gps always

# Switch to online maps
redis-cli hset settings dashboard.map.type online

# Use full-screen blinker overlay
redis-cli hset settings dashboard.blinker-style overlay
```

## Theming

`ThemeStore` exposes shared design tokens to QML — font scale, border radii, theme-aware text/surface/border colors, and theme-independent status colors (`statusSuccess`, `statusWarning`, `statusError`, `statusNeutral`, `statusInfo`). Use these instead of hardcoding hex literals.

## Screens

- **Cluster Screen** — speedometer, battery status, indicator lights
- **Map Screen** — 3D navigation view with turn-by-turn directions
- **Destination Screen** — saved locations, recent destinations, address search entry
- **Address Selection** — typeahead search with reverse geocoding
- **Navigation Setup** — route preview, distance, ETA, confirmation
- **Faults Screen** — active fault list with severity and component info
- **Hop On Info** — handover/anti-theft pairing flow
- **Update Mode Info** — explainer for OTA boot modes
- **OTA Background** — system update interface
- **About Screen** — version and system information
- **Maintenance Screen** — diagnostic information

## Contributing

Contributions to ScootUI Qt are welcome. When contributing, please follow the existing code style and patterns.

## License

This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: http://creativecommons.org/licenses/by-nc-sa/4.0/
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg
