import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

ColumnLayout {
    id: root
    property var commands
    property bool active: false
    property int pinMask: -1
    property double lastReply: 0
    spacing: 4
    onActiveChanged: { pinMask = -1; lastReply = 0 }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 6
        Repeater {
            model: ["SH · Hall A", "AH · Hall B", "Speed · Hall C", "PAS · PPM"]
            delegate: RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: 28
                Rectangle {
                    objectName: "mobileSpeedpedPin" + index
                    width: 14; height: 14; radius: 7
                    color: root.pinMask < 0 ? "#808080" : (root.pinMask & (1 << index)) ? "#e53935" : "#27ae60"
                    Accessible.name: modelData
                    Accessible.description: root.pinMask < 0 ? "Keine Daten" : (root.pinMask & (1 << index)) ? "GND" : "High"
                }
                Label { text: modelData; Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }
    }
    Label {
        Layout.fillWidth: true
        text: "Rot = GND · Grün = High · Grau = keine Daten"
        wrapMode: Text.WordWrap
        font.pixelSize: 12
    }
    Timer {
        interval: 100; repeat: true; running: root.active; triggeredOnStart: true
        onTriggered: {
            if (Date.now() - root.lastReply > 500) root.pinMask = -1
            if (root.commands) root.commands.requestSpeedpedPinLevels()
        }
    }
    Connections {
        target: root.commands
        function onSpeedpedPinLevelsReceived(mask) {
            if (!root.active) return
            root.pinMask = mask
            root.lastReply = Date.now()
        }
    }
}
