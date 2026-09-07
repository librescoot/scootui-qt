# Unified notifications and banners — discussion draft

Status: proposal, not approved for implementation. Branch: `plan/unified-notifications`.

## Goals

- One predictable attention policy across cluster and map; no independent toast stacks.
- Preserve speed, turn indicators, critical telltales, battery essentials, and useful map context.
- Work without touch and without asking riders to operate UI while riding.
- Borrow Android's ongoing notifications, heads-up promotion, grouping, and notification history; not its shade, swipe gestures, or action-heavy cards.
- Unify scheduling and presentation contracts, not every source into a generic text toast.

## Current implementation

| Surface | Observed behavior / integration point |
|---|---|
| `src/services/ToastService.cpp` | Append-only visible list, text-based deduplication for anonymous events, explicit-ID updates, permanent entries, 3 s expiry / 5 s for errors starting at creation. No arbitration or display budget. |
| `qml/overlays/ToastOverlay.qml` | Full-width column starting at y=48, every toast rendered, unconstrained aggregate height. |
| `qml/widgets/navigation/TurnByTurnWidget.qml` | Independent ongoing banner, minimum 96 px, instruction up to three lines plus next-turn preview and floating trip metrics. |
| `qml/screens/ClusterScreen.qml` | TBT docks below the 40 px status bar; speedometer fills the underlying central region. Reserving height for other cluster elements does not itself prove speedometer clearance. |
| `qml/screens/MapScreen.qml` | TBT, navigation status, road information and coverage warning have separate placement. |
| `src/services/MapService.h` | `tbtVisible` reserves a fixed 96 px footprint for north-up centering, not actual rendered height. |
| `qml/widgets/navigation/NavigationStatusOverlay.qml` | Calculating/rerouting/arrival pill; off-route child is inside a parent whose visibility excludes Navigating. Verify and cover this visibility conflict during migration. |
| `qml/Main.qml` | Independent overlay z-values; connection failures become permanent error toasts. |
| `src/services/SoundCueService.cpp` | Toast sounds driven by `toastAdded`, not presentation eligibility. Vehicle sound events also exist and must remain separate. |
| `qml/overlays/*` | Auto-lock countdown uses a centered card and scrim while parked; pairing PIN uses the bottom 30%; milestones have their own corner/center surfaces. |
| `src/services/InputHandler.h`, `qml/overlays/MenuOverlay.qml` | Brake gestures: double-left opens menu on main screens; menu left tap advances, right tap selects, left long-tap goes back, right long-tap activates primary action, left 3 s hold closes. Signals are broadcast with consumer-side guards. |

The problem is independent ownership of screen space and interruption, not just inconsistent styling. Existing warning/error colors cannot reliably determine safety priority: a failed map update and loss of trustworthy vehicle telemetry should not interrupt equally.

## Proposed presentation

### One attention dock, optional compact navigation companion

Below the existing top status bar, provide a shared dock on both main screens:

- Idle: zero notification height.
- Routine feedback: one compact row, approximately 40–48 px.
- Normal navigation: ongoing compact maneuver row, approximately 48 px.
- Approaching/performing a maneuver: expanded navigation card, target 80–96 px.
- Warning interrupts navigation: warning owns the main card; retain arrow + distance in a compact navigation companion when still valid.
- Hard budget: target 128 px total for card plus companion. These are prototype budgets, not approved typography measurements.
- No toast columns, centered transient messages, screen scrims, marquee text, or automatic carousel.
- Never reduce safety-critical text below legibility to satisfy the budget. Shorten secondary content, then remove it; details remain available while parked.
- ETA, trip distance and secondary street detail yield first. Maneuver direction/distance, especially roundabout exit and close consecutive turns, must survive compacting.

Example, not pixel-final:

```text
┌────────────────────────────────┐
│ existing status / telltales     │
├────────────────────────────────┤
│ ! Vehicle data unavailable     │  main attention card
│ ↱ 120 m                       │  navigation only if valid
├────────────────────────────────┤
│                                │
│    SPEED / useful map region   │  protected content
│                                │
├────────────────────────────────┤
│ existing essential bottom info │
└────────────────────────────────┘
```

