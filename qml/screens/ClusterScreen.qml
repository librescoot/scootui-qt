import QtQuick
import QtQuick.Layouts
import "../widgets/speedometer"
import "../widgets/status_bars"
import "../widgets/cluster"
import "../widgets/indicators"
import "../notifications"
import "../widgets/components"
Rectangle {
    id: clusterScreen
    color: themeStore.backgroundColor

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] ClusterScreen completed")

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Rare and event-driven — don't hold up the first paint.
            // Confetti only fires on km milestones and incubates asynchronously
            // after the cluster is rendered.
            Loader {
                anchors.fill: parent
                asynchronous: true
                sourceComponent: Component { MilestoneConfettiLayer { anchors.fill: parent } }
            }

            SpeedometerDisplay {
                id: speedometer
                objectName: "clusterSpeedometer"
                anchors.fill: parent
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 0

                        Item { Layout.fillHeight: true }

                        ClusterBottomBar {
                            objectName: "clusterPowerAndTelltales"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                        }
                    }
                }
            }
        }

        UnifiedBottomStatusBar {
            id: bottomBar
            Layout.fillWidth: true
        }
    }

    UnifiedAttentionDock {
        id: attentionDock
        objectName: "clusterAttention"
        y: 40
        width: parent.width
        z: 20
    }

    BlinkerRow {
        objectName: "clusterBlinkers"
        x: 8
        y: 48 + attentionDock.height
        width: parent.width - 16
        z: 30
    }

    Binding {
        target: typeof notificationService !== "undefined" ? notificationService : null
        property: "surface"
        value: "cluster"
    }

    readonly property real bottomBarHeight: bottomBar.height
}
