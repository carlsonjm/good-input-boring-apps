import QtQuick
import QtQuick.Controls.Basic as T

// A control: half its height round. Resting raised or clear, selected at the
// suite's selected fill, or filled with the accent for the one main action.
T.AbstractButton {
    id: pill

    property bool raised: false
    property bool selected: false
    property bool accent: false
    property alias label: caption.text
    property alias labelFont: caption.font
    // A Lucide icon by name, drawn in the ink that reads on the pill.
    property string glyph: ""

    implicitHeight: Theme.control
    implicitWidth: Math.max(Theme.control, row.implicitWidth + (pill.glyph.length ? 32 : 36))
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    background: Rectangle {
        radius: height / 2
        color: pill.accent ? Theme.accent
             : pill.selected ? Theme.selected
             : pill.down ? Theme.selected
             : pill.hovered ? Theme.hover
             : pill.raised ? Theme.raised : "transparent"
        opacity: pill.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: Theme.micro; easing.type: Easing.OutCubic } }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.compact
            Image {
                visible: pill.glyph.length > 0
                source: pill.glyph.length ? Qt.resolvedUrl((pill.accent || !Theme.dark ? "icons/ink/" : "icons/") + pill.glyph + ".svg") : ""
                width: 20; height: 20
                sourceSize: Qt.size(40, 40)
                anchors.verticalCenter: parent.verticalCenter
                opacity: pill.enabled ? 1 : 0.4
            }
            Text {
                id: caption
                visible: text.length > 0
                color: pill.accent ? Theme.accentInk : Theme.text
                font.pixelSize: 15
                font.weight: pill.accent ? Font.DemiBold : Font.Normal
                anchors.verticalCenter: parent.verticalCenter
                opacity: pill.enabled ? 1 : 0.4
            }
        }
    }
}
