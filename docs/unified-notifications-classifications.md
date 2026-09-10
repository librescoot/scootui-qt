# Unified notification source classifications

This table is the source contract for the production attention policy. Priority is
urgency, not the producer's color or whether it historically used a permanent toast.
The order is error/critical (P0), warning (P2), navigation, success (P3), info (P4),
then debug (P5). Navigation ranks between warning and success regardless of distance.
The attention policy does not suppress routine feedback while riding; individual
producers retain their own condition eligibility.

| Source | Stable ID / ownership | Class | Lifecycle and eligibility |
|---|---|---|---|
| Navigation maneuver | `navigation-session` / NavigationService | Navigation, independent of distance | Live payload replaces the prior maneuver; invalid route data is removed. |
| Navigation calculation/reroute/arrival | navigation status | Navigation; arrival also emits one P3 success event | Replaces the navigation presentation; arrival feedback is emitted once per session, not duplicated as a toast. |
| Navigation off-route | `navigation-offroute` / NavigationService | P2 | Active until route tracking resolves or navigation ends. |
| Loss of primary vehicle connection | `redis-disconnect` / Application connection producer | P0 | Persistent while the previously established link is unavailable; valid backup data is not treated as P0. |
| USB backup link | `usb-disconnect` / Application connection producer | P2 | Active while fallback is in use, subject to the configured suppression. |
| Battery and ECU faults | `battery-fault-{slot}`, `ecu-fault` / Application | P0 only for formatter-classified critical faults, otherwise P2 | Active fault sets update in place and resolve when their source set clears. |
| Map coverage | `map-coverage` / MapService | P2 | Map-surface only and active while the current location has no installed map coverage. |
| Map update availability | `map-update` condition / MapDownloadService | P4 info condition | Persistent passive condition in the shared attention dock; no independent corner renderer or repeated toast on startup/reconnect. |
| Bluetooth/temperature/backup battery/lock monitors | existing ToastService IDs | P0 errors, P2 warnings, P3 success, P4 info | Toast compatibility adapter supplies stable condition IDs for permanent entries and bounded transient events. BackupBatteryMonitor dismisses its warnings while riding and reevaluates when parked. |
| Saved locations, update checks, log capture, pairing feedback | existing ToastService events | P0 errors, P2 warnings, P3 success, P4 info | Transient, eligible while riding; the severity selects priority, not the producer name. |
| Auto-lock, pairing, UMS, shutdown, maintenance | existing authoritative overlays | not notifications | Their existing workflows retain ownership and input semantics. |
| Speed, blinkers, telltales | existing instrumentation | not notifications | Protected; never queued or preempted by attention policy. |

Simulator warning/error/critical controls use `NotificationService` directly
and are available when the desktop simulator is explicitly enabled with
`SCOOTUI_SIMULATOR=1`, including with a live backend. They only create local
`simulator` source entries; clearing them addresses those IDs only and cannot
resolve vehicle-owned conditions or write simulator data to Redis.
