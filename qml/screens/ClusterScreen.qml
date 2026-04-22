import QtQuick
import QtQuick.Layouts
import "../widgets/speedometer"
import "../widgets/status_bars"
import "../widgets/cluster"
import "../widgets/indicators"
import "../widgets/navigation"
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
            // Confetti only fires on km milestones, TBT is only visible
            // during active navigation. Both incubate asynchronously
            // after the cluster is rendered.
            Loader {
                anchors.fill: parent
                asynchronous: true
                sourceComponent: Component { MilestoneConfettiLayer { anchors.fill: parent } }
            }

            SpeedometerDisplay {
                id: speedometer
                anchors.fill: parent
            }

            Loader {
                id: tbtLoader
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: item ? item.height : 0
                z: 10
                asynchronous: true
                sourceComponent: Component {
                    TurnByTurnWidget {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                // Spacer to avoid overlap with TurnByTurnWidget if it's visible.
                // tbtLoader.item is null until async incubation completes.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: tbtLoader.item && tbtLoader.item.visible ? tbtLoader.item.height : 0
                }

                BlinkerRow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                }

                Item { Layout.fillHeight: true }

                ClusterBottomBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                }
            }
        }

        UnifiedBottomStatusBar {
            id: bottomBar
            Layout.fillWidth: true
        }
    }

    readonly property real bottomBarHeight: bottomBar.height
}
