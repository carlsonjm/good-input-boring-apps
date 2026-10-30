import QtQuick

// The one row of controls, floating 40 px above the bottom edge: the file's
// name and page with the reading tools, or what the moment needs instead,
// such as the form's next field or where to sign.
Item {
    id: bar

    required property Item app
    readonly property string mode: app.mode
    property bool shown: true

    implicitWidth: row.implicitWidth + row.anchors.leftMargin + 4
    implicitHeight: 52

    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    Shadow { anchors.fill: face; radius: face.radius }
    Rectangle {
        id: face
        anchors.fill: parent
        radius: height / 2
        color: Qt.alpha(look.chrome, 0.94)
    }
    HoverHandler { onHoveredChanged: bar.app.barHovered = hovered }

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: words.visible ? 20 : 4
        spacing: 4

        Text {
            id: words
            visible: text.length > 0
            anchors.verticalCenter: parent.verticalCenter
            rightPadding: 8
            textFormat: Text.PlainText
            font.pixelSize: 13
            color: look.softInk
            elide: Text.ElideMiddle
            width: Math.min(implicitWidth, 320)
            text: {
                switch (bar.mode) {
                case "read": return pdf.title;
                case "highlight": return qsTr("Double-tap or hold a word, then drag its ends");
                case "selecting": return qsTr("Drag the blue ends to change what is selected");
                case "form": {
                    const n = pdf.stillNeeded;
                    const where = qsTr("Field %1 of %2").arg(bar.app.fieldNumber).arg(pdf.fieldCount);
                    return where + " · " + (n > 0 ? qsTr("%1 still empty").arg(n) : qsTr("All filled"));
                }
                case "signing": return qsTr("Tap where your signature goes");
                case "signed": return qsTr("Signed");
                case "message": return bar.app.message;
                }
                return "";
            }
        }
        Text {
            visible: bar.mode === "read"
            anchors.verticalCenter: parent.verticalCenter
            rightPadding: 8
            font.pixelSize: 13
            color: look.ink
            text: qsTr("%1 of %2").arg(pdf.page + 1).arg(pdf.pageCount)
        }

        IconButton { visible: bar.mode === "read"; glyph: "search"; label: qsTr("Search"); onClicked: bar.app.openPages(true) }
        IconButton { visible: bar.mode === "read"; glyph: "highlighter"; label: qsTr("Highlight"); onClicked: bar.app.mode = "highlight" }
        IconButton { visible: bar.mode === "read"; glyph: "signature"; label: qsTr("Sign"); onClicked: bar.app.openSign() }
        IconButton { visible: bar.mode === "read"; glyph: "layout-grid"; label: qsTr("All pages"); onClicked: bar.app.openPages(false) }
        IconButton { visible: bar.mode === "read"; glyph: "ellipsis"; label: qsTr("More: open, save a copy, your signature"); onClicked: bar.app.openMore() }

        IconButton { visible: bar.mode === "form"; glyph: "chevron-up"; label: qsTr("Previous field"); onClicked: bar.app.goToField(pdf.previousField(bar.app.field)) }
        IconButton { visible: bar.mode === "form"; glyph: "chevron-down"; label: qsTr("Next field"); onClicked: bar.app.goToField(pdf.nextField(bar.app.field)) }

        Pill { visible: bar.mode === "form" || bar.mode === "highlight"; text: qsTr("Done"); on: true; onClicked: bar.app.backToReading() }
        Pill { visible: bar.mode === "signing"; text: qsTr("Cancel"); onClicked: bar.app.backToReading() }
        Pill { visible: bar.mode === "signed"; text: qsTr("Undo"); onClicked: bar.app.undoSignature() }
        // Words alone end with the same room they start with.
        Item { width: 16; height: 1; visible: bar.mode === "selecting" || bar.mode === "message" }
    }
}
