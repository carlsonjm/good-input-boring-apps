import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import co.goodinput.office

// Aa in Slides, in two sides. Layout and look: how the slide is laid out, what
// can be put on it, and the slide itself. Helpers: slides made from notes, and
// the chosen slide's helpers.
Rectangle {
    id: sheet

    property OfficeDocument document
    property SlideHelpers helpers
    // "look" or "helpers".
    property string side: "look"

    signal done()
    // A Codex helper was tapped without a Codex sign-in.
    signal needsSignIn()

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

        Rectangle {
            width: parent.width
            height: 48
            radius: height / 2
            color: Theme.desk
            Row {
                anchors.fill: parent
                anchors.margins: Theme.tight
                spacing: Theme.tight
                Pill { width: (parent.width - Theme.tight) / 2; height: 40; label: qsTr("Layout and look"); selected: sheet.side === "look"; onClicked: sheet.side = "look" }
                Pill { objectName: "helpersSide"; width: (parent.width - Theme.tight) / 2; height: 40; label: qsTr("Helpers"); selected: sheet.side === "helpers"; onClicked: { sheet.side = "helpers"; sheet.helpers.refresh() } }
            }
        }

        // The engine's AutoLayout numbers.
        Section {
            visible: sheet.side === "look"
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
            visible: sheet.side === "look"
            label: qsTr("Insert")
            Chip { label: qsTr("Text box"); onClicked: { sheet.document.command(".uno:Text", { "KeyModifier": 8192 }); sheet.done() } }
            Chip { label: qsTr("Picture"); onClicked: pictureDialog.open() }
            Chip { label: qsTr("Shape"); onClicked: { sheet.document.command(".uno:BasicShapes.round-rectangle", { "KeyModifier": 8192 }); sheet.done() } }
            Chip { label: qsTr("Table"); onClicked: { sheet.document.command(".uno:InsertTable", { "Columns": 3, "Rows": 3 }); sheet.done() } }
        }

        Section {
            visible: sheet.side === "look"
            label: qsTr("This slide")
            Chip { label: qsTr("Duplicate"); onClicked: sheet.document.command(".uno:DuplicatePage") }
            Chip { label: qsTr("Delete"); enabled: sheet.document && sheet.document.parts > 1; onClicked: sheet.document.command(".uno:DeletePage") }
        }

        // The Helpers side: slides from notes, by Codex, where Split Rock is here.
        Column {
            visible: sheet.side === "helpers" && sheet.helpers.available
            width: parent.width
            spacing: Theme.compact
            Text { leftPadding: Theme.tight; text: qsTr("Make slides from this"); color: Theme.secondary; font.pixelSize: Theme.metaSize }
            Rectangle {
                width: parent.width
                height: 150
                radius: 14
                color: Theme.desk
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 6
                    TextArea {
                        id: notes
                        objectName: "slideNotes"
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("Paste notes, an outline or a few paragraphs")
                        color: Theme.text
                        placeholderTextColor: Theme.secondary
                        font.pixelSize: 15
                        background: null
                    }
                }
            }
            Row {
                spacing: 10
                Chip { glyph: "file-text"; label: qsTr("Pick a Docs file"); onClicked: notesDialog.open() }
                Pill {
                    objectName: "makeSlides"
                    accent: true
                    implicitHeight: 40
                    glyph: "wand"
                    label: qsTr("Make slides")
                    enabled: notes.text.trim().length > 0 && sheet.helpers.working.length === 0
                    onClicked: {
                        if (!sheet.helpers.codexReady) {
                            sheet.needsSignIn()
                            return
                        }
                        sheet.helpers.make(notes.text)
                        notes.text = ""
                        sheet.done()
                    }
                }
            }
            Text {
                width: parent.width
                leftPadding: Theme.tight
                wrapMode: Text.Wrap
                text: qsTr("Codex writes these. Only the notes above are sent, using your Codex sign-in.")
                color: Theme.secondary
                font.pixelSize: 12
            }
        }

        Text {
            visible: sheet.side === "helpers" && !sheet.helpers.available
            width: parent.width
            leftPadding: Theme.tight
            wrapMode: Text.Wrap
            text: qsTr("Make slides and Fit this slide use Codex through Split Rock, which isn't on this computer. Tidy the look works without it.")
            color: Theme.secondary
            font.pixelSize: Theme.metaSize
        }

        Section {
            visible: sheet.side === "helpers"
            label: qsTr("For this slide")
            Chip {
                visible: sheet.helpers.available
                glyph: "wand"
                label: qsTr("Fit this slide")
                enabled: sheet.helpers.working.length === 0
                onClicked: {
                    if (!sheet.helpers.codexReady) {
                        sheet.needsSignIn()
                        return
                    }
                    sheet.helpers.fit()
                    sheet.done()
                }
            }
            Chip { glyph: "wand"; label: qsTr("Tidy the look"); enabled: sheet.helpers.working.length === 0; onClicked: { sheet.helpers.tidy(); sheet.done() } }
        }
    }

    FileDialog {
        id: notesDialog
        title: qsTr("Choose a document")
        nameFilters: [qsTr("Documents (*.odt *.fodt *.docx *.doc *.rtf *.txt *.md)")]
        onAccepted: notes.text = sheet.helpers.wordsOf(selectedFile)
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
