import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import co.goodinput.office

// Aa in Sheets: everything the chosen cells can be, in one note under its
// button. How numbers read, the text, the cell's colour, rows and columns,
// what stays in sight while scrolling, and what can be put in.
Rectangle {
    id: sheet

    property OfficeDocument document
    readonly property var states: document ? document.states : ({})
    property bool askingComment: false

    signal done()

    color: Theme.chrome
    radius: Theme.noteRadius
    width: 420
    implicitHeight: column.implicitHeight + 2 * Theme.comfortable

    function run(command, args) { document.command(command, args || {}) }
    function on(command) { return states[command] === "true" }

    component Section: Column {
        property alias label: heading.text
        default property alias controls: flow.data
        width: parent.width
        spacing: Theme.compact
        Text { id: heading; leftPadding: Theme.tight; color: Theme.secondary; font.pixelSize: Theme.metaSize }
        Flow { id: flow; width: parent.width; spacing: 6 }
    }
    component Chip: Pill { raised: !selected; implicitHeight: 40 }
    component Glyph: IconPill { raised: !selected; implicitHeight: 40 }
    // A round sample of a fill colour; -1 is none.
    component Swatch: AbstractButton {
        id: swatch
        property color fill
        property int value
        implicitWidth: 40
        implicitHeight: 40
        Accessible.name: value < 0 ? qsTr("No colour") : qsTr("Colour")
        onClicked: sheet.run(".uno:BackgroundColor", { "BackgroundColor": value })
        contentItem: Item {
            Rectangle {
                anchors.centerIn: parent
                width: 28; height: 28; radius: 14
                color: swatch.value < 0 ? "transparent" : swatch.fill
                border.width: swatch.value < 0 ? 2 : 0
                border.color: Theme.divider
                Rectangle {
                    visible: swatch.value < 0
                    anchors.centerIn: parent
                    width: 2; height: 30; rotation: 45
                    color: Theme.divider
                }
            }
        }
        background: Rectangle { radius: 20; color: swatch.hovered ? Theme.hover : "transparent" }
    }

    Column {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.comfortable
        spacing: 14

        Section {
            label: qsTr("Numbers")
            Chip { label: qsTr("Number"); selected: sheet.on(".uno:NumberFormatStandard"); onClicked: sheet.run(".uno:NumberFormatStandard") }
            Chip { label: qsTr("Money"); selected: sheet.on(".uno:NumberFormatCurrency"); onClicked: sheet.run(".uno:NumberFormatCurrency") }
            Chip { label: qsTr("Percent"); selected: sheet.on(".uno:NumberFormatPercent"); onClicked: sheet.run(".uno:NumberFormatPercent") }
            Chip { label: qsTr("Date"); selected: sheet.on(".uno:NumberFormatDate"); onClicked: sheet.run(".uno:NumberFormatDate") }
            Chip { label: qsTr("Time"); selected: sheet.on(".uno:NumberFormatTime"); onClicked: sheet.run(".uno:NumberFormatTime") }
            Row {
                spacing: 6
                Text { text: qsTr("Decimals"); color: Theme.secondary; font.pixelSize: Theme.metaSize; anchors.verticalCenter: parent.verticalCenter; leftPadding: Theme.tight }
                Chip { label: "−"; implicitWidth: 44; Accessible.name: qsTr("Fewer decimals"); onClicked: sheet.run(".uno:NumberFormatDecDecimals") }
                Chip { label: "+"; implicitWidth: 44; Accessible.name: qsTr("More decimals"); onClicked: sheet.run(".uno:NumberFormatIncDecimals") }
            }
        }

        Section {
            label: qsTr("Text")
            Chip { label: "B"; implicitWidth: 44; labelFont.bold: true; selected: sheet.on(".uno:Bold"); onClicked: sheet.run(".uno:Bold") }
            Chip { label: "I"; implicitWidth: 44; labelFont.italic: true; labelFont.family: "serif"; selected: sheet.on(".uno:Italic"); onClicked: sheet.run(".uno:Italic") }
            Chip { label: "U"; implicitWidth: 44; labelFont.underline: true; selected: sheet.on(".uno:Underline"); onClicked: sheet.run(".uno:Underline") }
            Glyph { glyph: "align-left"; Accessible.name: qsTr("Left"); selected: sheet.on(".uno:AlignLeft"); onClicked: sheet.run(".uno:AlignLeft") }
            Glyph { glyph: "align-center"; Accessible.name: qsTr("Centre"); selected: sheet.on(".uno:AlignHorizontalCenter"); onClicked: sheet.run(".uno:AlignHorizontalCenter") }
            Glyph { glyph: "align-right"; Accessible.name: qsTr("Right"); selected: sheet.on(".uno:AlignRight"); onClicked: sheet.run(".uno:AlignRight") }
            Chip { label: qsTr("Wrap"); selected: sheet.on(".uno:WrapText"); onClicked: sheet.run(".uno:WrapText") }
        }

        Section {
            label: qsTr("Cell colour")
            Swatch { value: -1 }
            Swatch { fill: "#EAF5FB"; value: 0xEAF5FB }
            Swatch { fill: "#E6F4EA"; value: 0xE6F4EA }
            Swatch { fill: "#FEF7E0"; value: 0xFEF7E0 }
            Swatch { fill: "#FCE8E6"; value: 0xFCE8E6 }
            Swatch { fill: "#F3E8FD"; value: 0xF3E8FD }
            Swatch { fill: "#EEEEF2"; value: 0xEEEEF2 }
        }

        Section {
            label: qsTr("Rows and columns")
            Chip { label: qsTr("Row above"); onClicked: sheet.run(".uno:InsertRowsBefore") }
            Chip { label: qsTr("Row below"); onClicked: sheet.run(".uno:InsertRowsAfter") }
            Chip { label: qsTr("Column left"); onClicked: sheet.run(".uno:InsertColumnsBefore") }
            Chip { label: qsTr("Column right"); onClicked: sheet.run(".uno:InsertColumnsAfter") }
            Chip { label: qsTr("Delete row"); onClicked: sheet.run(".uno:DeleteRows") }
            Chip { label: qsTr("Delete column"); onClicked: sheet.run(".uno:DeleteColumns") }
            Chip { label: qsTr("Merge"); selected: sheet.on(".uno:ToggleMergeCells"); onClicked: sheet.run(".uno:ToggleMergeCells") }
        }

        Section {
            label: qsTr("Keep in sight")
            Chip { label: qsTr("Top row"); selected: sheet.on(".uno:FreezePanesFirstRow"); onClicked: sheet.run(".uno:FreezePanesFirstRow") }
            Chip { label: qsTr("First column"); selected: sheet.on(".uno:FreezePanesFirstColumn"); onClicked: sheet.run(".uno:FreezePanesFirstColumn") }
        }

        Section {
            visible: !sheet.askingComment
            label: qsTr("Insert")
            Chip { label: qsTr("Picture"); onClicked: pictureDialog.open() }
            Chip { label: qsTr("Comment"); onClicked: { sheet.askingComment = true; comment.text = ""; comment.forceActiveFocus() } }
            Chip { label: qsTr("Filter"); selected: sheet.on(".uno:DataFilterAutoFilter"); onClicked: sheet.run(".uno:DataFilterAutoFilter") }
        }

        // A comment is asked for in place.
        Row {
            visible: sheet.askingComment
            spacing: 6
            TextField {
                id: comment
                width: 260
                implicitHeight: Theme.control
                placeholderText: qsTr("Comment")
                placeholderTextColor: Theme.secondary
                color: Theme.text
                font.pixelSize: 15
                leftPadding: Theme.comfortable
                background: Rectangle { radius: height / 2; color: Theme.raised }
                onAccepted: add.clicked()
                Keys.onEscapePressed: sheet.askingComment = false
            }
            Pill {
                id: add
                accent: true
                label: qsTr("Add")
                enabled: comment.text.length > 0
                onClicked: {
                    sheet.run(".uno:InsertAnnotation", { "Text": comment.text })
                    sheet.askingComment = false
                    sheet.done()
                }
            }
        }
    }

    FileDialog {
        id: pictureDialog
        title: qsTr("Choose a picture")
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.gif *.webp *.svg)")]
        onAccepted: {
            sheet.run(".uno:InsertGraphic", { "FileName": String(selectedFile) })
            sheet.done()
        }
    }
}