A normal turn uses the main card, not a card plus a duplicate companion. If warning text and a navigation companion cannot fit legibly, critical safety wins; do not squeeze both into unreadable fragments. Do not display confident maneuver distances if position/navigation data is stale.

The cluster needs explicit protected rectangles, not merely higher z-values. The map needs shared occlusion insets used by marker placement, route framing, road labels and controls. Apply this to all map orientations, with stable size classes/hysteresis to avoid camera pumping on text changes. Other screens register their own available region rather than blindly inheriting a cluster overlay.

## Priority policy

Priority means interruption urgency, separate from severity/color and persistence.

| Priority | Meaning | Examples / proposed behavior |
|---|---|---|
| P0 — immediate safety / trust | Rider needs to know now | Validated stop-safely fault; loss of reliable driving telemetry. Immediate heads-up, persistent while condition remains, no automatic demotion to an icon. Never cover speed/critical telltales; stale speed must be visibly invalid rather than frozen as credible data. |
| P1 — maneuver now | Time-sensitive navigation | Approaching/performing turn, roundabout exit, closely spaced next turn. Suppresses routine interruptions. |
| P2 — actionable soon | Important, not immediate danger | Battery reserve warning, degraded but functioning connectivity, relevant vehicle fault. One heads-up, then compact active indicator; returns on meaningful escalation, not each sensor update. |
| P3 — ongoing context | Background task or navigation | Distant next maneuver, recalculating, route calculation, map download progress. Stable identity and in-place updates, generally silent. |
| P4 — feedback | Nonurgent event | Setting saved, update available, routine success, milestone. Brief heads-up when eligible; defer or discard while riding as appropriate. |

Examples require source-by-source review; no blanket conversion from `showError` to P0 or `permanent` to high priority. In particular, USB fallback with valid data is not equivalent to total telemetry loss. Connection validity and vehicle-fault classifications need explicit contracts.

Policy details:

1. Eligibility first (vehicle state, screen, freshness, session), then priority, then stable source-specific rank, then arrival order. Equal-rank arrivals do not churn the current card.
2. Higher priority preempts immediately. Keep displaced navigation current, never replay an old maneuver.
3. P0 stays expanded until resolution; multiple active P0 conditions need a deterministic primary plus visible additional-critical count and parked details. No rotating critical messages. Validate whether any fault combinations require a combined summary.
4. P2 heads-up normally beats distant navigation, but waits through P1; an actually urgent fault must be classified P0 instead of hidden behind an urgency timeout.
5. Navigation urgency is owned by NavigationService and exposed explicitly. Reuse its maneuver identity and hysteresis; do not invent a second distance-only scheduler in QML. Its current verbal stages are not automatically a complete urgency model. Validate speed, maneuver complexity and next-turn proximity before selecting thresholds.
6. P0 may take navigation's main card; retain a compact valid maneuver whenever the budget permits. P2+ cannot steal the main card during P1.
7. After a P2 heads-up, active state remains in a grouped indicator/parked list; acknowledgment never clears the underlying fault.
8. No periodic repeat for unchanged conditions. Escalation or clear-then-recurrence can notify again, with source-specific debounce/cooldown.

## Lifecycle and queueing

Keep state, event history and current presentation separate.

- Ongoing condition: stable namespaced ID, update in place, explicit source resolution. Never silently expires because it lost arbitration.
- Transient event: display-time budget plus absolute freshness deadline. Candidate defaults: 4 s normal / 6 s warning, to validate on hardware. A hidden event does not consume display time, but can become too stale to show.
- Interrupted transient: resume only if fresh and still useful, with a minimum useful dwell; do not replay sounds. Drop obsolete success messages rather than delivering a burst after a maneuver.
- Navigation: one ongoing session entry; current state replaces previous payload. Arrival/reroute/status replaces the appropriate navigation presentation, not an additional pill. Invalidated route instructions cannot survive rerouting as apparently valid guidance.
- Bounded pending transient queue (prototype cap: 20) and session history (prototype cap: 50); coalesce by semantic key, evict stale/lowest-value transient events first. Active conditions are a separate bounded-by-source registry and must not be evicted with history.
- History is RAM-only initially. Group repeated events with count and latest time. No storage of pairing PINs or other secrets; navigation entries need not log street-by-street history.
- Use monotonic timing and revision/generation checks so an old expiry cannot remove a renewed condition. Resolution invalidates pending presentation immediately.
- Reconnect reconstructs active source state without replaying a startup flood. Suppress routine restored-state sounds; an active P0 still becomes visible.

