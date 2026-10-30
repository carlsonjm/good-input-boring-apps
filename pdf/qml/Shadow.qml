import QtQuick

// A soft shadow under a floating surface, built from a few widening layers so
// it needs no effects module: about 24 px of blur, 6 px down, as the mock-up.
Item {
    id: shadow
    property real radius: 12
    property color tint: look.shadow

    Repeater {
        model: 6
        Rectangle {
            required property int index
            readonly property real spread: 2 + index * 4
            x: -spread
            y: 6 - spread
            width: shadow.width + 2 * spread
            height: shadow.height + 2 * spread
            radius: shadow.radius + spread
            color: Qt.alpha(shadow.tint, shadow.tint.a * 0.12 * (1 - index / 6))
        }
    }
}
