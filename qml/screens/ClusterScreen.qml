import QtQuick
import QtQuick.Layouts
import "../widgets/speedometer"
import "../widgets/status_bars"
import "../widgets/cluster"
import "../widgets/indicators"

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

            // Speed limit + road name below speedometer center
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 68
                spacing: 4

                SpeedLimitIndicator {
                    iconSize: 27
                    anchors.verticalCenter: parent.verticalCenter
                }

                RoadNameDisplay {
                    anchors.verticalCenter: parent.verticalCenter
                    fontSize: 13
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

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
