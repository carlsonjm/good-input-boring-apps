import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// What a chosen slide can do, floating under it: its helpers first, then
// duplicating and deleting it. Fit this slide is Codex's and shows only where
// Split Rock's helpers are on this computer; Tidy the look is the app's own.
Rectangle {
    id: bar

    property OfficeDocument document
    property SlideHelpers helpers

    signal done()
    // Fit this slide was tapped without a Codex sign-in.
    signal needsSignIn()

    color: Theme.chrome
    radius: height / 2
    implicitWidth: row.implicitWidth + 2 * Theme.tight
    implicitHeight: 52

    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: -6
        radius: parent.radius
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.12)
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Pill {
            visible: bar.helpers.available
            raised: true
            glyph: "wand"
            label: qsTr("Fit this slide")
            onClicked: {
                if (bar.helpers.codexReady) {
                    bar.helpers.fit()
                    bar.done()
                } else {
                    bar.needsSignIn()
                }
            }
        }
        Pill { raised: true; glyph: "wand"; label: qsTr("Tidy the look"); onClicked: { bar.helpers.tidy(); bar.done() } }
        Rectangle { width: 1; height: 24; color: Theme.divider; anchors.verticalCenter: parent.verticalCenter }
        IconPill { glyph: "copy"; Accessible.name: qsTr("Duplicate"); onClicked: { bar.document.command(".uno:DuplicatePage"); bar.done() } }
        IconPill { glyph: "trash-2"; Accessible.name: qsTr("Delete"); enabled: bar.document && bar.document.parts > 1; onClicked: { bar.document.command(".uno:DeletePage"); bar.done() } }
    }
}
