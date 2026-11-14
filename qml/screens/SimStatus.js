.pragma library

function describe(state, lock, translate) {
    if (state !== "present" && state !== "locked")
        return state
    var t = translate || function(key, fallback) { return fallback }
    switch (lock) {
    case "sim-pin": return t("infoSimPinRequired", "PIN required")
    case "sim-puk": return t("infoSimPukRequired", "PUK required")
    case "sim-pin2": return t("infoSimPin2Only", "Present (PIN2 only)")
    case "sim-puk2": return t("infoSimPuk2Only", "Present (PUK2 only)")
    }
    if (state === "locked")
        return t("infoSimLocked", "Locked") + (lock && lock !== "none" ? " (" + lock + ")" : "")
    return state
}
