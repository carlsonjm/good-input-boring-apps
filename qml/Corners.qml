import QtQuick

// Rounds a sheet of paper's corners to the paper tier by laying the desk's
// colour round the paper outside the curve, as PDF does. One border whose
// inner edge is the curve does it, drawn by the scene graph at the screen's
// own pixels so no sliver of paper shows at any scaling. `inset` pulls the
// edge in, to trim the thin outline the engine draws round a slide; `reach`
// covers what the engine draws just outside a page, and no further, so
// nothing beside the paper is covered.
Item {
    id: corners
    property real radius: Theme.paperRadius
    property color ground: Theme.desk
    property real inset: 0
    property real reach: 0

    // From the curve out to the corner itself, with a pixel spare.
    readonly property real band: Math.ceil(radius * 0.42) + 2 + inset + reach

    Item {
        anchors.fill: parent
        anchors.margins: -corners.reach
        clip: true
        Rectangle {
            x: corners.reach + corners.inset - corners.band
            y: corners.reach + corners.inset - corners.band
            width: corners.width - 2 * corners.inset + 2 * corners.band
            height: corners.height - 2 * corners.inset + 2 * corners.band
            radius: corners.radius + corners.band
            color: "transparent"
            border.width: corners.band
            border.color: corners.ground
            antialiasing: true
        }
    }
}
