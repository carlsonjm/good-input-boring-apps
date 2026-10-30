import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import co.goodinput.office

// One window of Docs, Sheets or Slides. With no file it shows the recent list;
// with one, the paper and the quiet line above it.
ApplicationWindow {
    id: window

    // "text", "spreadsheet" or "presentation".
    property string kind: "text"
    property url startFile: ""
    readonly property bool editing: doc.status === OfficeDocument.Ready || doc.status === OfficeDocument.Loading
    readonly property string appName: kind === "spreadsheet" ? qsTr("Sheets") : kind === "presentation" ? qsTr("Slides") : qsTr("Docs")
    // Set when a blank was opened and still has to be kept under its real name.
    property url pendingTarget: ""

    width: 1280
    height: 800
    visible: true
    color: Theme.desk
    title: doc.status === OfficeDocument.Ready ? doc.title + " — " + appName : appName

    function open(url) {
        styleOpen = false
        doc.source = url
    }

    function newDocument() {
        pendingTarget = recent.newDocumentTarget()
        doc.source = recent.blankSource()
    }

    function closeDocument() {
        if (doc.modified)
            doc.save()
        doc.source = ""
        home.refresh()
    }

    property bool styleOpen: false
    property bool finding: false

    function startFinding() {
        styleOpen = false
        finding = true
        findBar.start()
    }

    // How long the writing is, from the engine's own count.
    readonly property string wordCount: {
        const raw = doc.states[".uno:StateWordCount"] || ""
        const m = raw.match(/([\d,.\s]+)\s+words?/)
        if (!m)
            return ""
        const words = Number(m[1].replace(/[^\d]/g, ""))
        const minutes = Math.max(1, Math.round(words / 200))
        return qsTr("%1 words · %2 min to read").arg(words.toLocaleString(Qt.locale(), "f", 0)).arg(minutes)
    }

    Shortcut {
        sequence: StandardKey.Find
        enabled: window.editing && window.kind === "text"
        onActivated: window.startFinding()
    }

    Recent { id: recent; kind: window.kind }

    OfficeDocument {
        id: doc
        onStatusChanged: {
            if (status !== OfficeDocument.Ready)
                return
            if (window.pendingTarget.toString().length) {
                const target = window.pendingTarget
                window.pendingTarget = ""
                doc.saveAs(target)
            }
            recent.add(doc.source)
            paper.focusPage()
        }
        onSourceChanged: if (status === OfficeDocument.Ready) recent.add(doc.source)
    }

    Component.onCompleted: if (startFile.toString().length) open(startFile)

    HomeView {
        id: home
        anchors.fill: parent
        visible: !window.editing && doc.status !== OfficeDocument.Error
        kind: window.kind
        newLabel: window.kind === "spreadsheet" ? qsTr("New spreadsheet") : window.kind === "presentation" ? qsTr("New presentation") : qsTr("New document")
        findLabel: window.kind === "spreadsheet" ? qsTr("Find a spreadsheet") : window.kind === "presentation" ? qsTr("Find a presentation") : qsTr("Find a document")
        onOpenRequested: (url) => window.open(url)
        onNewRequested: window.newDocument()
    }

    // A file that would not open says why, and offers the way back.
    Column {
        visible: doc.status === OfficeDocument.Error
        anchors.centerIn: parent
        spacing: Theme.comfortable
        Text { text: qsTr("This file couldn't be opened"); color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: doc.errorString; color: Theme.error; font.pixelSize: Theme.metaSize; width: 520; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter }
        Pill { raised: true; label: qsTr("Back to your files"); anchors.horizontalCenter: parent.horizontalCenter; onClicked: window.closeDocument() }
    }

    Item {
        id: editor
        anchors.fill: parent
        visible: window.editing

        TopLine {
            id: topLine
            anchors.left: parent.left
            anchors.right: parent.right
            document: doc
            styleOpen: window.styleOpen
            showStyle: window.kind === "text"
            onBack: window.closeDocument()
            onStyleRequested: window.styleOpen = !window.styleOpen
            onMoreRequested: (anchor) => moreMenu.popup(anchor, 0, anchor.height + Theme.tight)
            trailing: [
                Pill {
                    visible: window.kind === "presentation"
                    accent: true
                    label: qsTr("Present")
                    onClicked: presenter.start()
                }
            ]
        }

        // Sheets: what the chosen cell holds, shown only while one is chosen.
        Rectangle {
            id: formulaLine
            visible: window.kind === "spreadsheet" && doc.cellAddress.length > 0
            anchors.top: topLine.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 2 * Theme.surface, 900)
            height: visible ? Theme.control : 0
            radius: Theme.paperRadius
            color: Theme.chrome
            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.comfortable
                spacing: Theme.standard
                Text { text: doc.cellAddress; color: Theme.secondary; font.pixelSize: Theme.metaSize; anchors.verticalCenter: parent.verticalCenter }
                Text { text: doc.cellFormula; color: Theme.text; font.pixelSize: 15; font.family: "monospace"; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Paper {
            id: paper
            anchors.top: formulaLine.visible ? formulaLine.bottom : topLine.bottom
            anchors.topMargin: formulaLine.visible ? Theme.compact : 0
            anchors.left: slideStrip.visible ? slideStrip.right : parent.left
            anchors.right: parent.right
            anchors.bottom: partTabs.visible ? partTabs.top : parent.bottom
            document: doc
            fit: window.kind === "spreadsheet" ? "actual" : window.kind === "presentation" ? "page" : "width"
        }

        // Slides: every slide down the side, the current one ringed.
        ListView {
            id: slideStrip
            visible: window.kind === "presentation" && doc.parts > 0
            anchors.top: topLine.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: Theme.surface
            width: visible ? 196 : 0
            spacing: 14
            clip: true
            model: doc.parts
            footer: Pill { x: 22; y: 14; width: 160; raised: true; label: qsTr("New slide"); onClicked: doc.command(".uno:InsertPage") }
            delegate: AbstractButton {
                id: slideButton
                required property int index
                width: 196; height: 90
                onClicked: doc.setPart(index)
                contentItem: Item {
                    Text { text: slideButton.index + 1; color: Theme.secondary; font.pixelSize: 12; width: 14; horizontalAlignment: Text.AlignRight; y: 4 }
                    Rectangle {
                        id: thumb
                        x: 22; width: 160; height: 90
                        radius: Theme.paperRadius
                        color: "white"
                        clip: true
                        PageView {
                            anchors.fill: parent
                            document: doc
                            part: slideButton.index
                            zoom: doc.size.width > 0 ? Math.min(160 / doc.size.width, 90 / doc.size.height) : 0.01
                            enabled: false
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.paperRadius
                            color: "transparent"
                            border.width: doc.currentPart === slideButton.index ? 3 : 0
                            border.color: Theme.accent
                        }
                    }
                }
            }
        }

        // Sheets: the sheets of the file as pills along the bottom.
        Row {
            id: partTabs
            visible: window.kind === "spreadsheet" && doc.parts > 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.comfortable
            anchors.left: parent.left
            anchors.leftMargin: Theme.surface
            height: visible ? Theme.control : 0
            spacing: 6
            Repeater {
                model: doc.partNames
                Pill {
                    required property string modelData
                    required property int index
                    label: modelData
                    selected: doc.currentPart === index
                    onClicked: doc.setPart(index)
                }
            }
            IconPill {
                glyph: "plus"
                Accessible.name: qsTr("New sheet")
                onClicked: doc.command(".uno:Add")
            }
        }

        SelectionBar {
            id: selectionBar
            visible: window.kind === "text" && doc.hasSelection && !window.styleOpen && !window.finding
            document: doc
            view: paper.view
            readonly property rect first: doc.selectionRects.length ? paper.viewRect(doc.selectionRects[0]) : Qt.rect(0, 0, 0, 0)
            x: Math.max(Theme.comfortable, Math.min(parent.width - width - Theme.comfortable, paper.x + first.x + first.width / 2 - width / 2))
            y: {
                const above = paper.y + first.y - height - 56
                return above > topLine.height ? above : paper.y + first.y + first.height + 56
            }
            onDone: paper.focusPage()
        }

        StyleSheet {
            visible: window.styleOpen && window.kind === "text"
            anchors.right: parent.right
            anchors.rightMargin: Theme.comfortable
            y: topLine.height
            document: doc
            onDone: { window.styleOpen = false; paper.focusPage() }
        }

        FindBar {
            id: findBar
            objectName: "findBar"
            visible: window.finding
            anchors.horizontalCenter: parent.horizontalCenter
            y: topLine.height + Theme.compact
            document: doc
            onDone: { window.finding = false; paper.focusPage() }
        }

        Text {
            visible: window.kind === "text" && doc.parts > 1
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 20
            color: Theme.secondary
            font.pixelSize: Theme.metaSize
            text: qsTr("%1 pages").arg(doc.parts)
        }
    }

    Menu {
        id: moreMenu
        objectName: "moreMenu"
        x: window.width - width - Theme.comfortable
        y: topLine.height
        background: Rectangle { implicitWidth: 260; color: Theme.chrome; radius: Theme.noteRadius }
        delegate: MenuItem {
            id: menuItem
            implicitHeight: Theme.control
            contentItem: Text { text: menuItem.text; color: Theme.text; font.pixelSize: 15; leftPadding: Theme.standard; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { radius: Theme.paperRadius; color: menuItem.highlighted ? Theme.hover : "transparent" }
        }
        Action {
            text: qsTr("Find and replace")
            enabled: window.kind === "text"
            onTriggered: window.startFinding()
        }
        Action {
            text: qsTr("Print")
            onTriggered: {
                // The desktop's PDF app does the printing.
                const pdf = doc.printCopy()
                if (pdf.toString().length)
                    Qt.openUrlExternally(pdf)
            }
        }
        Action {
            text: qsTr("Save a PDF copy")
            onTriggered: {
                const path = doc.source.toString().replace(/\.[^./]+$/, "") + ".pdf"
                doc.saveCopy(path, "pdf")
            }
        }
        Action { text: qsTr("Open in LibreOffice"); onTriggered: Qt.openUrlExternally(doc.source) }
        Action { text: qsTr("Close"); onTriggered: window.closeDocument() }
        MenuSeparator {
            visible: window.kind === "text" && window.wordCount.length > 0
            height: visible ? implicitHeight : 0
            contentItem: Rectangle { implicitHeight: 1; color: Theme.divider }
        }
        // Word count: read only, at the foot of the menu.
        MenuItem {
            visible: window.kind === "text" && window.wordCount.length > 0
            height: visible ? implicitHeight : 0
            enabled: false
            contentItem: Text {
                text: window.wordCount + "\n" + (doc.parts === 1 ? qsTr("1 page") : qsTr("%1 pages").arg(doc.parts))
                color: Theme.secondary
                font.pixelSize: Theme.metaSize
                lineHeight: 1.3
                leftPadding: Theme.standard
                topPadding: Theme.compact
                bottomPadding: Theme.compact
            }
            background: null
        }
    }

    // Slides: the deck full screen, one slide at a time.
    Window {
        id: presenter
        color: "black"
        visibility: Window.Hidden
        function start() { visibility = Window.FullScreen; requestActivate(); stage.forceActiveFocus() }
        function stop() { visibility = Window.Hidden }
        Paper {
            id: stage
            anchors.fill: parent
            document: presenter.visibility !== Window.Hidden ? doc : null
            fit: "screen"
            interactive: false
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Escape) presenter.stop()
                else if (event.key === Qt.Key_Right || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) doc.setPart(Math.min(doc.parts - 1, doc.currentPart + 1))
                else if (event.key === Qt.Key_Left || event.key === Qt.Key_PageUp) doc.setPart(Math.max(0, doc.currentPart - 1))
            }
            TapHandler {
                onTapped: (point) => {
                    if (point.position.x < stage.width / 3) doc.setPart(Math.max(0, doc.currentPart - 1))
                    else if (doc.currentPart + 1 < doc.parts) doc.setPart(doc.currentPart + 1)
                    else presenter.stop()
                }
            }
        }
    }
}
