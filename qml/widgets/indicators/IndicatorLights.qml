import QtQuick
import ScootUI 1.0

Row {
    id: indicatorLights

    property int blinkerState: typeof VehicleStore !== "undefined" ? VehicleStore.blinkerState : 0
    // 0=Off, 1=Left, 2=Right, 3=Both

    spacing: 8

    IndicatorLight {
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
        active: blinkerState === 1 || blinkerState === 3
        blinking: true
        blinkSource: typeof VehicleStore !== "undefined" ? VehicleStore.blinkOpacity : -1
        tintColor: "#4CAF50"
    }

    IndicatorLight {
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
        active: blinkerState === 2 || blinkerState === 3
        blinking: true
        blinkSource: typeof VehicleStore !== "undefined" ? VehicleStore.blinkOpacity : -1
        tintColor: "#4CAF50"
    }
}
