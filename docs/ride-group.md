# Live ride-group maps

The feature is disabled unless `settings[ride-group.enabled]` is `true`. Cloud membership and consent, plus enabled MQTT or uplink transport support, are also required. The brake-navigated menu provides **Live sharing**, **Stop sharing** and explicit **Enable sharing** actions. A stopped or unconfirmed journal never silently enables sharing.

Qt is the sole writer of `/data/scootui/ride-group/stop.json`, protected by a stable `owner.lock`. File operations run outside the UI thread. Missing, unsafe, unmounted, unavailable or exhausted storage fails closed without gating ordinary dashboard startup. Journal writes use a private same-directory temporary file, file sync, checked close, atomic rename and directory sync. Interrupted writes require resynchronization. Recovered preparing records become durably blocked uncertain records before any response.

A genuine explicit stop creates a durable preparing record and an in-process intent permit. The first valid, correlated offer consumes that permit before preparation I/O and fixes the immutable fence. Committed receipts are persisted before confirmation; sharing remains blocked until a later explicit durable idle action.

Map visibility is reported only from a styled selected map. Activity expires after ten seconds and is refreshed every three seconds while the UI, display and eligible vehicle state remain active. Control, probes and stop responses do not constitute map activity.

Peers remain transient. Producer timestamps, recipient grants, source epochs, sequence watermarks and terminal states fence updates. Two sufficiently accurate fixes can produce a bounded estimate: intervals 1–5 seconds, speed at most 40 m/s, extrapolation at most three seconds and 50 metres. The measured point remains visible with a tether to its hollow estimated marker. Small corrections settle within 300 ms; large corrections snap. Stale points gray after ten seconds and disappear after thirty seconds or earlier authority withdrawal. Both QMapLibre QML module variants carry the overlay.

## Local validation

Configure against Qt 6.9.3 and the pinned patched QMapLibre install. Run the full suite under Xvfb/XCB with the install's `plugins`, `qml` and `lib` paths supplied through `QT_PLUGIN_PATH`, `QML_IMPORT_PATH` and `LD_LIBRARY_PATH`. The `map-render` test exercises the actual native styled map and all five peer markers at 480×480. Offscreen/software Quick tests alone are not map-render proof.

Journal tests cover ownership, symlinks, permission and directory replacement, monotonic counters, immutable fences, exhausted recovery and interruptions at each atomic-write boundary. Service tests cover default-off behavior, explicit enabling, competing offers, preparing recovery, committed receipts, source terminals, future timestamps and prediction confidence.
