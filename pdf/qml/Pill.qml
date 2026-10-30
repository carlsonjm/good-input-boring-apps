import QtQuick

// A word button, 44 px tall, its radius half its height. `primary` fills it
// with the ink, for the one action a sheet is for.
Item {
    id: pill

    property string text: ""
    property bool primary: false
    property bool raised: false
    property bool on: false
    signal clicked()

    implicitHeight: 44
    implicitWidth: words.implicitWidth + 2 * (primary ? 20 : 16)
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: clicked()

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: pill.primary ? (press.pressed ? Qt.darker(look.ink, 1.1) : look.ink)
             : pill.raised ? Qt.tint(look.raised, look.fill(press.pressed ? 0.16 : hover.hovered ? 0.08 : 0))
             : look.fill(press.pressed ? 0.16 : pill.on ? 0.24 : hover.hovered ? 0.12 : 0)
        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }
    Text {
        id: words
        anchors.centerIn: parent
        text: pill.text
        color: pill.primary ? look.chrome : look.ink
        font.pixelSize: 15
        font.weight: pill.primary ? Font.DemiBold : Font.Medium
    }
    HoverHandler { id: hover }
    TapHandler {
        id: press
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: pill.clicked()
    }
}
