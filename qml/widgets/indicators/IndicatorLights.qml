import QtQuick
import ScootUI 1.0

Row {
    id: indicatorLights

    property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : Scooter.BlinkerState.Off

    spacing: 8

    IndicatorLight {
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
        active: blinkerState === Scooter.BlinkerState.Left || blinkerState === Scooter.BlinkerState.Both
        blinking: true
        blinkSource: typeof vehicleStore !== "undefined" ? vehicleStore.blinkOpacity : -1
        tintColor: "#4CAF50"
    }

    IndicatorLight {
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
        active: blinkerState === Scooter.BlinkerState.Right || blinkerState === Scooter.BlinkerState.Both
        blinking: true
        blinkSource: typeof vehicleStore !== "undefined" ? vehicleStore.blinkOpacity : -1
        tintColor: "#4CAF50"
    }
}
