import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// What a selection can do, floating just above it: bold, italic, underline,
// highlight, link, comment, then copy and cut. A note-tier surface (12 px).
Rectangle {
    id: bar

    property OfficeDocument document
    property PageView view
    readonly property var states: document ? document.states : ({})

    // Asks for one line of text (a link's address, a comment) in place.
    property string asking: ""

    signal done()

    color: Theme.chrome
    radius: Theme.noteRadius
    implicitWidth: (asking.length ? ask.implicitWidth : buttons.implicitWidth) + 12
    implicitHeight: Theme.control + 12

    Row {
        id: buttons
        visible: !bar.asking.length
        anchors.centerIn: parent
        spacing: Theme.tight

        Pill {
            label: "B"; labelFont.bold: true; labelFont.pixelSize: 17
            Accessible.name: qsTr("Bold")
            selected: bar.states[".uno:Bold"] === "true"
            onClicked: bar.document.command(".uno:Bold")
        }
        Pill {
            label: "I"; labelFont.italic: true; labelFont.pixelSize: 17; labelFont.family: "serif"
            Accessible.name: qsTr("Italic")
            selected: bar.states[".uno:Italic"] === "true"
            onClicked: bar.document.command(".uno:Italic")
        }
        Pill {
            label: "U"; labelFont.underline: true; labelFont.pixelSize: 17
            Accessible.name: qsTr("Underline")
            selected: bar.states[".uno:Underline"] === "true"
            onClicked: bar.document.command(".uno:Underline")
        }
        Rectangle { width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        IconPill {
            glyph: "highlighter"
            Accessible.name: qsTr("Highlight")
            // Yellow, the one highlight people expect.
            onClicked: bar.document.command(".uno:CharBackColor", { "CharBackColor": 16776960 })
        }
        IconPill {
            glyph: "link"
            Accessible.name: qsTr("Link")
            onClicked: { bar.asking = "link"; field.text = ""; field.forceActiveFocus() }
        }
        IconPill {
            glyph: "message-circle"
            Accessible.name: qsTr("Comment")
            onClicked: { bar.asking = "comment"; field.text = ""; field.forceActiveFocus() }
        }
        Rectangle { width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        Pill { label: qsTr("Copy"); onClicked: { bar.view.copySelection(false); bar.done() } }
        Pill { label: qsTr("Cut"); onClicked: { bar.view.copySelection(true); bar.done() } }
    }

    Row {
        id: ask
        visible: bar.asking.length > 0
        anchors.centerIn: parent
        spacing: Theme.tight
        TextField {
            id: field
            width: 300
            height: Theme.control
            color: Theme.text
            placeholderText: bar.asking === "link" ? qsTr("Address, like goodinput.co") : qsTr("Comment")
            placeholderTextColor: Theme.secondary
            font.pixelSize: 15
            leftPadding: 16
            background: Rectangle { radius: height / 2; color: Theme.raised }
            onAccepted: apply.clicked()
        }
        Pill {
            id: apply
            accent: true
            label: bar.asking === "link" ? qsTr("Link") : qsTr("Add")
            enabled: field.text.length > 0
            onClicked: {
                if (bar.asking === "link") {
                    let url = field.text.trim()
                    if (!/^[a-z]+:/i.test(url))
                        url = "https://" + url
                    bar.document.command(".uno:HyperlinkDialog", { "Hyperlink.URL": url, "Hyperlink.Text": bar.document.selectedText() })
                } else {
                    bar.document.command(".uno:InsertAnnotation", { "Text": field.text })
                }
                bar.asking = ""
                bar.done()
            }
        }
        Pill { label: qsTr("Cancel"); onClicked: bar.asking = "" }
    }
}
