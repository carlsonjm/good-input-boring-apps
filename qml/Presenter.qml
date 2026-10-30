import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import co.goodinput.office

// Presenting. With a second screen, the room sees the slide full screen
// there, and this screen shows the presenter's view: the time, the slide,
// the next one and the controls. With one screen, the slide fills it and the
// controls show for a moment whenever it is touched.
Item {
    id: show

    property OfficeDocument document
    readonly property bool running: audience.visible
    readonly property bool twoScreens: Qt.application.screens.length > 1
    property date startedAt: new Date()
    property int elapsed: 0

    function start() {
        startedAt = new Date()
        elapsed = 0
        if (twoScreens) {
            audience.screen = Qt.application.screens[1]
            presenter.screen = Qt.application.screens[0]
            presenter.showFullScreen()
            presenter.requestActivate()
        }
        audience.showFullScreen()
        if (!twoScreens)
            audience.requestActivate()
        controls.wake()
    }
    function stop() {
        audience.hide()
        presenter.hide()
    }
    function next() {
        if (document.currentPart + 1 < document.parts)
            document.setPart(document.currentPart + 1)
        else if (!twoScreens)
            stop()
        controls.wake()
    }
    function back() {
        document.setPart(Math.max(0, document.currentPart - 1))
        controls.wake()
    }
    function keys(event) {
        if (event.key === Qt.Key_Escape)
            stop()
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown || event.key === Qt.Key_Down)
            next()
        else if (event.key === Qt.Key_Left || event.key === Qt.Key_PageUp || event.key === Qt.Key_Up)
            back()
        else
            return
        event.accepted = true
    }

    Timer {
        running: show.running
        interval: 1000
        repeat: true
        onTriggered: show.elapsed = Math.floor((new Date() - show.startedAt) / 1000)
    }

    function clock(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // ‹  2 of 6  ›, as one pill.
    component Controls: Rectangle {
        id: pill
        height: 64
        width: row.implicitWidth + 16
        radius: height / 2
        color: "#1C1C1C"
        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.compact
            AbstractButton {
                width: 56; height: 56
                Accessible.name: qsTr("Back")
                onClicked: show.back()
                background: Rectangle { radius: 28; color: "#242424" }
                contentItem: Item { Image { anchors.centerIn: parent; width: 24; height: 24; source: Qt.resolvedUrl("icons/chevron-left.svg"); sourceSize: Qt.size(48, 48) } }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: show.document ? qsTr("%1 of %2").arg(show.document.currentPart + 1).arg(show.document.parts) : ""
                color: "#F8F8FF"
                font.pixelSize: 16
                leftPadding: Theme.comfortable
                rightPadding: Theme.comfortable
            }
            AbstractButton {
                width: 56; height: 56
                Accessible.name: qsTr("Next")
                onClicked: show.next()
                background: Rectangle { radius: 28; color: Theme.accent }
                contentItem: Item { Image { anchors.centerIn: parent; width: 24; height: 24; source: Qt.resolvedUrl("icons/ink/chevron-right.svg"); sourceSize: Qt.size(48, 48) } }
            }
        }
    }

    // What the room sees.
    Window {
        id: audience
        color: "black"
        title: show.document ? show.document.title : ""
        Paper {
            id: stage
            anchors.fill: parent
            document: audience.visible ? show.document : null
            fit: "screen"
            ground: "black"
            interactive: false
            focus: true
            Keys.onPressed: (event) => show.keys(event)
            TapHandler {
                onTapped: (point) => {
                    if (point.position.x < stage.width / 3)
                        show.back()
                    else
                        show.next()
                }
            }
        }
        Controls {
            id: controls
            visible: !show.twoScreens
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            opacity: 0
            function wake() { opacity = 1; fade.restart() }
            Behavior on opacity { NumberAnimation { duration: Theme.stateTime } }
            Timer { id: fade; interval: 3000; onTriggered: controls.opacity = 0 }
        }
        onVisibleChanged: if (visible) stage.forceActiveFocus()
    }

    // What the presenter sees, when there is a second screen.
    Window {
        id: presenter
        color: "#0B0B0B"
        title: qsTr("Presenting")
        Item {
            anchors.fill: parent
            anchors.margins: 32
            focus: true
            Keys.onPressed: (event) => show.keys(event)

            Row {
                id: time
                spacing: 14
                Text { text: show.clock(show.elapsed); color: "#F8F8FF"; font.pixelSize: 40; font.weight: Font.DemiBold }
                Text {
                    anchors.baseline: parent.children[0].baseline
                    text: qsTr("Started %1").arg(show.startedAt.toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
                    color: Qt.rgba(1, 1, 1, 0.66)
                    font.pixelSize: 14
                }
            }
            Pill {
                anchors.right: parent.right
                raised: true
                label: qsTr("End show")
                onClicked: show.stop()
            }

            Paper {
                id: current
                anchors.top: time.bottom
                anchors.topMargin: 24
                width: parent.width * 0.62
                height: width * 9 / 16 + 2 * Theme.surface
                document: presenter.visible ? show.document : null
                fit: "page"
                ground: "#0B0B0B"
                interactive: false
            }
            Rectangle {
                anchors.left: current.left
                anchors.leftMargin: Theme.surface
                anchors.right: current.right
                anchors.rightMargin: Theme.surface
                anchors.top: current.bottom
                height: 4
                radius: 2
                color: "#242424"
                Rectangle {
                    width: show.document && show.document.parts > 0 ? parent.width * (show.document.currentPart + 1) / show.document.parts : 0
                    height: 4
                    radius: 2
                    color: Theme.accent
                }
            }

            Column {
                anchors.left: current.right
                anchors.leftMargin: Theme.surface
                anchors.right: parent.right
                anchors.top: current.top
                anchors.topMargin: Theme.surface
                spacing: 10
                Text { text: qsTr("Next"); color: Qt.rgba(1, 1, 1, 0.66); font.pixelSize: 13 }
                Rectangle {
                    id: nextSlide
                    readonly property int part: show.document ? show.document.currentPart + 1 : 0
                    width: parent.width
                    height: show.document && show.document.size.width > 0 ? width * show.document.size.height / show.document.size.width : width * 9 / 16
                    radius: Theme.paperRadius
                    color: "#1C1C1C"
                    clip: true
                    PageView {
                        visible: show.document && nextSlide.part < show.document.parts
                        anchors.fill: parent
                        document: presenter.visible ? show.document : null
                        part: nextSlide.part
                        zoom: show.document && show.document.size.width > 0 ? nextSlide.width / show.document.size.width : 0.01
                        enabled: false
                    }
                    Text {
                        visible: !show.document || nextSlide.part >= show.document.parts
                        anchors.centerIn: parent
                        text: qsTr("The end")
                        color: Qt.rgba(1, 1, 1, 0.66)
                        font.pixelSize: 15
                    }
                    Corners { anchors.fill: parent; ground: "#0B0B0B" }
                }
            }

            Controls {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
            }
        }
    }
}
