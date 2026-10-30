import QtQuick

// What can be done with the words selected, in a floating note (12 px
// corners) above them: four highlight colours, Copy and Search. Over a
// highlight this app made, it offers to remove it instead.
Item {
    id: menu

    required property Item app
    property string markName: ""   // set when the selection is an existing highlight of ours

    implicitWidth: row.implicitWidth + 8
    implicitHeight: 52

    Shadow { anchors.fill: face; radius: 12 }
    Rectangle { id: face; anchors.fill: parent; radius: 12; color: look.chrome }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

        Repeater {
            model: menu.markName ? [] : pdf.highlightColours()
            Item {
                required property var modelData
                required property int index
                width: 44
                height: 44
                Accessible.role: Accessible.Button
                Accessible.name: [qsTr("Highlight yellow"), qsTr("Highlight green"), qsTr("Highlight blue"), qsTr("Highlight lavender")][index]
                Rectangle { anchors.fill: parent; radius: 22; color: look.fill(press.pressed ? 0.16 : hover.hovered ? 0.12 : 0) }
                Rectangle { anchors.centerIn: parent; width: 22; height: 22; radius: 11; color: modelData }
                HoverHandler { id: hover }
                TapHandler { id: press; onTapped: menu.app.highlightSelection(index) }
            }
        }
        Pill { visible: menu.markName !== ""; text: qsTr("Remove highlight"); onClicked: menu.app.removeHighlight(menu.markName) }
        Rectangle { width: 1; height: 24; color: look.divider; anchors.verticalCenter: parent.verticalCenter }
        Pill { text: qsTr("Copy"); onClicked: menu.app.copySelection() }
        Pill { text: qsTr("Search"); onClicked: menu.app.searchSelection() }
    }
}
