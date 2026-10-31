import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// What a chosen box on a slide can do, floating just under it: bold, italic,
// underline, smaller and larger, its text colour, centring, bringing it to
// the front, and deleting it.
Rectangle {
    id: bar

    property OfficeDocument document
    readonly property var states: document ? document.states : ({})
    property bool choosingColour: false

    signal done()

    color: Theme.chrome
    radius: height / 2
    implicitWidth: (choosingColour ? colours.implicitWidth : buttons.implicitWidth) + 2 * Theme.tight
    implicitHeight: 52

    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: -6
        radius: parent.radius
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.12)
    }

    // The deck's own colours first, then plain ones.
    readonly property var palette: ["#0F3D3E", "#F2B134", "#1A1A1A", "#FFFFFF", "#E2543B", "#3DAEE9", "#7A9E7E"]

    Row {
        id: buttons
        visible: !bar.choosingColour
        anchors.centerIn: parent
        spacing: 2
        Pill { label: "B"; implicitWidth: 44; labelFont.bold: true; Accessible.name: qsTr("Bold"); selected: bar.states[".uno:Bold"] === "true"; onClicked: bar.document.command(".uno:Bold") }
        Pill { label: "I"; implicitWidth: 44; labelFont.italic: true; labelFont.family: "serif"; Accessible.name: qsTr("Italic"); selected: bar.states[".uno:Italic"] === "true"; onClicked: bar.document.command(".uno:Italic") }
        Pill { label: "U"; implicitWidth: 44; labelFont.underline: true; Accessible.name: qsTr("Underline"); selected: bar.states[".uno:Underline"] === "true"; onClicked: bar.document.command(".uno:Underline") }
        Pill { label: "A−"; implicitWidth: 48; Accessible.name: qsTr("Smaller"); onClicked: bar.document.command(".uno:Shrink") }
        Pill { label: "A+"; implicitWidth: 48; Accessible.name: qsTr("Larger"); onClicked: bar.document.command(".uno:Grow") }
        AbstractButton {
            implicitWidth: 44
            implicitHeight: 44
            Accessible.name: qsTr("Text colour")
            onClicked: bar.choosingColour = true
            contentItem: Item {
                Rectangle { anchors.centerIn: parent; width: 22; height: 22; radius: 11; color: bar.palette[0]; border.width: 2; border.color: Theme.text }
            }
            background: Rectangle { radius: 22; color: parent.hovered ? Theme.hover : "transparent" }
        }
        IconPill { glyph: "align-center"; Accessible.name: qsTr("Centre"); selected: bar.states[".uno:CenterPara"] === "true"; onClicked: bar.document.command(bar.states[".uno:CenterPara"] === "true" ? ".uno:LeftPara" : ".uno:CenterPara") }
        Rectangle { width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        IconPill { glyph: "layers"; Accessible.name: qsTr("Bring to front"); onClicked: bar.document.command(".uno:BringToFront") }
        IconPill { glyph: "trash-2"; Accessible.name: qsTr("Delete"); onClicked: { bar.document.command(".uno:Delete"); bar.done() } }
    }

    Row {
        id: colours
        visible: bar.choosingColour
        anchors.centerIn: parent
        spacing: 2
        Repeater {
            model: bar.palette
            AbstractButton {
                required property string modelData
                implicitWidth: 44
                implicitHeight: 44
                onClicked: {
                    bar.document.command(".uno:Color", { "Color": parseInt(modelData.slice(1), 16) })
                    bar.choosingColour = false
                }
                contentItem: Item {
                    Rectangle { anchors.centerIn: parent; width: 26; height: 26; radius: 13; color: modelData; border.width: 1; border.color: Theme.divider }
                }
                background: Rectangle { radius: 22; color: parent.hovered ? Theme.hover : "transparent" }
            }
        }
        IconPill { glyph: "x"; Accessible.name: qsTr("Back"); onClicked: bar.choosingColour = false }
    }
}
