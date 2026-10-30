import QtQuick
import co.goodinput.office

// The one quiet line above the page: back to the list, the name, whether it
// is saved, undo and redo, Aa for every style, and More.
Item {
    id: line

    property OfficeDocument document
    property bool styleOpen: false
    property bool showStyle: true
    property alias trailing: extra.data

    signal back()
    signal styleRequested()
    signal moreRequested(Item anchor)

    implicitHeight: 64

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.comfortable
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.standard

        IconPill {
            glyph: "chevron-left"
            Accessible.name: qsTr("Your documents")
            onClicked: line.back()
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.compact
            Text {
                text: line.document ? line.document.title : ""
                color: Theme.text
                font.pixelSize: Theme.bodySize
                font.weight: Font.Medium
                elide: Text.ElideRight
                width: Math.min(implicitWidth, line.width - 420)
            }
            Text {
                anchors.baseline: parent.children[0].baseline
                color: line.document && line.document.saveError.length ? Theme.error : Theme.secondary
                font.pixelSize: Theme.metaSize
                text: !line.document ? ""
                    : line.document.saveError.length ? qsTr("Couldn't save")
                    : line.document.saving ? qsTr("Saving")
                    : line.document.modified ? (line.document.autosave ? qsTr("Edited") : qsTr("Not saved"))
                    : qsTr("Saved")
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.comfortable
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.compact

        Row {
            id: extra
            spacing: Theme.compact
            anchors.verticalCenter: parent.verticalCenter
        }
        Row {
            spacing: Theme.tight
            IconPill {
                raised: true
                glyph: "undo-2"
                Accessible.name: qsTr("Undo")
                enabled: line.document && line.document.canUndo
                onClicked: line.document.undo()
            }
            IconPill {
                raised: true
                glyph: "redo-2"
                Accessible.name: qsTr("Redo")
                enabled: line.document && line.document.canRedo
                onClicked: line.document.redo()
            }
        }
        Pill {
            visible: line.showStyle
            raised: true
            selected: line.styleOpen
            label: "Aa"
            labelFont.pixelSize: 16
            labelFont.weight: Font.DemiBold
            implicitWidth: 64
            onClicked: line.styleRequested()
        }
        IconPill {
            id: more
            glyph: "ellipsis"
            Accessible.name: qsTr("More")
            onClicked: line.moreRequested(more)
        }
    }
}
