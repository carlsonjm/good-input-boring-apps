import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// Find and replace: a bar floating over the top of the page. Every place the
// words appear is marked on the page; the arrows step through them, and the
// one reached is selected. Replace changes it and moves on.
Rectangle {
    id: bar

    property OfficeDocument document
    // Which of the found places is selected, from 1; 0 before stepping.
    property int at: 0
    readonly property int count: document ? document.searchCount : 0
    property alias needle: findField.text

    signal done()

    function start() {
        findField.forceActiveFocus()
        findField.selectAll()
        search()
    }
    function close() {
        if (document)
            document.clearSearch()
        at = 0
        done()
    }
    function search() {
        at = 0
        if (!document)
            return
        if (document.findAll(needle) > 0)
            step(false)
    }
    function step(backward) {
        if (count === 0)
            return
        document.findNext(needle, backward)
        at = backward ? (at <= 1 ? count : at - 1) : (at >= count ? 1 : at + 1)
    }

    height: 60
    width: row.implicitWidth + 2 * Theme.compact
    radius: height / 2
    color: Theme.chrome

    // A soft shadow so the bar reads as lifted off the page.
    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: -6
        radius: parent.radius
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.12)
    }

    component Field: TextField {
        implicitHeight: Theme.control
        leftPadding: Theme.comfortable
        rightPadding: Theme.comfortable
        color: Theme.text
        placeholderTextColor: Theme.secondary
        font.pixelSize: 15
        selectByMouse: true
        background: Rectangle { radius: height / 2; color: Theme.raised }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.compact

        Field {
            id: findField
            width: 280
            placeholderText: qsTr("Find")
            rightPadding: tally.width + Theme.comfortable
            onTextEdited: searchSoon.restart()
            onAccepted: bar.step(false)
            Keys.onEscapePressed: bar.close()
            Text {
                id: tally
                anchors.right: parent.right
                anchors.rightMargin: Theme.comfortable
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.secondary
                font.pixelSize: Theme.metaSize
                text: findField.text.length === 0 ? ""
                    : bar.count === 0 ? qsTr("None")
                    : qsTr("%1 of %2").arg(Math.max(1, bar.at)).arg(bar.count)
            }
        }
        IconPill { raised: true; glyph: "chevron-left"; enabled: bar.count > 0; Accessible.name: qsTr("Previous"); onClicked: bar.step(true) }
        IconPill { raised: true; glyph: "chevron-right"; enabled: bar.count > 0; Accessible.name: qsTr("Next"); onClicked: bar.step(false) }
        Field {
            id: replaceField
            width: 220
            placeholderText: qsTr("Replace with")
            Keys.onEscapePressed: bar.close()
        }
        Pill {
            raised: true
            label: qsTr("Replace")
            enabled: bar.count > 0 && bar.at > 0
            onClicked: {
                bar.document.replace(bar.needle, replaceField.text)
                bar.search()
            }
        }
        Pill {
            raised: true
            label: qsTr("Replace all")
            enabled: bar.count > 0
            onClicked: {
                bar.document.replaceAll(bar.needle, replaceField.text)
                bar.at = 0
            }
        }
        IconPill { glyph: "x"; Accessible.name: qsTr("Close"); onClicked: bar.close() }
    }

    // Looks again a moment after typing stops, not on every key.
    Timer { id: searchSoon; interval: 250; onTriggered: bar.search() }
}