## No-touch interaction

Recommended default: **no global notification-dismiss gesture**.

- All banners are passive during ReadyToDrive, including at a traffic light. Zero speed alone does not mean parked; unknown/stale state takes the conservative path.
- Do not consume brake taps, holds, horn or turn-indicator actions to acknowledge banners. Preserve vehicle controls and existing menu entry behavior.
- Parked: add `Notifications` to the existing menu, using existing navigation/select/back gestures. Active conditions first, then recent events. Read/select reveals details; explicit acknowledgment only for sources that support it.
- Acknowledgment means 'seen', not 'resolved'. P0 remains visible. Clearing recent history does not clear active conditions.
- Entering ReadyToDrive closes notification details and returns to the driving screen. Pairing/action workflows cannot retain input focus while riding.
- Centralize input-context ownership for interactive surfaces: one context receives a gesture. Audit tap/double-tap/hold overlap so entering a menu cannot also select an item from the same gesture sequence.
- Never automatically focus a newly arrived banner, change a menu selection, or treat a brake action as confirmation of a newly appearing action.

This is Android-style progressive disclosure adapted to a cluster, not a notification shade to operate while moving.

## Other surfaces: integrate policy, not everything into toasts

| Existing family | Proposed destination |
|---|---|
| Toasts, connection/battery/temperature/lock/Bluetooth monitors, fault announcements | Typed events/conditions in attention service; explicit classification per producer. |
| TBT, calculating/rerouting/arrival/off-route, coverage warning | Navigation presentation adapter; one coherent navigation state, with freshness rules. Coverage loss alone must not imply routing is unusable if navigation remains valid. |
| Map/update progress, setting feedback, log capture | Ongoing task or transient feedback; parked-first for maintenance events. |
| Auto-lock countdown | Compact parked ongoing card, authoritative timer remains in vehicle state. Decide whether the final seconds merit stronger emphasis; no UI change to lock/cancel semantics. |
| Pairing PIN, learning/configuration flows | Parked-only dedicated task surfaces under a shared surface arbiter; secure data stays outside notification history. Backend behavior must be checked before hiding/canceling an active flow. |
| Milestones/confetti | Low-priority event; suppress celebration during riding and critical alerts by default, optionally celebrate when parked. Product decision required. |
| Shutdown, hibernation, UMS, maintenance/update screens | Explicit system modes, not queued notifications. Retain authoritative mode semantics and audit which alerts can meaningfully appear in each. |
| Blinkers, speed, critical telltales | Protected instrumentation, not notifications. Never put these in a queue. |
| Debug/version/service overlays | Developer or intentional screens, not routine heads-up events; document precedence. |

System-mode gating precedes normal arbitration. Security/task overlays must not automatically outrank safety just because their old z-value was larger. Define a mode/surface eligibility matrix before migration; do not presume a shutdown or maintenance screen can always preserve a functioning driving cluster.

## Suggested architecture

- `NotificationService`: typed active registry, event history, stable IDs, source ownership, update/resolve/acknowledge APIs.
- `AttentionPolicy`: deterministic/testable reducer for eligibility, priority, display clocks, preemption, coalescing, and sound decisions. Inject monotonic clock.
- `NavigationAttentionAdapter`: derives a live typed maneuver/task payload and urgency from NavigationService. No copied route engine logic.
- `NotificationPresenter`/QML-facing store: selected main entry, optional navigation companion, active summary, layout class and occupied insets.
- Shared `AttentionDock.qml` with specialized maneuver, warning, task and feedback bodies. No domain decisions or timer ownership inside delegates.
- `SurfaceArbiter` / input-context owner: resolves intentional screens/system modes and interactive focus; keeps existing workflow controllers authoritative.
- `NotificationsScreen.qml`: parked-only details/history through existing menu controls.
- Keep `ToastService` temporarily as a compatibility adapter, migrate producers one at a time, then remove it. Legacy error calls must not silently become safety alerts; explicitly migrate all known safety/trust sources before rollout.

