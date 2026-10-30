import QtQuick

// The few things that are not reading, in a floating note above the bar.
Item {
    id: menu

    required property Item app
    signal done()

    Rectangle { anchors.fill: parent; color: "transparent"; TapHandler { onTapped: menu.done() } }

    Rectangle {
        id: note
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40 + 52 + 8
        width: column.implicitWidth + 8
        height: column.implicitHeight + 8
        radius: 12
        color: look.chrome
        Shadow { anchors.fill: parent; radius: 12; z: -1 }
        Accessible.role: Accessible.PopupMenu
        Column {
            id: column
            x: 4
            y: 4
            spacing: 0
            Repeater {
                model: [
                    { text: qsTr("Open a PDF…"), act: "open", show: true },
                    { text: qsTr("Save now"), act: "save", show: pdf.changed },
                    { text: qsTr("Save a copy…"), act: "copy", show: pdf.status === "open" },
                    { text: qsTr("Forget my signature"), act: "forget", show: pdf.hasSignature }
                ].filter(item => item.show)
                Item {
                    required property var modelData
                    width: 240
                    height: 44
                    Accessible.role: Accessible.MenuItem
                    Accessible.name: modelData.text
                    Rectangle { anchors.fill: parent; radius: 10; color: look.fill(press.pressed ? 0.16 : hover.hovered ? 0.12 : 0) }
                    Text { id: words; x: 16; anchors.verticalCenter: parent.verticalCenter; text: modelData.text; font.pixelSize: 15; color: look.ink }
                    HoverHandler { id: hover }
                    TapHandler { id: press; onTapped: { menu.done(); menu.app.act(modelData.act); } }
                }
            }
        }
    }
}
