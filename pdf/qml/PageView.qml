import QtQuick

// The pages, one under the next on the desk, 24 px apart. A page opens fitted
// to the window's height; a pinch or Ctrl and the wheel zooms, from fitted to
// four times. The page in view is the session's current page.
ListView {
    id: view

    required property Item app
    property real zoom: 1
    readonly property real fitWidth: {
        const aspect = pdf.pageCount > 0 ? pdf.pageAspect(0) : 11 / 8.5;
        return Math.max(120, Math.min(width - 48, (height - 48) / aspect));
    }
    readonly property real pageWidth: Math.round(fitWidth * zoom)

    spacing: 24
    topMargin: 24
    bottomMargin: 120 // the last page can scroll clear of the bar
    clip: true
    model: pdf.status === "open" ? pdf.pageCount : 0
    cacheBuffer: height
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.AutoFlickIfNeeded
    contentWidth: Math.max(width, pageWidth + 48)
    reuseItems: true

    delegate: Item {
        required property int index
        width: view.contentWidth
        height: Math.round(view.pageWidth * pdf.pageAspect(index))
        PageItem {
            pageIndex: index
            app: view.app
            width: view.pageWidth
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // The page whose middle band is in view.
    function pageInView() {
        const i = indexAt(contentX + width / 2, contentY + height / 3);
        return i >= 0 ? i : pdf.page;
    }
    onContentYChanged: {
        if (!moving && !flicking)
            return;
        pdf.page = pageInView();
        app.scrolled();
    }
    function show(index) {
        positionViewAtIndex(Math.max(0, Math.min(count - 1, index)), ListView.Beginning);
        contentY -= topMargin;
    }
    function showRect(index, r) {
        // Bring a match or a field into the middle of the window.
        positionViewAtIndex(index, ListView.Beginning);
        const top = itemAtIndex(index) ? itemAtIndex(index).y : contentY;
        contentY = Math.max(-topMargin, top + r.y * view.pageWidth * pdf.pageAspect(index) - height / 3);
    }
    Connections {
        target: pdf
        function onOpened() { view.zoom = 1; Qt.callLater(() => view.show(pdf.page)); }
    }

    function zoomTo(z, around) {
        const old = zoom;
        zoom = Math.max(1, Math.min(4, z));
        if (old === zoom)
            return;
        // Keep the point under the fingers (or the middle) where it was.
        const ax = around ? around.x : width / 2, ay = around ? around.y : height / 2;
        const f = zoom / old;
        contentX = Math.max(0, (contentX + ax) * f - ax);
        contentY = (contentY + ay) * f - ay;
        returnToBounds();
    }
    PinchHandler {
        target: null
        property real startZoom: 1
        onActiveChanged: if (active) startZoom = view.zoom
        onActiveScaleChanged: view.zoomTo(startZoom * activeScale, centroid.position)
    }
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: (event) => view.zoomTo(view.zoom * (event.angleDelta.y > 0 ? 1.1 : 1 / 1.1),
                                        Qt.point(event.x, event.y))
    }

    // Where in the file you are, at the right edge, while the pages move.
    Item {
        id: scrubber
        parent: view
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 4
        height: Math.min(560, view.height - 240)
        opacity: view.moving ? 1 : 0
        visible: opacity > 0 && pdf.pageCount > 1
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Rectangle { anchors.fill: parent; radius: 2; color: look.fill(0.08) }
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: -8
            y: (parent.height - height) * (pdf.page / Math.max(1, pdf.pageCount - 1))
            height: 28
            width: number.implicitWidth + 20
            radius: 14
            color: look.raised
            Text {
                id: number
                anchors.centerIn: parent
                text: pdf.page + 1
                color: look.softInk
                font.pixelSize: 12
            }
        }
    }
}