Candidate entry fields: `id`, `source`, `kind`, `severity`, `priority`, `payload`, `revision`, `createdAt`, `updatedAt`, `validUntil`, `displayBudget`, `stateEligibility`, `ackPolicy`, `soundPolicy`, `groupKey`. Typed payloads retain maneuver/roundabout data rather than preformatting everything into strings. Presentation strings should remain language-responsive.

Sounds: notification audio comes from a policy-approved presentation/escalation, not insertion. At most one cue for an interruption; no sound on distance/progress updates or resume. Deferred information stays silent during a maneuver. Preserve existing vehicle cues and audit navigation audio coexistence rather than treating all sounds as notifications. Respect audio settings; do not assume audio is available or audible as a substitute for visuals.

## Implementation sequence after decisions

1. Approve priority taxonomy, protected content, riding/parked boundary, and modal exceptions. Inventory every producer into a checked-in classification table.
2. Prototype 480×480 cluster/map layouts with fixed example payloads: distant turn, complex roundabout, long localized text, warning plus turn, two critical faults. Compare compact-navigation default against always-expanded TBT before changing behavior.
3. Implement/test typed registry and pure policy with fake clock; no user-visible migration yet.
4. Add shared dock, protected cluster geometry and map insets. Adapt live navigation; preserve existing maneuver semantics and test all orientations.
5. Migrate toast producers and audio; remove duplicate rendering only once each source is covered. Add parked notifications details.
6. Integrate countdowns/task surfaces and explicit input ownership; separately approve changes to pairing, learning and system modes.
7. Simulator and bench validation, then vehicle trial with owner approval. No automatic deployment or vehicle unlocking as part of this planning work.

## Acceptance / validation

- At most one main card plus one compact navigation companion; no unbounded stacking.
- Speed, indicators and essential bottom information remain readable at 480×480 in both themes and long translations. Capture screenshots for every layout class; verify real display readability/glare.
- P2 warning during P1 waits; P0 preempts immediately; unchanged equal-priority faults do not oscillate. Multiple critical faults remain discoverable.
- Distant guidance survives warning as compact companion; maneuver at 0 m remains visible; roundabout exits and close turns retain needed information.
- Calculating/reroute/arrival/off-route do not overlap or leave stale guidance; total data loss clearly invalidates telemetry.
- Hidden transient expires by freshness, not prematurely by insertion timer; no stale queue burst on return from menu/mode or long turn.
- Resolve/update/expiry races, reconnect floods, repeated IDs, escalation, translation change, and bounded history have deterministic tests.
- No double sound for updated/resumed entries; vehicle sounds unaffected.
- Brake input while a banner arrives neither dismisses nor confirms anything. Details close on ReadyToDrive; zero-speed ReadyToDrive is still restricted.
- Map camera/marker remains in usable space across north-up, heading-up and perspective modes without resize jitter. No full-screen blur; profile QML/layout churn on DBC.

## Decisions requested

1. Navigation: compact by default, expanding near maneuvers (recommended), or keep today's full-size TBT whenever navigating?
2. Interaction: passive while riding, with history/details only while parked (recommended), or is acknowledgment while riding a requirement?
3. Safety hierarchy: approve safety/trust > imminent maneuver > actionable warning > ongoing context > routine feedback? Which current vehicle conditions genuinely warrant persistent P0?
4. Permanent warnings: brief heads-up then persistent compact indicator, except P0 (recommended), or must some noncritical messages remain fully expanded?
5. Scope: include countdown and pairing presentation in this design, but migrate their behavior only in a separately reviewed phase (recommended)?
6. Milestones: defer all celebratory animation until parked (recommended), or retain a quiet in-ride milestone indication?

No runtime files changed and no tests executed: this branch currently contains planning only.
