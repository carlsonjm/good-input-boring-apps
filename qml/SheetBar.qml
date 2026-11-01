import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// What a block of chosen cells can do, floating just under it: their sum
// (a tap writes it below them), sort, fill down, money and percent, bold,
// then copy and cut.
Rectangle {
    id: bar

    property OfficeDocument document
    property PageView view
    // "Sum 2,025", from the engine's own sum of the chosen cells.
    property string sum: ""
    readonly property var states: document ? document.states : ({})

    signal done()

    color: Theme.chrome
    radius: height / 2
    implicitWidth: buttons.implicitWidth + 2 * Theme.tight
    implicitHeight: 52

    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: -6
        radius: parent.radius
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.12)
    }

    Row {
        id: buttons
        anchors.centerIn: parent
        spacing: 2

        Pill {
            visible: bar.sum.length > 0
            label: bar.sum
            labelFont.weight: Font.DemiBold
            Accessible.name: qsTr("Write the sum below")
            onClicked: { bar.document.command(".uno:AutoSum"); bar.document.pressKey(1280); bar.done() }
        }
        Rectangle { visible: bar.sum.length > 0; width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        IconPill { glyph: "arrow-down-a-z"; Accessible.name: qsTr("Sort A to Z"); onClicked: bar.document.command(".uno:SortAscending") }
        IconPill { glyph: "arrow-down"; Accessible.name: qsTr("Fill down"); onClicked: bar.document.command(".uno:FillDown") }
        Pill { label: "$"; implicitWidth: 44; Accessible.name: qsTr("Money"); selected: bar.states[".uno:NumberFormatCurrency"] === "true"; onClicked: bar.document.command(".uno:NumberFormatCurrency") }
        Pill { label: "%"; implicitWidth: 44; Accessible.name: qsTr("Percent"); selected: bar.states[".uno:NumberFormatPercent"] === "true"; onClicked: bar.document.command(".uno:NumberFormatPercent") }
        Pill { label: "B"; implicitWidth: 44; labelFont.bold: true; Accessible.name: qsTr("Bold"); selected: bar.states[".uno:Bold"] === "true"; onClicked: bar.document.command(".uno:Bold") }
        Rectangle { width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        Pill { label: qsTr("Copy"); onClicked: { bar.view.copySelection(false); bar.done() } }
        Pill { label: qsTr("Cut"); onClicked: { bar.view.copySelection(true); bar.done() } }
    }
}
