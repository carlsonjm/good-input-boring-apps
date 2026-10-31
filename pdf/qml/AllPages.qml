import QtQuick

// Every page at a glance, with a search across the file above them. While
// searching, each page says how many matches it has and pages without one
// step back. A tap goes to that page, at its first match.
Rectangle {
    id: sheet

    required property Item app
    property alias query: field.text
    property var counts: ({})

    color: look.desk
    function focusSearch() { field.forceActiveFocus(); field.selectAll(); }
    function refresh() {
        const c = {};
        for (const p of pdf.pagesWithHits())
            c[p.page] = p.count;
        counts = c;
    }
    Connections { target: pdf; function onSearchHitsChanged() { sheet.refresh(); } }

    Row {
        id: top
        anchors.top: parent.top
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        Rectangle {
            width: Math.min(420, sheet.width - 120)
            height: 44
            radius: 22
            color: look.raised
            border.width: 2
            border.color: field.activeFocus ? look.strongBorder : "transparent"
            Image {
                id: lens
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 18; height: 18
                sourceSize: Qt.size(36, 36)
                source: "image://glyph/search/" + look.softInk
            }
            TextInput {
                id: field
                anchors.left: lens.right
                anchors.leftMargin: 8
                anchors.right: found.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: look.ink
                font.pixelSize: 15
                clip: true
                selectByMouse: true
                selectionColor: look.fill(0.24)
                Accessible.name: qsTr("Search this PDF")
                onTextChanged: searchSoon.restart()
                Keys.onReturnPressed: { pdf.search(text); sheet.goToFirstHit(); }
                Keys.onEscapePressed: sheet.app.closePages()
                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: !parent.text
                    text: qsTr("Search this PDF")
                    color: look.softInk
                    font.pixelSize: 15
                }
            }
            Timer { id: searchSoon; interval: 250; onTriggered: pdf.search(field.text) }
            Text {
                id: found
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 13
                color: look.softInk
                text: {
                    if (!field.text.trim())
                        return "";
                    const pages = Object.keys(sheet.counts).length;
                    return pdf.searchHits.length === 0 ? qsTr("None")
                         : pages === 1 ? qsTr("%1 on 1 page").arg(pdf.searchHits.length)
                         : qsTr("%1 on %2 pages").arg(pdf.searchHits.length).arg(pages);
                }
            }
        }
        IconButton { glyph: "x"; label: qsTr("Close all pages"); onClicked: sheet.app.closePages() }
    }

    function goToFirstHit() {
        if (pdf.searchHits.length > 0)
            app.closePages(pdf.searchHits[0].page, pdf.searchHits[0]);
    }

    GridView {
        id: grid
        anchors.top: top.bottom
        anchors.topMargin: 24
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        readonly property int columns: Math.max(2, Math.min(6, Math.floor((sheet.width - 48) / 174)))
        width: Math.min(columns, Math.max(1, count)) * 174
        topMargin: 8
        cellWidth: 174
        cellHeight: 194 + 8 + 20 + 20
        clip: true
        model: pdf.status === "open" ? pdf.pageCount : 0
        bottomMargin: 24
        delegate: Item {
            id: cell
            required property int index
            readonly property int hits: sheet.counts[index] || 0
            readonly property bool searching: field.text.trim().length > 0
            readonly property bool current: index === pdf.page
            width: 174
            height: grid.cellHeight
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Page %1").arg(index + 1)

            Item {
                id: thumb
                x: 12
                width: 150
                height: Math.min(194, 150 * pdf.pageAspect(cell.index))
                opacity: cell.searching && cell.hits === 0 ? 0.55 : 1
                Rectangle { // the current page's ring, 4 px out
                    anchors.fill: parent
                    anchors.margins: -7
                    radius: 14
                    color: "transparent"
                    border.width: 3
                    border.color: Qt.alpha(look.ink, 0.9)
                    visible: cell.current
                }
                Rectangle { anchors.fill: parent; color: "white" }
                Image {
                    anchors.fill: parent
                    asynchronous: true
                    sourceSize.width: Math.ceil(150 * Screen.devicePixelRatio)
                    source: "image://page/" + pdf.revision + "/" + cell.index
                }
                Corners { anchors.fill: parent }
                Rectangle {
                    visible: cell.hits > 0
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 8
                    height: 24
                    width: Math.max(24, count.implicitWidth + 12)
                    radius: 12
                    color: "#141414"
                    Text { id: count; anchors.centerIn: parent; text: cell.hits; color: "#F8F8FF"; font.pixelSize: 12 }
                }
            }
            Text {
                anchors.horizontalCenter: thumb.horizontalCenter
                anchors.top: thumb.bottom
                anchors.topMargin: 8
                text: cell.index + 1
                font.pixelSize: 13
                color: cell.current ? look.ink : look.softInk
            }
            TapHandler {
                onTapped: {
                    const first = pdf.searchHits.find(h => h.page === cell.index);
                    sheet.app.closePages(cell.index, first);
                }
            }
        }
        Component.onCompleted: positionViewAtIndex(pdf.page, GridView.Center)
    }
}
