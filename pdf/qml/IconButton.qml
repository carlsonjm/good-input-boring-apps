import QtQuick

// A round glyph button, 44 px: no face at rest, the sheet's hover fill under
// a pointer, a stronger fill pressed, the selected fill when on.
Item {
    id: button

    property string glyph: ""
    property string label: ""
    property bool on: false
    property color ink: look.ink
    signal clicked()

    implicitWidth: 44
    implicitHeight: 44
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: clicked()

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: look.fill(press.pressed ? 0.16 : button.on ? 0.24 : hover.hovered ? 0.12 : 0)
        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }
    Image {
        anchors.centerIn: parent
        width: 20
        height: 20
        sourceSize: Qt.size(Math.ceil(20 * Screen.devicePixelRatio), Math.ceil(20 * Screen.devicePixelRatio))
        source: button.glyph ? "image://glyph/" + button.glyph + "/" + button.ink : ""
    }
    HoverHandler { id: hover }
    TapHandler {
        id: press
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: button.clicked()
    }
}
