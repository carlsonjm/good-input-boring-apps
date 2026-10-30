import QtQuick
import co.goodinput.office

// The column letters across the top of a sheet and the row numbers down its
// side, in a soft band, lined up with the engine's grid as it scrolls. The
// letters and numbers of the chosen cells are drawn in the accent.
Item {
    id: headers

    property OfficeDocument document
    property Paper paper
    readonly property int band: 28
    readonly property int gutter: 44

    property var columns: []
    property var rows: []
    // The chosen cells, from the engine's address ("B2" or "B2:D9"), as
    // zero-based first and last column and row.
    readonly property var chosen: parse(document ? document.cellAddress : "")

    function letters(text) {
        let n = 0
        for (let i = 0; i < text.length; ++i)
            n = n * 26 + (text.charCodeAt(i) - 64)
        return n - 1
    }
    function parse(address) {
        const m = address.replace(/\$/g, "").match(/^([A-Z]+)(\d+)(?::([A-Z]+)(\d+))?$/)
        if (!m)
            return null
        const c0 = letters(m[1]), r0 = Number(m[2]) - 1
        const c1 = m[3] ? letters(m[3]) : c0, r1 = m[4] ? Number(m[4]) - 1 : r0
        return { c0: Math.min(c0, c1), c1: Math.max(c0, c1), r0: Math.min(r0, r1), r1: Math.max(r0, r1) }
    }

    // Asks the engine which columns and rows are in sight and where they end.
    function refresh() {
        if (!document || document.status !== OfficeDocument.Ready || document.kind !== OfficeDocument.Spreadsheet || !paper)
            return
        const z = paper.view.zoom
        const x = Math.max(0, Math.round(paper.scrollX / z))
        const y = Math.max(0, Math.round(paper.scrollY / z))
        const w = Math.round(paper.width / z)
        const h = Math.round(paper.height / z)
        const raw = document.commandValues(".uno:ViewRowColumnHeaders?x=" + x + "&y=" + y + "&width=" + w + "&height=" + h)
        let data
        try { data = JSON.parse(raw) } catch (e) { return }
        columns = spans(data.columns || [], paper.scrollX)
        rows = spans(data.rows || [], paper.scrollY)
    }
    // The engine gives where each one ends, in pixels at full size; the first
    // entry is where the first one starts.
    function spans(list, scroll) {
        const out = []
        const scale = 15 * paper.view.zoom
        for (let i = 1; i < list.length; ++i) {
            const from = Number(list[i - 1].size) * scale - scroll
            const to = Number(list[i].size) * scale - scroll
            const label = String(list[i].text)
            out.push({ text: label, from: from, size: to - from, index: label.match(/^\d+$/) ? Number(label) - 1 : letters(label) })
        }
        return out
    }

    Timer { id: soon; interval: 40; onTriggered: headers.refresh() }
    Connections {
        target: headers.paper
        function onScrollXChanged() { soon.restart() }
        function onScrollYChanged() { soon.restart() }
        function onWidthChanged() { soon.restart() }
        function onHeightChanged() { soon.restart() }
    }
    Connections {
        target: headers.document
        function onStatusChanged() { soon.restart() }
        function onPartsChanged() { soon.restart() }
        function onSizeChanged() { soon.restart() }
    }

    readonly property color bandColour: "#F3F3F6"
    readonly property color lineColour: "#E2E2E8"
    readonly property color quiet: "#8A8A93"
    readonly property color chosenInk: Qt.darker(Theme.accent, 1.6)

    // Across the top.
    Rectangle {
        x: headers.gutter
        width: parent.width - headers.gutter
        height: headers.band
        color: headers.bandColour
        clip: true
        Repeater {
            model: headers.columns
            Rectangle {
                required property var modelData
                readonly property bool on: headers.chosen !== null && modelData.index >= headers.chosen.c0 && modelData.index <= headers.chosen.c1
                x: modelData.from
                width: modelData.size
                height: headers.band
                color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : "transparent"
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: headers.lineColour }
                Text {
                    anchors.centerIn: parent
                    text: modelData.text
                    font.pixelSize: 12
                    font.weight: parent.on ? Font.DemiBold : Font.Normal
                    color: parent.on ? headers.chosenInk : headers.quiet
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: headers.lineColour }
    }

    // Down the side.
    Rectangle {
        y: headers.band
        width: headers.gutter
        height: parent.height - headers.band
        color: headers.bandColour
        clip: true
        Repeater {
            model: headers.rows
            Rectangle {
                required property var modelData
                readonly property bool on: headers.chosen !== null && modelData.index >= headers.chosen.r0 && modelData.index <= headers.chosen.r1
                y: modelData.from
                height: modelData.size
                width: headers.gutter
                color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : "transparent"
                Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: headers.lineColour }
                Text {
                    anchors.centerIn: parent
                    text: modelData.text
                    font.pixelSize: 12
                    font.weight: parent.on ? Font.DemiBold : Font.Normal
                    color: parent.on ? headers.chosenInk : headers.quiet
                }
            }
        }
        Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: headers.lineColour }
    }

    // The corner where the two bands meet.
    Rectangle { width: headers.gutter; height: headers.band; color: headers.bandColour
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: headers.lineColour }
        Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: headers.lineColour }
    }
}
