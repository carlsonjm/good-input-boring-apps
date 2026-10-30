import QtQuick

// A locked PDF asks for its password in a floating note.
Item {
    id: sheet

    required property Item app
    property url file
    property bool wrong: false

    function ask(url, wasWrong) {
        file = url;
        wrong = wasWrong;
        secret.text = "";
        secret.forceActiveFocus();
    }
    function tryIt() {
        if (!pdf.open(file, secret.text))
            ask(file, pdf.status === "wrongPassword");
    }

    Rectangle { anchors.fill: parent; color: Qt.alpha(look.desk, 0.55) }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 32)
        height: column.implicitHeight + 48
        radius: 12
        color: look.chrome
        Shadow { anchors.fill: parent; radius: 12; z: -1 }
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("This PDF is locked")
        Column {
            id: column
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 16
            Text {
                text: qsTr("This PDF is locked")
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: look.ink
            }
            Rectangle {
                width: parent.width
                height: 44
                radius: 22
                color: look.raised
                border.width: 2
                border.color: secret.activeFocus ? look.strongBorder : "transparent"
                TextInput {
                    id: secret
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    verticalAlignment: TextInput.AlignVCenter
                    echoMode: TextInput.Password
                    color: look.ink
                    font.pixelSize: 15
                    Accessible.name: qsTr("Password")
                    Keys.onReturnPressed: sheet.tryIt()
                    Keys.onEscapePressed: sheet.app.closeFile()
                }
            }
            Text {
                visible: sheet.wrong
                text: qsTr("That password didn't open it. Try again.")
                font.pixelSize: 13
                color: look.dark ? "#FFB5A8" : "#B23A2A"
            }
            Item {
                width: parent.width
                height: 44
                Pill { raised: true; text: qsTr("Cancel"); onClicked: sheet.app.closeFile() }
                Pill { anchors.right: parent.right; primary: true; text: qsTr("Open"); onClicked: sheet.tryIt() }
            }
        }
    }
}
