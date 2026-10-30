import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import co.goodinput.office

// Opened with no file: a find field, the documents used lately as wide rows,
// newest first, and one way to start a new one.
Item {
    id: home

    property string kind: "text"
    property string newLabel: qsTr("New document")
    property string findLabel: qsTr("Find a document")

    signal openRequested(url url)
    signal newRequested()

    function refresh() { recent.reload() }

    Recent {
        id: recent
        kind: home.kind
        filter: find.text
    }

    Rectangle { anchors.fill: parent; color: Theme.desk }

    Column {
        id: top
        width: Math.min(parent.width - 2 * Theme.surface, 760)
        anchors.horizontalCenter: parent.horizontalCenter
        y: 32
        spacing: Theme.tight

        Rectangle {
            width: parent.width
            height: 48
            radius: 24
            color: Theme.raised
            Image {
                source: Qt.resolvedUrl(Theme.dark ? "icons/search.svg" : "icons/ink/search.svg")
                width: 20; height: 20; sourceSize: Qt.size(40, 40)
                x: 20; anchors.verticalCenter: parent.verticalCenter
                opacity: 0.66
            }
            TextField {
                id: find
                anchors.fill: parent
                leftPadding: 52
                color: Theme.text
                font.pixelSize: Theme.bodySize
                placeholderText: home.findLabel
                placeholderTextColor: Theme.secondary
                background: null
                Accessible.name: home.findLabel
            }
        }
        Item { width: 1; height: Theme.comfortable }
        Text {
            text: recent.count ? qsTr("Recent") : (find.text.length ? qsTr("Nothing by that name") : qsTr("Nothing opened yet"))
            color: Theme.secondary
            font.pixelSize: Theme.metaSize
            leftPadding: Theme.standard
            bottomPadding: Theme.tight
        }
    }

    ListView {
        id: list
        anchors.top: top.bottom
        anchors.bottom: parent.bottom
        width: top.width
        anchors.horizontalCenter: parent.horizontalCenter
        clip: true
        model: recent
        spacing: Theme.tight
        boundsBehavior: Flickable.StopAtBounds
        footer: Item {
            width: list.width
            height: 120
            Pill {
                y: Theme.compact
                label: qsTr("Open from Files")
                onClicked: openDialog.open()
            }
        }
        delegate: AbstractButton {
            id: row
            required property string name
            required property url url
            required property string place
            required property string when
            width: list.width
            height: 72
            hoverEnabled: true
            onClicked: home.openRequested(url)
            background: Rectangle {
                radius: Theme.paperRadius
                color: row.down ? Theme.selected : row.hovered ? Theme.hover : "transparent"
            }
            contentItem: Item {
                Rectangle {
                    id: thumb
                    x: Theme.standard
                    anchors.verticalCenter: parent.verticalCenter
                    width: home.kind === "text" ? 40 : 56
                    height: home.kind === "text" ? 52 : 40
                    radius: 4
                    color: "#F8F8FF"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 4
                        Rectangle { width: parent.width * 0.7; height: 3; color: "#9A9A9A" }
                        Rectangle { width: parent.width; height: 2; color: "#C8C8C8" }
                        Rectangle { width: parent.width; height: 2; color: "#C8C8C8" }
                        Rectangle { width: parent.width * 0.8; height: 2; color: "#C8C8C8" }
                    }
                }
                Column {
                    anchors.left: thumb.right
                    anchors.leftMargin: Theme.comfortable
                    anchors.right: whenText.left
                    anchors.rightMargin: Theme.standard
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { text: row.name; color: Theme.text; font.pixelSize: Theme.bodySize; elide: Text.ElideRight; width: parent.width }
                    Text { text: row.place; color: Theme.secondary; font.pixelSize: Theme.metaSize; elide: Text.ElideMiddle; width: parent.width }
                }
                Text {
                    id: whenText
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.standard
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.when
                    color: Theme.secondary
                    font.pixelSize: Theme.metaSize
                }
            }
        }
    }

    Pill {
        accent: true
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 32
        height: 52
        implicitHeight: 52
        glyph: "plus"
        label: home.newLabel
        labelFont.pixelSize: 16
        onClicked: home.newRequested()
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open")
        nameFilters: home.kind === "spreadsheet" ? [qsTr("Spreadsheets (*.xlsx *.xls *.ods *.csv)")]
                   : home.kind === "presentation" ? [qsTr("Presentations (*.pptx *.ppt *.odp)")]
                   : [qsTr("Documents (*.docx *.doc *.odt *.rtf *.txt)")]
        onAccepted: home.openRequested(selectedFile)
    }
}
