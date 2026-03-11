# ScootUI Qt - LibreScoot's User Interface

ScootUI Qt is the dashboard application for the LibreScoot electric scooter firmware. It is a native Qt 6 application targeting the i.MX6-based dashboard computer (DBC).

[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa]

## Features

- **Real-time Telemetry Display**
  - Speed, power output, battery levels, odometer, trip meter
  - GPS status, connectivity indicators, and system warnings

- **Multiple View Modes**
  - Cluster view with speedometer and vehicle status
  - Map view with 3D-tilted navigation
  - Destination selection and navigation setup screens
  - OTA update interface

- **Navigation**
  - Vector map rendering via QMapLibre
  - Turn-by-turn directions with Valhalla routing
  - Speed limit indicators and road name display
  - Auto-rotating map with heading tracking

- **System Integration**
  - Connects to the scooter's main data bus (Redis-based MDB)
  - Handles battery, engine, GPS, Bluetooth, and other vehicle systems
  - Support for over-the-air (OTA) updates

- **Adaptable Design**
  - Light and dark themes with auto-switching based on ambient light
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
  widgets/        Reusable UI components (speedometer, status bars, blinker, ...)
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

Settings are stored in the `settings` Redis hash and can be modified at runtime.

#### Dashboard Display

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `dashboard.show-raw-speed` | `true`, `false` | `false` | Show uncorrected ECU speed |
| `dashboard.show-gps` | `always`, `active-or-error`, `error`, `never` | `error` | GPS icon visibility |
| `dashboard.show-bluetooth` | `always`, `active-or-error`, `error`, `never` | `active-or-error` | Bluetooth icon visibility |
| `dashboard.show-cloud` | `always`, `active-or-error`, `error`, `never` | `error` | Cloud connection icon visibility |
| `dashboard.show-internet` | `always`, `active-or-error`, `error`, `never` | `always` | Cellular icon visibility |
| `dashboard.theme` | `light`, `dark`, `auto` | `auto` | UI theme |
| `dashboard.blinker-style` | `default`, `overlay` | `default` | Blinker indicator style |
| `dashboard.language` | `en`, `de`, ... | `en` | UI language |

#### Map & Navigation

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `dashboard.map.type` | `online`, `offline` | `offline` | Map tile source |
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

## Screens

- **Cluster Screen** - Main dashboard with speedometer, battery status, and indicator lights
- **Map Screen** - 3D navigation view with turn-by-turn directions
- **Destination Screen** - Saved locations and destination selection
- **Navigation Setup** - Route preview and confirmation
- **About Screen** - Version and system information
- **Maintenance Screen** - Diagnostic information
- **OTA Screen** - System update interface

## Contributing

Contributions to ScootUI Qt are welcome. When contributing, please follow the existing code style and patterns.

## License

This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: http://creativecommons.org/licenses/by-nc-sa/4.0/
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg
