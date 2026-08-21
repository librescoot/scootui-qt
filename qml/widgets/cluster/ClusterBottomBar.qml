import QtQuick
import ScootUI 1.0

Item {
    id: clusterBottom
    height: 60

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] ClusterBottomBar completed")

    // Telltales live bottom-left (same spot as the map view); the power bar
    // stays centered and only yields when the panel would actually run into it.
    readonly property bool panelIntrudes: telltalePanel.visible
                                          && telltalePanel.width + 4 > (width - 200) / 2

    TelltalePanel {
        id: telltalePanel
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
    }

    PowerDisplay {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 200
        height: 56
        visible: opacity > 0
        opacity: clusterBottom.panelIntrudes ? 0 : 1

        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }
    }
}
