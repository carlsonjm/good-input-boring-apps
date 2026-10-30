import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// The page on the desk. The engine draws the paper; this draws everything a
// finger works with on it, in the suite's own shapes: the cursor, the
// selection, its two handles. A tap places the cursor, a double tap or a long
// press takes a word, a triple tap a sentence; the handles stretch it.
Item {
    id: paper

    property OfficeDocument document
    // How the page meets the window: "width" fits the page's width (Docs),
    // "page" the whole page (Slides), "screen" edge to edge (presenting), and
    // "actual" draws it near its own size (Sheets).
    property string fit: "width"
    // Off for a page that is only shown, such as a slide being presented.
    property bool interactive: true
    // Widest the page is drawn, in pixels, whatever the window.
    property real maxPageWidth: 860
    // What the paper lies on, for rounding its corners into it.
    property color ground: Theme.desk
    readonly property alias view: view
    // Where the window onto the document is, for drawing beside it.
    readonly property alias scrollX: flick.contentX
    readonly property alias scrollY: flick.contentY
    readonly property bool selecting: interactive && document && document.hasSelection

    readonly property real zoom: {
        if (!document || document.size.width <= 0)
            return 0.05
        const w = document.size.width
        const h = document.size.height
        const room = 2 * Theme.surface
        switch (fit) {
        case "page": return Math.min((width - room) / w, (height - room) / h)
        case "screen": return Math.min(width / w, height / h)
        // A sheet a little larger than the engine's own size, for fingers.
        case "actual": return 1.4 / 15
        default: return Math.min(width - room, maxPageWidth) / w
        }
    }
    // A spreadsheet's grid starts at the edge; its frame and headers are
    // drawn around it by the window.
    readonly property real sideX: fit === "actual" ? 0 : Math.max(0, (width - view.contentWidth) / 2)
    readonly property real topY: fit === "screen" ? Math.max(0, (height - view.contentHeight) / 2)
        : fit === "page" ? Math.max(Theme.surface, (height - view.contentHeight) / 2)
        : fit === "width" ? Theme.surface : 0

    function viewRect(r) {
        // Reads contentX and contentY so the shapes follow the page as it scrolls.
        return Qt.rect(r.x * view.zoom - view.contentX, r.y * view.zoom - view.contentY,
                       r.width * view.zoom, r.height * view.zoom)
    }

    function focusPage() {
        view.forceActiveFocus()
        Qt.inputMethod.show()
    }

    // Keeps the cursor in sight as typing moves it.
    function revealCursor() {
        if (!document || !document.cursorVisible || flick.moving)
            return
        const r = document.cursorRect
        const top = r.y * view.zoom + topY
        const bottom = top + r.height * view.zoom
        const margin = 48
        if (top < flick.contentY + margin)
            flick.contentY = Math.max(0, top - margin)
        else if (bottom > flick.contentY + flick.height - margin)
            flick.contentY = Math.min(flick.contentHeight - flick.height, bottom - flick.height + margin)
    }

    Connections {
        target: paper.document
        function onCursorChanged() { paper.revealCursor() }
        function onSourceChanged() { flick.contentY = 0 }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: paper.fit === "actual" ? Math.max(width, view.contentWidth) : Math.max(width, view.contentWidth + 2 * Theme.surface)
        contentHeight: paper.fit === "actual" ? Math.max(height, view.contentHeight) : view.contentHeight + paper.topY + Theme.surface
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    }

    PageView {
        id: view
        anchors.fill: parent
        document: paper.document
        zoom: paper.zoom
        contentX: flick.contentX - paper.sideX
        contentY: flick.contentY - paper.topY
        focus: paper.interactive
        enabled: paper.interactive

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.DragThreshold
            onTapped: (point) => {
                const p = view.toDocument(point.position.x, point.position.y)
                const count = Math.min(3, tapCount)
                paper.document.tap(p.x, p.y, count)
                paper.focusPage()
            }
            onLongPressed: {
                const p = view.toDocument(point.position.x, point.position.y)
                paper.document.tap(p.x, p.y, 2)
                paper.focusPage()
            }
        }

        // With a mouse, dragging selects instead of scrolling.
        DragHandler {
            acceptedDevices: PointerDevice.Mouse
            target: null
            property point last
            onActiveChanged: {
                const p = view.toDocument(centroid.position.x, centroid.position.y)
                if (active)
                    paper.document.press(p.x, p.y)
                else
                    paper.document.release(last.x, last.y)
            }
            onCentroidChanged: {
                if (!active)
                    return
                last = view.toDocument(centroid.position.x, centroid.position.y)
                paper.document.move(last.x, last.y)
            }
        }

        WheelHandler {
            target: null
            onWheel: (event) => {
                flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height,
                                                     flick.contentY - event.angleDelta.y))
            }
        }
    }

    // Each sheet of paper gets the paper tier's round corners, as in PDF: the
    // desk's colour is laid over the four corners outside the curve. Only the
    // pages in sight are rounded.
    Item {
        anchors.fill: parent
        clip: true
        visible: paper.fit !== "actual"
        Repeater {
            model: {
                const doc = paper.document
                if (!doc || doc.size.width <= 0)
                    return []
                const all = doc.kind === OfficeDocument.Text ? doc.pages : [Qt.rect(0, 0, doc.size.width, doc.size.height)]
                const top = view.contentY / view.zoom
                const bottom = (view.contentY + paper.height) / view.zoom
                return all.filter(r => r.y + r.height >= top && r.y <= bottom)
            }
            delegate: Item {
                required property var modelData
                readonly property rect at: paper.viewRect(modelData)
                // The engine's paper reaches a pixel past its rectangle's edge.
                x: Math.floor(at.x); y: Math.floor(at.y)
                width: Math.ceil(at.x + at.width) + 1 - x
                height: Math.ceil(at.y + at.height) + 1 - y
                // Full screen, a slide keeps square corners.
                Corners { visible: paper.fit !== "screen"; anchors.fill: parent; ground: paper.ground }
                // Trims the engine's own thin outline round a slide.
                Rectangle {
                    visible: paper.document && paper.document.kind === OfficeDocument.Presentation
                    anchors.fill: parent
                    radius: paper.fit === "screen" ? 0 : Theme.paperRadius
                    color: "transparent"
                    border.width: 2
                    border.color: paper.ground
                }
            }
        }
    }

    // Every place a search found, marked like a highlighter.
    Repeater {
        model: paper.interactive && paper.document ? paper.document.searchRects : []
        delegate: Rectangle {
            required property var modelData
            readonly property rect at: paper.viewRect(modelData)
            x: at.x; y: at.y; width: at.width; height: at.height
            radius: 2
            color: "#F2B134"
            opacity: 0.45
        }
    }

    // The selection, drawn by us so it can follow the suite's look.
    Repeater {
        model: paper.interactive && paper.document ? paper.document.selectionRects : []
        delegate: Rectangle {
            required property var modelData
            readonly property rect at: paper.viewRect(modelData)
            x: at.x; y: at.y; width: at.width; height: at.height
            color: Theme.accent
            opacity: 0.28
        }
    }

    // Sheets: the chosen cell, ringed in the accent with a pearl at its corner.
    Rectangle {
        readonly property rect at: paper.document && paper.interactive ? paper.viewRect(paper.document.cellCursor) : Qt.rect(0, 0, 0, 0)
        visible: at.width > 0
        x: at.x - 1; y: at.y - 1; width: at.width + 2; height: at.height + 2
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        Rectangle {
            width: 12; height: 12; radius: 6
            color: Theme.accent
            x: parent.width - 6; y: parent.height - 6
        }
    }

    // Slides: the chosen box, framed in the accent with a pearl at each
    // corner and side.
    Item {
        id: box
        readonly property rect at: paper.interactive && paper.document && paper.document.hasObject
            ? paper.viewRect(paper.document.objectRect) : Qt.rect(0, 0, 0, 0)
        visible: at.width > 0
        x: at.x; y: at.y; width: at.width; height: at.height
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
        Repeater {
            model: [[0, 0], [0.5, 0], [1, 0], [0, 0.5], [1, 0.5], [0, 1], [0.5, 1], [1, 1]]
            Rectangle {
                required property var modelData
                width: 14; height: 14; radius: 7
                x: box.width * modelData[0] - 7
                y: box.height * modelData[1] - 7
                color: Theme.accent
                border.width: 2
                border.color: "white"
            }
        }
    }

    // The cursor: a 2 px accent line that blinks only while nothing moves.
    Rectangle {
        id: caret
        readonly property rect at: paper.document ? paper.viewRect(paper.document.cursorRect) : Qt.rect(0, 0, 0, 0)
        visible: paper.interactive && paper.document && paper.document.cursorVisible && !paper.selecting && view.activeFocus
        x: at.x; y: at.y
        width: 2; height: Math.max(12, at.height)
        color: Theme.accent
        SequentialAnimation on opacity {
            running: caret.visible
            loops: Animation.Infinite
            PropertyAction { value: 1 }
            PauseAnimation { duration: 600 }
            PropertyAction { value: 0 }
            PauseAnimation { duration: 400 }
        }
    }

    component Handle: Item {
        id: handle
        property bool end: false
        readonly property rect at: paper.selecting
            ? paper.viewRect(end ? paper.document.selectionEnd : paper.document.selectionStart) : Qt.rect(0, 0, 0, 0)
        visible: paper.selecting && at.height > 0
        // A 44 px target around a 12 px pearl: the start's above the line, the end's below.
        width: Theme.control; height: Theme.control
        x: (end ? at.x + at.width : at.x) - width / 2
        y: end ? at.y + at.height - 6 : at.y - height + 6
        Rectangle {
            width: 2; height: handle.at.height
            color: Theme.accent
            x: parent.width / 2 - 1
            y: handle.end ? 6 - height : parent.height - 6
        }
        Rectangle {
            width: 12; height: 12; radius: 6
            color: Theme.accent
            anchors.horizontalCenter: parent.horizontalCenter
            y: handle.end ? 6 : parent.height - 18
        }
        DragHandler {
            target: null
            onCentroidChanged: {
                if (!active)
                    return
                // The finger holds the pearl; the line it means is just past it.
                const p = handle.mapToItem(view, centroid.position.x, centroid.position.y)
                const reach = handle.at.height / 2 + 12
                const d = view.toDocument(p.x, p.y + (handle.end ? -reach : reach))
                paper.document.moveSelectionEnd(handle.end, d.x, d.y)
            }
        }
    }

    Handle { end: false }
    Handle { end: true }
}
