import QtQuick
import QtQuick.Dialogs
import QtQuick.Window

// Good Input PDF: a piece of paper on the desk, nothing in the way, one row
// of controls that comes when it is wanted. Laid out as the approved mock-up
// (Basics 1.2, 30 Oct 2026) and the suite's Visual sheet.
Window {
    id: window

    width: 1280
    height: 800
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    color: look.desk
    title: pdf.status === "open" ? pdf.title : qsTr("PDF")

    // What is going on: "read", "highlight", "selecting", "form", "signing",
    // "signed" or "message". The bar shows the controls each one needs.
    property alias app: app
    Item {
        id: app
        anchors.fill: parent

        property string mode: "read"
        property var selection: null      // {page, text, rects, from: {x, y}, to: {x, y}}
        property string selectedMark: ""  // an existing highlight of ours, when the selection is one
        property int field: -1
        readonly property int fieldNumber: field + 1
        property string lastSignature: ""
        property int lastSignaturePage: -1
        property string message: ""
        property bool barHovered: false
        property bool barWanted: true

        // Reading hides the bar after a few quiet seconds; anything else keeps it.
        function tapped() { tapSoon.restart(); }
        Timer { id: tapSoon; interval: 260; onTriggered: { if (app.selection) app.clearSelection(); else app.toggleBar(); } }
        function toggleBar() { barWanted = !(barWanted && bar.shown); quiet.restart(); }
        function scrolled() { if (mode === "read") barWanted = false; }
        Timer { id: quiet; interval: 3000; running: app.mode === "read" && app.barWanted && !app.barHovered; onTriggered: app.barWanted = false }

        function backToReading() {
            clearSelection();
            field = -1;
            mode = "read";
            barWanted = true;
            quiet.restart();
        }
        function say(words) {
            message = words;
            mode = "message";
            barWanted = true;
            sayFor.restart();
        }
        Timer { id: sayFor; interval: 2800; onTriggered: if (app.mode === "message") app.backToReading() }

        // Selecting.
        function selectWord(page, x, y) {
            tapSoon.stop();
            const word = pdf.wordAt(page, x, y);
            if (!word.text) {
                clearSelection();
                return;
            }
            selection = { page: page, text: word.text, rects: word.rects, from: { x: x, y: y }, to: { x: x, y: y } };
            selectedMark = markUnder(page, word.rects[0]);
            if (mode === "read" || mode === "highlight")
                mode = mode === "highlight" ? "highlight" : "selecting";
            barWanted = true;
        }
        function markUnder(page, r) {
            const cx = r.x + r.w / 2, cy = r.y + r.h / 2;
            for (const h of pdf.highlights(page)) {
                if (!h.ours)
                    continue;
                for (const q of h.rects)
                    if (cx >= q.x && cx <= q.x + q.w && cy >= q.y && cy <= q.y + q.h)
                        return h.name;
            }
            return "";
        }
        function dragSelectionEnd(page, which, x, y) {
            if (!selection || selection.page !== page)
                return;
            const from = which === 0 ? { x: x, y: y } : selection.from;
            const to = which === 0 ? selection.to : { x: x, y: y };
            const run = pdf.textBetween(page, from.x, from.y, to.x, to.y);
            if (!run.text)
                return;
            selection = { page: page, text: run.text, rects: run.rects, from: from, to: to };
            selectedMark = "";
        }
        function clearSelection() {
            selection = null;
            selectedMark = "";
            if (mode === "selecting")
                mode = "read";
        }
        function highlightSelection(colour) {
            if (!selection)
                return;
            pdf.highlight(selection.page, selection.rects, colour);
            const keep = mode === "highlight";
            clearSelection();
            if (!keep)
                backToReading();
        }
        function removeHighlight(name) {
            if (selection)
                pdf.removeMark(selection.page, name);
            clearSelection();
        }
        function copySelection() {
            if (selection)
                pdf.copy(selection.text);
            clearSelection();
            say(qsTr("Copied"));
        }
        function searchSelection() {
            const words = selection ? selection.text : "";
            clearSelection();
            searchFor(words);
        }
        function searchFor(words) {
            openPages(true);
            pages.query = words;
        }

        // Forms.
        function goToField(index) {
            if (index < 0)
                return;
            clearSelection();
            field = index;
            mode = "form";
            barWanted = true;
            const f = pdf.field(index);
            if (f.page !== undefined)
                view.showRect(f.page, f);
        }

        // Signing.
        function openSign() { signSheet.load(); signSheet.visible = true; }
        function placeSignatureAt(page, x, y) {
            // A signature-sized box, 40% of the page wide, centred on the tap.
            const w = 0.4, h = 0.08;
            const name = pdf.placeSignature(page, Math.max(0, Math.min(1 - w, x - w / 2)),
                                            Math.max(0, Math.min(1 - h, y - h / 2)), w, h);
            if (!name) {
                backToReading();
                return;
            }
            lastSignature = name;
            lastSignaturePage = page;
            mode = "signed";
            signedFor.restart();
        }
        Timer { id: signedFor; interval: 6000; onTriggered: if (app.mode === "signed") app.backToReading() }
        function undoSignature() {
            pdf.removeMark(lastSignaturePage, lastSignature);
            lastSignature = "";
            backToReading();
        }

        // All pages and search.
        function openPages(searching) {
            clearSelection();
            pages.visible = true;
            if (searching)
                pages.focusSearch();
        }
        function closePages(page, hit) {
            pages.visible = false;
            if (page === undefined)
                return;
            pdf.page = page;
            if (hit)
                view.showRect(page, hit);
            else
                view.show(page);
        }

        function openMore() { more.visible = true; }
        // GOODINPUT_PDF_SHOT_DO in main.cpp reaches the app's functions through here.
        function callFromCheck(name, args) { return app[name].apply(app, args); }
        // For the screens check: shows the tip of the bar's button `n`, counted from 0.
        function showTip(n) { bar.showTip(n); }
        function act(what) {
            switch (what) {
            case "open": openDialog.open(); break;
            case "copy": copyDialog.open(); break;
            case "save": report(pdf.save()); break;
            case "forget": pdf.forgetSignature(); say(qsTr("Your signature is forgotten")); break;
            }
        }
        function report(result) {
            if (result === "saved")
                say(qsTr("Saved"));
            else if (result === "cannotWrite")
                say(qsTr("This file can't be changed here. Save a copy instead."));
            else if (result !== "nothingOpen")
                say(qsTr("That didn't save. Your file is as it was."));
        }
        function closeFile() { password.visible = false; pdf.close(); }

        PageView {
            id: view
            anchors.fill: parent
            app: app
            visible: pdf.status === "open"
        }

        // Nothing open, or what could not be opened.
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: pdf.status !== "open" && !password.visible
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                width: Math.min(implicitWidth, window.width - 32)
                wrapMode: Text.WordWrap
                font.pixelSize: 16
                color: look.softInk
                text: {
                    switch (pdf.status) {
                    case "missing": return qsTr("That file isn't there any more.");
                    case "notPdf": return qsTr("That file isn't a PDF this app can read.");
                    }
                    return qsTr("Open a PDF to read, fill in, sign or highlight it.");
                }
            }
            Pill {
                anchors.horizontalCenter: parent.horizontalCenter
                primary: true
                text: qsTr("Open a PDF")
                onClicked: openDialog.open()
            }
        }

        SelectionMenu {
            id: selectionMenu
            app: app
            markName: app.selectedMark
            visible: app.selection !== null && !pages.visible
            readonly property var anchorRect: {
                if (!app.selection)
                    return Qt.rect(0, 0, 0, 0);
                const item = view.itemAtIndex(app.selection.page);
                if (!item)
                    return Qt.rect(0, 0, 0, 0);
                const r = app.selection.rects[0];
                const pageItem = item.children[0];
                const p = pageItem.mapToItem(app, r.x * pageItem.width, r.y * pageItem.height);
                return Qt.rect(p.x, p.y, r.w * pageItem.width, r.h * pageItem.height);
            }
            x: Math.max(16, Math.min(app.width - width - 16, anchorRect.x + anchorRect.width / 2 - width / 2))
            y: anchorRect.y - height - 16 < 16 ? anchorRect.y + anchorRect.height + 40 : anchorRect.y - height - 16
            Connections { target: view; function onContentYChanged() { selectionMenu.anchorRectChanged(); } }
        }

        Bar {
            id: bar
            app: app
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            shown: pdf.status === "open" && !pages.visible && (app.barWanted || app.mode !== "read")
        }

        AllPages {
            id: pages
            app: app
            anchors.fill: parent
            visible: false
        }
        SignSheet {
            id: signSheet
            app: app
            anchors.fill: parent
            visible: false
            onDone: visible = false
        }
        MoreMenu {
            id: more
            app: app
            anchors.fill: parent
            visible: false
            onDone: visible = false
        }
        PasswordSheet {
            id: password
            app: app
            anchors.fill: parent
            visible: false
        }

        Connections {
            target: pdf
            function onOpened() {
                app.backToReading();
                pages.visible = false;
                if (pdf.status === "needsPassword" || pdf.status === "wrongPassword") {
                    password.visible = true;
                    password.ask(pdf.asked,
                                 pdf.status === "wrongPassword");
                } else {
                    password.visible = false;
                }
            }
        }

        Shortcut { sequences: [StandardKey.Open]; onActivated: openDialog.open() }
        Shortcut { sequences: [StandardKey.Find]; enabled: pdf.status === "open"; onActivated: app.openPages(true) }
        Shortcut { sequences: [StandardKey.Save]; enabled: pdf.changed; onActivated: app.report(pdf.save()) }
        Shortcut { sequences: [StandardKey.Copy]; enabled: app.selection !== null; onActivated: app.copySelection() }
        Shortcut { sequences: ["Escape"]; onActivated: { pages.visible = false; signSheet.visible = false; more.visible = false; app.backToReading(); } }
        Shortcut { sequences: [StandardKey.ZoomIn]; onActivated: view.zoomTo(view.zoom * 1.25) }
        Shortcut { sequences: [StandardKey.ZoomOut]; onActivated: view.zoomTo(view.zoom / 1.25) }
        Shortcut { sequences: ["Ctrl+0"]; onActivated: view.zoomTo(1) }
        Shortcut { sequences: ["PgDown", "Space"]; enabled: pdf.status === "open"; onActivated: view.show(pdf.page + 1) }
        Shortcut { sequences: ["PgUp", "Shift+Space"]; enabled: pdf.status === "open"; onActivated: view.show(pdf.page - 1) }
        Shortcut { sequences: ["Home"]; enabled: pdf.status === "open"; onActivated: view.show(0) }
        Shortcut { sequences: ["End"]; enabled: pdf.status === "open"; onActivated: view.show(pdf.pageCount - 1) }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open a PDF")
        nameFilters: [qsTr("PDF files (*.pdf *.PDF)")]
        onAccepted: pdf.open(selectedFile)
    }
    FileDialog {
        id: copyDialog
        title: qsTr("Save a copy")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: app.report(pdf.saveCopy(selectedFile))
    }
}
