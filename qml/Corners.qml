import QtQuick

// Rounds a sheet of paper's corners to the paper tier by laying the desk's
// colour over the four corners outside the curve, as PDF does.
Item {
    id: corners
    property real radius: Theme.paperRadius
    property color ground: Theme.desk

    Repeater {
        model: 4
        Canvas {
            required property int index
            width: corners.radius
            height: corners.radius
            x: index % 2 ? corners.width - width : 0
            y: index > 1 ? corners.height - height : 0
            rotation: [0, 90, 270, 180][index]
            property color ground: corners.ground
            onGroundChanged: requestPaint()
            onPaint: {
                const c = getContext("2d")
                c.reset()
                c.fillStyle = ground
                c.beginPath()
                c.moveTo(0, 0)
                c.lineTo(width, 0)
                c.arc(width, height, width, 1.5 * Math.PI, Math.PI, true)
                c.closePath()
                c.fill()
            }
        }
    }
}
