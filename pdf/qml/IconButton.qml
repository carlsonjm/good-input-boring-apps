import QtQuick

// A round glyph button, 44 px: no face at rest, the sheet's hover fill under
// a pointer, a stronger fill pressed, the selected fill when on.
Item {
    id: button

    property string glyph: ""
    property string label: ""
    // Shown above the button after a short rest of the mouse pointer; a
    // finger never hovers, so touch keeps the quiet bar.
    property string tip: label
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
    property bool rested: false
    property bool checkTip: false // the screens check shows a tip without a pointer
    HoverHandler {
        id: hover
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onHoveredChanged: { button.rested = false; if (hovered) rest.restart(); else rest.stop(); }
    }
    Timer { id: rest; interval: 500; onTriggered: button.rested = true }

    Rectangle { // the tip, a small floating note
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 12
        width: words.implicitWidth + 24
        height: 32
        radius: 12
        color: look.chrome
        z: 10
        opacity: button.rested && (hover.hovered || checkTip) && !press.pressed && button.tip !== "" ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Shadow { anchors.fill: parent; radius: 12; z: -1 }
        Text { id: words; anchors.centerIn: parent; text: button.tip; font.pixelSize: 13; color: look.ink }
    }
    TapHandler {
        id: press
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: button.clicked()
    }
}
