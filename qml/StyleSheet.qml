import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import co.goodinput.office

// Aa: every other way to shape the writing, in one note under its button.
// The Text side holds the paragraph styles, the font and its size and look,
// line spacing, lists and alignment, and what can be put in; the Page side
// holds the paper, which way it is turned, margins, page numbers, and the top
// and bottom of each page.
Rectangle {
    id: sheet

    property OfficeDocument document
    readonly property var states: document ? document.states : ({})
    readonly property string currentStyle: states[".uno:StyleApply"] || ""
    readonly property string currentFont: states[".uno:CharFontName"] || ""
    property string side: "text"

    // The first page's shape, in twips, tells which paper and which way.
    readonly property rect firstPage: document && document.pages.length ? document.pages[0] : Qt.rect(0, 0, 0, 0)
    readonly property bool sideways: firstPage.width > firstPage.height
    readonly property real longSide: Math.max(firstPage.width, firstPage.height)
    readonly property real shortSide: Math.min(firstPage.width, firstPage.height)

    signal done()

    color: Theme.chrome
    radius: Theme.noteRadius
    width: 420
    implicitHeight: column.implicitHeight + 2 * Theme.comfortable

    function applyStyle(name) {
        document.command(".uno:StyleApply", { "Style": name, "FamilyName": "ParagraphStyles" })
    }
    function applyFont(name) {
        document.command(".uno:CharFontName", { "CharFontName.FamilyName": name })
    }
    // Paper sizes in hundredths of a millimetre, the long side last.
    function setPaper(width, height) {
        const w = sideways ? height : width
        const h = sideways ? width : height
        document.command(".uno:AttributePageSize", { "AttributePageSize.Width": w, "AttributePageSize.Height": h })
    }
    function turn(toSideways) {
        if (toSideways === sideways || longSide <= 0)
            return
        // Twips to hundredths of a millimetre.
        const lng = Math.round(longSide * 2540 / 1440)
        const shrt = Math.round(shortSide * 2540 / 1440)
        document.command(".uno:AttributePageSize", toSideways
            ? { "AttributePageSize.Width": lng, "AttributePageSize.Height": shrt }
            : { "AttributePageSize.Width": shrt, "AttributePageSize.Height": lng })
        document.command(".uno:AttributePage", { "AttributePage.Landscape": toSideways })
    }
    function setMargins(hundredths) {
        document.command(".uno:PageLRMargin", { "PageLRMargin.Left": hundredths, "PageLRMargin.Right": hundredths })
        document.command(".uno:PageULMargin", { "PageULMargin.Upper": hundredths, "PageULMargin.Lower": hundredths })
    }
    // Within a few twips, for paper that came from another program.
    function near(a, b) { return Math.abs(a - b) < 60 }

    // A labelled group of controls that wrap onto a second line when full.
    component Section: Column {
        property alias label: heading.text
        default property alias controls: flow.data
        width: parent.width
        spacing: Theme.compact
        Text { id: heading; leftPadding: Theme.tight; color: Theme.secondary; font.pixelSize: Theme.metaSize }
        Flow { id: flow; width: parent.width; spacing: 6 }
    }

    component Chip: Pill {
        raised: !selected
        implicitHeight: 40
    }

    component Glyph: IconPill {
        raised: !selected
        implicitHeight: 40
    }

    Column {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.comfortable
        spacing: 14

        // Text or Page.
        Rectangle {
            width: parent.width
            height: 48
            radius: height / 2
            color: Theme.desk
            Row {
                anchors.fill: parent
                anchors.margins: Theme.tight
                spacing: Theme.tight
                Pill { width: (parent.width - Theme.tight) / 2; height: 40; label: qsTr("Text"); selected: sheet.side === "text"; onClicked: sheet.side = "text" }
                Pill { width: (parent.width - Theme.tight) / 2; height: 40; label: qsTr("Page"); selected: sheet.side === "page"; onClicked: sheet.side = "page" }
            }
        }

        // The Text side.
        Section {
            visible: sheet.side === "text"
            label: qsTr("Style")
            Chip { label: qsTr("Title"); labelFont.pixelSize: 17; labelFont.weight: Font.Bold; selected: sheet.currentStyle === "Title"; onClicked: sheet.applyStyle("Title") }
            Chip { label: qsTr("Heading"); labelFont.weight: Font.DemiBold; selected: sheet.currentStyle === "Heading 1"; onClicked: sheet.applyStyle("Heading 1") }
            Chip { label: qsTr("Subheading"); labelFont.weight: Font.DemiBold; labelFont.pixelSize: 14; selected: sheet.currentStyle === "Heading 2"; onClicked: sheet.applyStyle("Heading 2") }
            Chip {
                label: qsTr("Body"); labelFont.family: "serif"
                selected: ["Default Paragraph Style", "Standard", "Text Body", "Body Text", "Normal"].indexOf(sheet.currentStyle) >= 0
                onClicked: sheet.applyStyle("Default Paragraph Style")
            }
            Chip { label: qsTr("Quote"); labelFont.family: "serif"; labelFont.italic: true; selected: ["Quotations", "Quote"].indexOf(sheet.currentStyle) >= 0; onClicked: sheet.applyStyle("Quotations") }
        }

        Section {
            visible: sheet.side === "text"
            label: qsTr("Font")
            Chip { label: qsTr("Serif"); labelFont.family: "Liberation Serif"; labelFont.pixelSize: 16; selected: sheet.currentFont === "Liberation Serif"; onClicked: sheet.applyFont("Liberation Serif") }
            Chip { label: qsTr("Sans"); labelFont.family: "Liberation Sans"; labelFont.pixelSize: 16; selected: sheet.currentFont === "Liberation Sans"; onClicked: sheet.applyFont("Liberation Sans") }
            Chip { label: qsTr("Mono"); labelFont.family: "Liberation Mono"; labelFont.pixelSize: 16; selected: sheet.currentFont === "Liberation Mono"; onClicked: sheet.applyFont("Liberation Mono") }
            Chip {
                label: ["Liberation Serif", "Liberation Sans", "Liberation Mono", ""].indexOf(sheet.currentFont) < 0 ? sheet.currentFont : qsTr("More fonts…")
                selected: ["Liberation Serif", "Liberation Sans", "Liberation Mono", ""].indexOf(sheet.currentFont) < 0
                onClicked: { fontList.model = sheet.document.fontNames(); fonts.open() }
            }
        }

        Section {
            visible: sheet.side === "text"
            label: qsTr("Size and look")
            Chip { label: "B"; implicitWidth: 44; labelFont.bold: true; labelFont.pixelSize: 17; selected: sheet.states[".uno:Bold"] === "true"; onClicked: sheet.document.command(".uno:Bold") }
            Chip { label: "I"; implicitWidth: 44; labelFont.italic: true; labelFont.family: "serif"; labelFont.pixelSize: 17; selected: sheet.states[".uno:Italic"] === "true"; onClicked: sheet.document.command(".uno:Italic") }
            Chip { label: "U"; implicitWidth: 44; labelFont.underline: true; labelFont.pixelSize: 17; selected: sheet.states[".uno:Underline"] === "true"; onClicked: sheet.document.command(".uno:Underline") }
            Chip { label: "S"; implicitWidth: 44; labelFont.strikeout: true; labelFont.pixelSize: 17; selected: sheet.states[".uno:Strikeout"] === "true"; onClicked: sheet.document.command(".uno:Strikeout") }
            Rectangle {
                height: 40
                width: 104
                radius: height / 2
                color: Theme.raised
                Pill { anchors.left: parent.left; height: 40; label: "−"; implicitWidth: 40; labelFont.pixelSize: 20; Accessible.name: qsTr("Smaller"); onClicked: sheet.document.command(".uno:Shrink") }
                Text {
                    anchors.centerIn: parent
                    color: Theme.text
                    font.pixelSize: 15
                    text: {
                        const h = sheet.states[".uno:FontHeight"]
                        return h === undefined ? "" : String(Math.round(Number(h)))
                    }
                }
                Pill { anchors.right: parent.right; height: 40; label: "+"; implicitWidth: 40; labelFont.pixelSize: 20; Accessible.name: qsTr("Larger"); onClicked: sheet.document.command(".uno:Grow") }
            }
        }

        Section {
            visible: sheet.side === "text"
            label: qsTr("Line spacing")
            Chip { label: qsTr("Single"); selected: sheet.states[".uno:SpacePara1"] === "true"; onClicked: sheet.document.command(".uno:SpacePara1") }
            Chip { label: "1.5"; selected: sheet.states[".uno:SpacePara15"] === "true"; onClicked: sheet.document.command(".uno:SpacePara15") }
            Chip { label: qsTr("Double"); selected: sheet.states[".uno:SpacePara2"] === "true"; onClicked: sheet.document.command(".uno:SpacePara2") }
        }

        Section {
            visible: sheet.side === "text"
            label: qsTr("Lists and alignment")
            Glyph { glyph: "list"; Accessible.name: qsTr("Bulleted list"); selected: sheet.states[".uno:DefaultBullet"] === "true"; onClicked: sheet.document.command(".uno:DefaultBullet") }
            Glyph { glyph: "list-ordered"; Accessible.name: qsTr("Numbered list"); selected: sheet.states[".uno:DefaultNumbering"] === "true"; onClicked: sheet.document.command(".uno:DefaultNumbering") }
            Glyph { glyph: "list-checks"; Accessible.name: qsTr("Checklist"); onClicked: sheet.document.command(".uno:InsertCheckboxContentControl") }
            Glyph { glyph: "align-left"; Accessible.name: qsTr("Align left"); selected: sheet.states[".uno:LeftPara"] === "true"; onClicked: sheet.document.command(".uno:LeftPara") }
            Glyph { glyph: "align-center"; Accessible.name: qsTr("Centre"); selected: sheet.states[".uno:CenterPara"] === "true"; onClicked: sheet.document.command(".uno:CenterPara") }
        }

        Section {
            visible: sheet.side === "text"
            label: qsTr("Insert")
            Chip { label: qsTr("Picture"); onClicked: pictureDialog.open() }
            Chip { label: qsTr("Table"); onClicked: { sheet.document.command(".uno:InsertTable", { "Columns": 3, "Rows": 3 }); sheet.done() } }
            Chip { label: qsTr("Page break"); onClicked: { sheet.document.command(".uno:InsertPagebreak"); sheet.done() } }
            Chip { label: qsTr("Footnote"); onClicked: { sheet.document.command(".uno:InsertFootnote"); sheet.done() } }
        }

        // The Page side.
        Section {
            visible: sheet.side === "page"
            label: qsTr("Paper")
            Chip { label: qsTr("Letter"); selected: sheet.near(sheet.shortSide, 12240) && sheet.near(sheet.longSide, 15840); onClicked: sheet.setPaper(21590, 27940) }
            Chip { label: "A4"; selected: sheet.near(sheet.shortSide, 11906) && sheet.near(sheet.longSide, 16838); onClicked: sheet.setPaper(21000, 29700) }
            Chip { label: qsTr("Legal"); selected: sheet.near(sheet.shortSide, 12240) && sheet.near(sheet.longSide, 20160); onClicked: sheet.setPaper(21590, 35560) }
        }

        Section {
            visible: sheet.side === "page"
            label: qsTr("Turn")
            Chip { label: qsTr("Upright"); selected: sheet.longSide > 0 && !sheet.sideways; onClicked: sheet.turn(false) }
            Chip { label: qsTr("Sideways"); selected: sheet.sideways; onClicked: sheet.turn(true) }
        }

        Section {
            visible: sheet.side === "page"
            label: qsTr("Margins")
            Chip { label: qsTr("Narrow"); onClicked: sheet.setMargins(1270) }
            Chip { label: qsTr("Normal"); onClicked: sheet.setMargins(2540) }
            Chip { label: qsTr("Wide"); onClicked: sheet.setMargins(3810) }
        }

        Section {
            visible: sheet.side === "page"
            label: qsTr("Page numbers")
            Chip { label: qsTr("Bottom middle"); onClicked: { sheet.document.addPageNumbers("bottom-middle"); sheet.done() } }
            Chip { label: qsTr("Bottom corner"); onClicked: { sheet.document.addPageNumbers("bottom-corner"); sheet.done() } }
            Chip { label: qsTr("Top corner"); onClicked: { sheet.document.addPageNumbers("top-corner"); sheet.done() } }
        }

        Section {
            visible: sheet.side === "page"
            label: qsTr("Top and bottom of each page")
            Chip {
                label: qsTr("Add a header")
                onClicked: { sheet.document.command(".uno:InsertPageHeader", { "PageStyle": "Default Page Style", "On": true }); sheet.done() }
            }
            Chip {
                label: qsTr("Add a footer")
                onClicked: { sheet.document.command(".uno:InsertPageFooter", { "PageStyle": "Default Page Style", "On": true }); sheet.done() }
            }
        }
    }

    // Every font the engine has, each written in itself.
    Popup {
        id: fonts
        x: 0
        y: 0
        width: sheet.width
        height: Math.min(520, sheet.height)
        padding: Theme.compact
        background: Rectangle { color: Theme.chrome; radius: Theme.noteRadius; border.width: 1; border.color: Theme.divider }
        ListView {
            id: fontList
            anchors.fill: parent
            clip: true
            delegate: Pill {
                required property string modelData
                width: ListView.view.width
                height: Theme.control
                label: modelData
                labelFont.family: modelData
                labelFont.pixelSize: 16
                selected: modelData === sheet.currentFont
                onClicked: { sheet.applyFont(modelData); fonts.close() }
            }
        }
    }

    FileDialog {
        id: pictureDialog
        title: qsTr("Choose a picture")
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.gif *.webp *.svg)")]
        onAccepted: {
            sheet.document.command(".uno:InsertGraphic", { "FileName": String(selectedFile) })
            sheet.done()
        }
    }
}
