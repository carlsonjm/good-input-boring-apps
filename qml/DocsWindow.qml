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
        onTemplateRequested: (name, title) => {
            window.pendingTarget = recent.newDocumentTarget(title)
            doc.source = recent.templateSource(name)
        }
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
            showStyle: window.kind === "text" || window.kind === "spreadsheet"
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

        // Sheets: which cells are chosen and what the first one holds.
        Rectangle {
            id: formulaLine
            visible: window.kind === "spreadsheet"
            anchors.top: topLine.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.surface
            anchors.rightMargin: Theme.surface
            height: visible ? Theme.control : 0
            radius: Theme.paperRadius
            color: Theme.chrome
            Row {
                anchors.fill: parent
                anchors.leftMargin: 6
                spacing: Theme.standard
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: 32
                    width: Math.max(48, address.implicitWidth + 24)
                    radius: 16
                    color: Theme.raised
                    Text { id: address; anchors.centerIn: parent; text: doc.cellAddress; color: Theme.secondary; font.pixelSize: Theme.metaSize }
                }
                Text { text: "fx"; color: Theme.secondary; font.pixelSize: 14; font.italic: true; anchors.verticalCenter: parent.verticalCenter }
                Text { text: doc.cellFormula; color: Theme.text; font.pixelSize: 15; font.family: "monospace"; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        // Sheets: the grid as one sheet of paper, with its letters and numbers.
        Item {
            id: sheetFrame
            visible: window.kind === "spreadsheet"
            anchors.top: formulaLine.bottom
            anchors.topMargin: Theme.compact
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.surface
            anchors.rightMargin: Theme.surface
            anchors.bottom: partTabs.top
            anchors.bottomMargin: Theme.standard
            Rectangle { anchors.fill: parent; color: "white" }
            SheetHeaders {
                id: sheetHeaders
                anchors.fill: parent
                document: window.kind === "spreadsheet" ? doc : null
                paper: paper
            }
        }

        Paper {
            id: paper
            anchors.top: sheetFrame.visible ? sheetFrame.top : topLine.bottom
            anchors.topMargin: sheetFrame.visible ? sheetHeaders.band : 0
            anchors.left: sheetFrame.visible ? sheetFrame.left : slideStrip.visible ? slideStrip.right : parent.left
            anchors.leftMargin: sheetFrame.visible ? sheetHeaders.gutter : 0
            anchors.right: sheetFrame.visible ? sheetFrame.right : parent.right
            anchors.bottom: sheetFrame.visible ? sheetFrame.bottom : parent.bottom
            document: doc
            fit: window.kind === "spreadsheet" ? "actual" : window.kind === "presentation" ? "page" : "width"
        }

        Corners {
            visible: sheetFrame.visible
            anchors.fill: sheetFrame
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
            // Only Slides draws its strip; elsewhere the parts are sheets or pages.
            model: window.kind === "presentation" ? doc.parts : 0
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

        // Sheets: the sheets of the file as pills along the bottom, and what
        // the chosen cells add up to.
        Row {
            id: partTabs
            visible: window.kind === "spreadsheet"
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

        Rectangle {
            id: tally
            // "Average: 4; Sum: 12" from the engine, read as Sum, Average, Count.
            readonly property var parts: {
                const out = {}
                const raw = doc.states[".uno:StateTableCell"] || ""
                for (const piece of raw.split(";")) {
                    const kv = piece.split(":")
                    if (kv.length === 2)
                        out[kv[0].trim()] = kv[1].trim()
                }
                return out
            }
            readonly property string count: {
                const m = (doc.states[".uno:RowColSelCount"] || "").match(/(\d+) rows?, (\d+) columns?/)
                return m ? String(Number(m[1]) * Number(m[2])) : ""
            }
            readonly property bool block: doc.cellAddress.indexOf(":") >= 0
            readonly property string text: {
                const bits = []
                if (parts["Sum"] !== undefined)
                    bits.push(qsTr("Sum") + "  <b>" + parts["Sum"] + "</b>")
                if (block && parts["Average"] !== undefined)
                    bits.push(qsTr("Average") + "  <b>" + parts["Average"] + "</b>")
                if (block && count.length)
                    bits.push(qsTr("Count") + "  <b>" + count + "</b>")
                return bits.join("&nbsp;&nbsp;&nbsp;&nbsp;")
            }
            visible: window.kind === "spreadsheet" && text.length > 0
            anchors.right: parent.right
            anchors.rightMargin: Theme.surface
            anchors.verticalCenter: partTabs.verticalCenter
            height: 36
            width: tallyText.implicitWidth + 32
            radius: height / 2
            color: Theme.chrome
            Text {
                id: tallyText
                anchors.centerIn: parent
                textFormat: Text.StyledText
                text: tally.text
                color: Theme.secondary
                font.pixelSize: Theme.metaSize
            }
        }

        SheetBar {
            id: sheetBar
            visible: window.kind === "spreadsheet" && tally.block && !window.styleOpen
            document: doc
            view: paper.view
            sum: tally.parts["Sum"] !== undefined ? qsTr("Sum %1").arg(tally.parts["Sum"]) : ""
            readonly property rect first: doc.selectionRects.length ? paper.viewRect(doc.selectionRects[0]) : Qt.rect(0, 0, 0, 0)
            x: Math.max(Theme.comfortable, Math.min(parent.width - width - Theme.comfortable, paper.x + first.x + first.width / 2 - width / 2))
            y: {
                const below = paper.y + first.y + first.height + Theme.standard
                return below + height < partTabs.y - Theme.compact ? below : Math.max(paper.y + Theme.compact, paper.y + first.y - height - Theme.standard)
            }
            onDone: paper.focusPage()
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

        CellSheet {
            visible: window.styleOpen && window.kind === "spreadsheet"
            anchors.right: parent.right
            anchors.rightMargin: Theme.comfortable
            y: topLine.height
            document: doc
            onDone: { window.styleOpen = false; paper.focusPage() }
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
