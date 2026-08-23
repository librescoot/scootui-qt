# scootui-qt main menu tree

Root of the menu hierarchy as built by `MenuStore::rebuildMenuTree()`, in build
order. Conditions in square brackets: if false, the item is hidden. Cycle
options are listed in cycle order. `→ Screen` means the entry closes the menu
and hands the display to a full-screen page.

## Root (MENU)

- **Disable Service Mode** [`settings[service-active] == true`]
- **Activate Hop-on** [a hop-on combo is learned]
- **Navigation ▸** [(local display maps OR map type Online) AND routing ready]
- **Set up Navigation** [routing not ready] → NavigationSetup (Routing)
- **Set up Map Mode** [on cluster, no local maps, map type not Online] → NavigationSetup (DisplayMaps)
- **Switch to Cluster View** [on map]
- **Switch to Map View** [on cluster, local maps or Online map type]
- **Lock Scooter** [vehicle state is exactly Parked]
- **Toggle Hazard Lights**
- **Faults (N)** [N active faults > 0] → Faults
- **Settings ▸**
- **Info ▸**
- **Exit Menu**

## Navigation ▸

- **Enter Address…** → AddressSelection
- **Recent Destinations ▸** [at least one recent destination]
  - `<label>` ▸ (per destination, newest first)
    - Start Navigation
    - Save to Favorites
    - Delete Location
- **Saved Locations ▸**
  - Save Current Location
  - `<label>` ▸ (per saved location)
    - Start Navigation
    - Delete Location
- **Stop Navigation**
- **Maps & Routing…** → NavigationSetup (Both)

## Settings ▸

### Appearance ▸

- **Theme** `[Auto / Dark / Light]`
- **Backlight** `[Auto / Low / Medium / High]`
- **Power Display** `[kW / Amps]`
- **Blinker ▸**
  - Style `[Icon / Fullscreen Arrow]`
  - DBC Blinker LED `[toggle]`
- **Status Bar ▸**
  - Battery Display `[Percentage / Range / Icons only]`
  - CB Battery `[Always / When Low / Never]`
  - AUX Battery `[Always / When Low / Never]`
  - GPS Icon `[Always / Active or Error / Error only / Never]`
  - Bluetooth Icon `[same four]`
  - Cloud Icon `[same four]`
  - Internet Icon `[same four]`
  - Clock `[Time / Date + Time / Alternating / Never]`
  - Temperature `[Always / Warning only / Never]`
- **Milestone Celebrations** `[toggle]`

### Vehicle ▸

- **Hop On / Hop Off…**
  - No combo yet: action → HopOnInfo (Back / Start; Start enters the learning overlay)
  - Combo learned: submenu
    - Set new combo…
    - Disable hop-on
- **Alarm ▸**
  - Alarm `[toggle]`
  - Honk `[toggle]`
  - Duration `[10s / 20s / 30s]`
- **Battery Mode** `[Single / Dual]`
- **Horn While Seatbox Open** `[toggle]`

### Map & Navigation ▸

- **Map View** `[3D / 2D]`
- **Orientation** `[Heading up / North]` [2D only]
- **Route Preference** `[Fastest / Shortest]`
- **Avoid Cobblestone** `[Off / Low / Medium / High]` [not Shortest, and not the public Valhalla endpoint]
- **Map Updates** `[Off / Notify / Download]`
- **Check for Updates Now** [maps installed], trailing "2h ago"
- **Map Type** `[Online / Offline]`
- **Navigation Routing** `[Online / Offline]`

### System ▸

- **Language** `[English / Deutsch]`
- **Update Mode…** → UpdateModeInfo (Back / Start; Start enters UMS)
- **Service Mode** [service mode not already active]
- **Updates ▸** [UpdateChannelService present]
  - Check Frequency `[Off / 6h / 12h / 24h / 3d / 7d]`
  - Check for Updates Now, trailing "2h ago"
  - Update Type ▸ `[Delta / Full]` (caution; checkable rows, not a cycle)
  - Release Channel ▸ `[Stable / Testing / Nightly]` (caution) → UpdateChannel on a real switch
- **Capture Logs**

## Info ▸

- **Components** → SystemInfo (Device)
- **Connectivity** → SystemInfo (Connectivity)
- **Batteries** → SystemInfo (Batteries)
- **Faults (N)** → Faults
- **About & Licenses** → About

## Input

- Left brake tap: scroll down. At root on cluster/map, a double-tap opens the menu.
- Left brake long-tap (800 ms): go back one level, or close the menu at root.
- Left brake hold (3 s): close the menu from any depth. Deliberately unhinted,
  the one bound gesture the bar does not name: an escape from four levels down
  is worth having and is not worth 19 px of every screen to advertise. It
  arrives after the 800 ms long-tap has already popped one level, so from the
  root the menu has closed before it can fire.
