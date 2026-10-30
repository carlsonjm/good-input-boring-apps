import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import co.goodinput.office

// Aa in Slides: how the slide is laid out, what can be put on it, and the
// slide itself, in one note under its button.
Rectangle {
    id: sheet

    property OfficeDocument document

    signal done()

    color: Theme.chrome
    radius: Theme.noteRadius
    width: 400
    implicitHeight: column.implicitHeight + 2 * Theme.comfortable

    component Section: Column {
        property alias label: heading.text
        default property alias controls: flow.data
        width: parent.width
        spacing: Theme.compact
        Text { id: heading; leftPadding: Theme.tight; color: Theme.secondary; font.pixelSize: Theme.metaSize }
        Flow { id: flow; width: parent.width; spacing: 10 }
    }
    component Chip: Pill { raised: true; implicitHeight: 40 }

    // A small drawing of a layout; `layout` is the engine's number for it.
    component Layout: AbstractButton {
        id: card
        property int layout
        property string name
        // Which bars to draw: title, body, two columns, centred title.
        property bool title: false
        property bool body: false
        property bool columns: false
        property bool centred: false
        implicitWidth: 112
        implicitHeight: 86
        hoverEnabled: true
        onClicked: sheet.document.command(".uno:AssignLayout", { "WhatLayout": layout })
        contentItem: Column {
            spacing: 6
            Rectangle {
                width: 112; height: 62
                radius: 6
                color: "white"
                border.width: card.hovered ? 2 : 0
                border.color: Theme.accent
                Rectangle { visible: card.title; x: 10; y: 9; width: 52; height: 7; radius: 2; color: "#C8C8D0" }
                Repeater {
                    model: card.body ? 3 : 0
                    Rectangle { required property int index; x: 10; y: 24 + index * 9; width: 80 - index * 14; height: 4; radius: 2; color: "#E0E0E6" }
                }
                Rectangle { visible: card.columns; x: 10; y: 24; width: 42; height: 28; radius: 2; color: "#E0E0E6" }
                Rectangle { visible: card.columns; x: 60; y: 24; width: 42; height: 28; radius: 2; color: "#E0E0E6" }
                Rectangle { visible: card.centred; anchors.centerIn: parent; width: 70; height: 8; radius: 2; color: "#C8C8D0" }
                Rectangle { visible: card.centred && card.layout === 0; anchors.horizontalCenter: parent.horizontalCenter; y: 40; width: 46; height: 4; radius: 2; color: "#E0E0E6" }
            }
            Text { text: card.name; color: Theme.secondary; font.pixelSize: 12; anchors.horizontalCenter: parent.horizontalCenter }
        }
        background: null
    }

    Column {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.comfortable
        spacing: 14

        // The engine's AutoLayout numbers.
        Section {
            label: qsTr("Layout")
            Layout { layout: 0; name: qsTr("Title"); centred: true }
            Layout { layout: 1; name: qsTr("Title and text"); title: true; body: true }
            Layout { layout: 3; name: qsTr("Two columns"); title: true; columns: true }
            Layout { layout: 19; name: qsTr("Title only"); title: true }
            Layout { layout: 32; name: qsTr("Centred text"); body: true }
            Layout { layout: 20; name: qsTr("Blank") }
        }

        // With the Ctrl modifier the engine puts a new box in the middle of
        // the slide instead of waiting for it to be drawn.
        Section {
            label: qsTr("Insert")
            Chip { label: qsTr("Text box"); onClicked: { sheet.document.command(".uno:Text", { "KeyModifier": 8192 }); sheet.done() } }
            Chip { label: qsTr("Picture"); onClicked: pictureDialog.open() }
            Chip { label: qsTr("Shape"); onClicked: { sheet.document.command(".uno:BasicShapes.round-rectangle", { "KeyModifier": 8192 }); sheet.done() } }
            Chip { label: qsTr("Table"); onClicked: { sheet.document.command(".uno:InsertTable", { "Columns": 3, "Rows": 3 }); sheet.done() } }
        }

        Section {
            label: qsTr("This slide")
            Chip { label: qsTr("Duplicate"); onClicked: sheet.document.command(".uno:DuplicatePage") }
            Chip { label: qsTr("Delete"); enabled: sheet.document && sheet.document.parts > 1; onClicked: sheet.document.command(".uno:DeletePage") }
        }
    }

    FileDialog {
        id: pictureDialog
        title: qsTr("Choose a picture")
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.gif *.webp *.svg)")]
        onAccepted: {
            sheet.document.command(".uno:InsertGraphic", { "FileName": String(selectedFile) })
            sheet.done()
        }
    }
}
