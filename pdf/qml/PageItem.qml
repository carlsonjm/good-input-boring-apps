import QtQuick

// One page: white paper with the paper tier's corners, drawn sharp at its
// shown width, with what lies on it: search matches, the selection and its
// two ends, and the form's fields. Geometry from the session is normalised,
// so everything here is a fraction of the page's width and height.
Item {
    id: page

    required property int pageIndex
    required property Item app          // Main: selection, field, signing state
    readonly property real w: width
    readonly property real h: height

    function at(r) { return Qt.rect(r.x * w, r.y * h, r.w * w, r.h * h); }

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Page %1 of %2").arg(pageIndex + 1).arg(pdf.pageCount)

    Rectangle { anchors.fill: parent; color: "white" }

    // Two pictures: the one shown stays until its redraw has arrived, so a
    // new mark or answer never flashes the page blank.
    property bool frontIsA: true
    readonly property string wanted: "image://page/" + pdf.revision + "/" + pageIndex
    readonly property int pixelWidth: Math.min(4096, Math.ceil(width * Screen.devicePixelRatio))
    onWantedChanged: swap()
    onPixelWidthChanged: resize.restart()
    Timer { id: resize; interval: 120; onTriggered: page.swap() }
    function swap() {
        const back = frontIsA ? pictureB : pictureA;
        back.sourceSize.width = pixelWidth;
        back.source = wanted;
    }
    Image {
        id: pictureA
        anchors.fill: parent
        asynchronous: true
        cache: false
        smooth: true
        visible: page.frontIsA
        onStatusChanged: if (status === Image.Ready && source == page.wanted) page.frontIsA = true
    }
    Image {
        id: pictureB
        anchors.fill: parent
        asynchronous: true
        cache: false
        smooth: true
        visible: !page.frontIsA
        onStatusChanged: if (status === Image.Ready && source == page.wanted) page.frontIsA = false
    }

    // Search matches.
    Repeater {
        model: pdf.searchHits.filter(hit => hit.page === page.pageIndex)
        Rectangle {
            required property var modelData
            readonly property rect r: page.at(modelData)
            x: r.x - 1; y: r.y - 1; width: r.width + 2; height: r.height + 2
            radius: 2
            color: Qt.alpha("#FFD60A", 0.45)
        }
    }

    // The selection and its ends.
    readonly property bool selected: app.selection && app.selection.page === pageIndex
    Repeater {
        model: page.selected ? app.selection.rects : []
        Rectangle {
            required property var modelData
            readonly property rect r: page.at(modelData)
            x: r.x; y: r.y; width: r.width; height: r.height
            radius: 2
            color: Qt.alpha(look.accent, 0.30)
        }
    }
    Repeater {
        model: page.selected ? 2 : 0
        Item {
            id: end
            required property int index
            readonly property var rects: app.selection.rects
            readonly property rect edge: page.at(index === 0 ? rects[0] : rects[rects.length - 1])
            readonly property real ex: index === 0 ? edge.x : edge.x + edge.width
            x: ex - 22
            y: edge.y
            width: 44
            height: edge.height + 30
            Rectangle {
                x: 21; width: 2; height: end.edge.height
                color: look.accent
            }
            Rectangle {
                x: 22 - 9; y: end.edge.height + 2
                width: 18; height: 18
                radius: 9
                color: look.accent
            }
            DragHandler {
                target: null
                onCentroidChanged: {
                    if (!active)
                        return;
                    const p = end.mapToItem(page, centroid.position.x, Math.max(0, centroid.position.y - 24));
                    page.app.dragSelectionEnd(page.pageIndex, end.index, p.x / page.w, p.y / page.h);
                }
            }
        }
    }

    // Form fields: a text field types in place, a tick box ticks.
    property var fields: []
    function readFields() { fields = pdf.fieldsOnPage(pageIndex); }
    Component.onCompleted: {
        pictureA.sourceSize.width = pixelWidth;
        pictureA.source = wanted;
        readFields();
    }
    Connections {
        target: pdf
        function onFormChanged() { page.readFields(); }
        function onOpened() { page.readFields(); }
    }
    Repeater {
        model: page.fields
        Item {
            id: field
            required property var modelData
            readonly property rect r: page.at(modelData)
            readonly property bool current: page.app.field === modelData.index
            x: r.x; y: r.y; width: r.width; height: r.height
            visible: !modelData.readOnly && modelData.kind !== "signature" && modelData.kind !== "other"

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: 4
                color: "transparent"
                border.width: 2
                border.color: look.accent
                visible: field.current
            }
            // A fillable field wears a quiet wash of the accent, as PDF readers
            // mark fields; the one being filled turns to white paper.
            Rectangle {
                anchors.fill: parent
                color: field.current && field.modelData.kind === "text" ? "white" : Qt.alpha(look.accent, 0.12)
            }
            TextInput {
                id: typing
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                visible: field.current && field.modelData.kind === "text"
                enabled: visible
                text: field.modelData.text
                color: "#141414"
                selectionColor: Qt.alpha(look.accent, 0.35)
                selectedTextColor: "#141414"
                font.pixelSize: Math.max(10, Math.min(18, field.height * 0.6))
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                onVisibleChanged: if (visible) { forceActiveFocus(); cursorPosition = text.length; }
                onEditingFinished: pdf.setFieldText(field.modelData.index, text)
                Keys.onTabPressed: page.app.goToField(pdf.nextField(field.modelData.index))
                Keys.onBacktabPressed: page.app.goToField(pdf.previousField(field.modelData.index))
                Keys.onReturnPressed: page.app.goToField(pdf.nextField(field.modelData.index))
                Accessible.name: field.modelData.label
            }
            TapHandler {
                onTapped: {
                    if (field.modelData.kind === "check" || field.modelData.kind === "radio")
                        pdf.setFieldChecked(field.modelData.index, !field.modelData.checked);
                    page.app.goToField(field.modelData.index);
                }
            }
        }
    }

    // Reading taps: a double tap or a hold selects a word; a tap shows or
    // hides the bar, or lays the signature while signing.
    TapHandler {
        gesturePolicy: TapHandler.DragThreshold
        onTapped: (point) => {
            const p = point.position;
            if (page.app.signing) {
                page.app.placeSignatureAt(page.pageIndex, p.x / page.w, p.y / page.h);
                return;
            }
            if (tapCount === 2)
                page.app.selectWord(page.pageIndex, p.x / page.w, p.y / page.h);
            else
                page.app.tapped();
        }
        onLongPressed: page.app.selectWord(page.pageIndex, point.position.x / page.w, point.position.y / page.h)
    }

    Corners { anchors.fill: parent }
}