- Right brake tap: select, enter a submenu, or advance a cycle setting.

The hold is the only way back: the synthetic `__back` row submenus used to
carry at index 0 is gone, now that the bar names the gesture that does the same
thing.

### The hint bar

`ControlHints` names up to three durations per lever, each in a capsule in
front of the action rather than under a "Left Brake" caption: `TAP`, `HOLD`,
`HOLD 3S`. German is `TAP`, `HALTEN`, `3S HALTEN`: the duration leads the verb
there and trails it in English, so the three are separate strings rather than
one format with a number in it. Which lever is carried by the edge the hint
sits against, which is what the caption used to say on every screen.

The capsule runs straight into its verb rather than sitting in a column sized
to the widest word. Holding the verbs on one margin would leave `TAP` 20 px
from the word it belongs to in German, where `HALTEN` sits right against its
own.

The three durations are row slots shared by both sides, so taps line up across
the bar and holds line up under them. A slot neither side binds collapses and
the bar shrinks, which is why a page that does not scroll shows no scroll hint.
Screens whose labels change with the scroll position set `reservedRows` to pin
the height: a bar that grew and shrank mid-scroll would shift the content above
it, and where a flickable is sized `parent.height - bar.height` a content-derived
bar height is a binding loop.

`leftHoldLong` / `rightHoldLong` are the 3 s hold and are reserved for close and
cancel, so the long hold means one thing wherever it is bound. Today only the
UMS overlay binds it. Nothing binds a right-side hold; `InputHandler` emits
`rightHold()` and `rightBrakeHold()` so one can be wired without touching the
input layer again.

## Navigation stacks

`MenuStore` keeps `m_pathStack` (node ids) and `m_indexStack` (the selected row
at each level). `selectItem()` pushes both when entering a submenu, `goBack()`
pops both. `rebuildMenuTree()` replays the saved path against the tree it just
built and stops at the first level that no longer resolves, so an entry that
disappears under you (a deleted saved location, a submenu whose condition went
false) drops you to the nearest surviving ancestor instead of nowhere.

Entries that hand the display to a full-screen page call `closeForScreen()`
rather than `close()`: it remembers the path and index, and the page's Back
calls `menuStore.resume()`, which reopens the menu on the level the rider left.
Any other `close()` clears that memory, so a page reached some other way (the
simulator panel, a toast action) still backs out to the dashboard.

Armed entries and where Back returns to:

| Entry | Page | Back returns to |
|---|---|---|
| Set up Navigation | NavigationSetup | MENU |
| Set up Map Mode | NavigationSetup | MENU |
| Faults (N) | Faults | MENU |
| Navigation ▸ Maps & Routing… | NavigationSetup | NAVIGATION |
| Settings ▸ Vehicle ▸ Hop On / Hop Off… | HopOnInfo | VEHICLE |
| Settings ▸ System ▸ Update Mode… | UpdateModeInfo | SYSTEM |
| Settings ▸ System ▸ Updates ▸ Release Channel ▸ `<channel>` | UpdateChannel | RELEASE CHANNEL |
| Info ▸ Components / Connectivity / Batteries | SystemInfo | INFO |
| Info ▸ Faults | Faults | INFO |
| Info ▸ About & Licenses | About | INFO |

Confirm paths deliberately do not resume: Start on UpdateModeInfo, Start on
HopOnInfo, and Confirm on UpdateChannel all leave the menu closed, because each
one hands over to an overlay or a background job the rider wants to watch.
Main.qml's ReadyToDrive auto-close does not resume either; `openAt()` would drop
it anyway, since the menu only opens while parked.

Fire-and-forget entries keep plain `close()`: hazards, lock, Capture Logs, both
Check for Updates Now entries, Service Mode on and off, Start Navigation, Stop
Navigation. They finish on the dashboard, which is where the resulting toast is
visible.

### Known gaps

- **AddressSelection does not participate.** `Enter Address…` uses plain
  `close()`, and the screen's own back-out hardcodes `screenStore.setScreen(1)`
  (Map) instead of a `closeAddressSelection()` that restores the screen it was
  opened from. A rider who starts on the cluster and backs all the way out of
  address entry lands on the map. Confirming a destination going to the map is
  intentional; cancelling should not.
- **`canScrollUp()` and `canScrollDown()` are the same predicate**
  (`totalCount > 1`). MenuOverlay reads `canScrollDown` to drop the Scroll hint
  on a level with a single entry; `canScrollUp` still has no consumer, and the
  overlay derives its gradients from `contentY` rather than either.
