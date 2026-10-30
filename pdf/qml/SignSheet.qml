import QtQuick

// Drawing your signature, in a floating note over the dimmed page. It is
// kept on this computer only; "Place on page" keeps it and then asks where
// it goes. A signature already kept shows here, ready to place again.
Item {
    id: sheet

    required property Item app
    readonly property real padAspect: pad.width / pad.height
    property var strokes: []
    property var drawing: null
    signal done()

    function load() {
        strokes = pdf.hasSignature ? pdf.signatureStrokes() : [];
        pad.requestPaint();
    }

    Rectangle { // the dimmed page behind
        anchors.fill: parent
        color: Qt.alpha(look.desk, 0.55)
        TapHandler { onTapped: sheet.done() }
    }

    Rectangle {
        id: note
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 32)
        height: column.implicitHeight + 48
        radius: 12
        color: look.chrome
        Shadow { anchors.fill: parent; radius: 12; z: -1 }
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Your signature")
        TapHandler {} // a tap on the note stays on the note

        Column {
            id: column
            x: 24
            y: 24
            width: parent.width - 48
            spacing: 16

            Item {
                width: parent.width
                height: 24
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Your signature")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: look.ink
                }
                IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: -10
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "x"
                    label: qsTr("Close")
                    onClicked: sheet.done()
                }
            }

            Canvas {
                id: pad
                width: parent.width
                height: 180
                Accessible.role: Accessible.Canvas
                Accessible.name: qsTr("Draw your signature here")
                onPaint: {
                    const c = getContext("2d");
                    c.reset();
                    c.fillStyle = look.fill(0.08);
                    c.beginPath();
                    c.roundedRect(0, 0, width, height, 10, 10);
                    c.fill();
                    c.strokeStyle = look.fill(0.24);
                    c.lineWidth = 1;
                    c.beginPath();
                    c.moveTo(40, height - 40);
                    c.lineTo(width - 40, height - 40);
                    c.stroke();
                    c.strokeStyle = look.ink;
                    c.lineWidth = 2.5;
                    c.lineCap = "round";
                    c.lineJoin = "round";
                    const all = sheet.drawing ? sheet.strokes.concat([sheet.drawing]) : sheet.strokes;
                    for (const s of all) {
                        if (s.length < 2)
                            continue;
                        c.beginPath();
                        c.moveTo(s[0].x * width, s[0].y * height);
                        for (let i = 1; i < s.length; ++i)
                            c.lineTo(s[i].x * width, s[i].y * height);
                        c.stroke();
                    }
                }
                DragHandler {
                    target: null
                    dragThreshold: 0
                    onActiveChanged: {
                        if (active) {
                            sheet.drawing = [];
                        } else if (sheet.drawing) {
                            if (sheet.drawing.length > 1)
                                sheet.strokes = sheet.strokes.concat([sheet.drawing]);
                            sheet.drawing = null;
                            pad.requestPaint();
                        }
                    }
                    onCentroidChanged: {
                        if (!active || !sheet.drawing)
                            return;
                        const p = centroid.position;
                        sheet.drawing.push({ x: Math.max(0, Math.min(1, p.x / pad.width)),
                                             y: Math.max(0, Math.min(1, p.y / pad.height)) });
                        pad.requestPaint();
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("Draw with a finger or pen. It is kept on this computer only.")
                font.pixelSize: 13
                color: look.softInk
            }

            Item {
                width: parent.width
                height: 44
                Pill {
                    raised: true
                    text: qsTr("Clear")
                    onClicked: { sheet.strokes = []; pad.requestPaint(); }
                }
                Pill {
                    anchors.right: parent.right
                    primary: true
                    text: qsTr("Place on page")
                    opacity: sheet.strokes.length > 0 ? 1 : 0.4
                    onClicked: {
                        if (sheet.strokes.length === 0 || !pdf.saveSignature(sheet.strokes, sheet.padAspect))
                            return;
                        sheet.done();
                        sheet.app.mode = "signing";
                    }
                }
            }
        }
    }
}
