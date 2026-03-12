import QtQuick
import QtQuick.Layouts
import "../widgets/speedometer"
import "../widgets/status_bars"
import "../widgets/cluster"
import "../widgets/indicators"
import "../widgets/navigation"

Rectangle {
    id: clusterScreen
    color: themeStore.backgroundColor

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

            SpeedometerDisplay {
                id: speedometer
                anchors.fill: parent
            }

            TurnByTurnWidget {
                id: tbtWidget
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                z: 10
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                // Spacer to avoid overlap with TurnByTurnWidget if it's visible
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: tbtWidget.visible ? tbtWidget.height : 0
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
            Layout.fillWidth: true
        }
    }
}
