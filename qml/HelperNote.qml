import QtQuick
import QtQuick.Controls.Basic
import co.goodinput.office

// A helper's change, waiting for Keep or Undo, with who made it; while Codex
// works, what it is doing and a way to stop it; when it couldn't, why; and,
// without a Codex sign-in, the way to Codex's own.
Rectangle {
    id: note

    property SlideHelpers helpers
    // Asking for a Codex sign-in, after a Codex helper was tapped without one.
    property bool signingIn: false

    readonly property bool working: helpers.working.length > 0
    readonly property bool waiting: Object.keys(helpers.result).length > 0
    readonly property bool trouble: helpers.problem.length > 0
    readonly property bool showing: signingIn || working || waiting || trouble

    signal closed()

    visible: showing
    color: Theme.chrome
    radius: Theme.noteRadius
    implicitWidth: 600
    implicitHeight: Math.max(64, content.implicitHeight + 2 * Theme.standard)

    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 8
        anchors.bottomMargin: -8
        radius: parent.radius
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.35 : 0.12)
    }

    readonly property string headline: {
        if (signingIn)
            return qsTr("Fit this slide and Make slides use Codex")
        if (helpers.working === "fit")
            return qsTr("Codex is fitting this slide…")
        if (helpers.working === "make")
            return qsTr("Codex is making your slides…")
        if (helpers.working === "apply")
            return qsTr("Putting them in your deck…")
        if (helpers.working === "tidy")
            return qsTr("Tidying the slide…")
        if (trouble)
            return helpers.problem
        return helpers.result.line || ""
    }
    readonly property string detail: {
        if (signingIn)
            return qsTr("Sign in to Codex once, with your ChatGPT account. Tidy the look works without it.")
        if (working)
            return helpers.working === "fit" || helpers.working === "make" ? qsTr("Only this slide or your notes are sent.") : ""
        if (trouble)
            return ""
        return helpers.result.who === "codex" ? qsTr("Made by Codex") : qsTr("Done by Slides itself")
    }

    Row {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: Theme.standard
        spacing: Theme.standard

        Column {
            width: content.width - buttons.width - content.spacing
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text { width: parent.width; text: note.headline; color: Theme.text; font.pixelSize: 15; wrapMode: Text.Wrap }
            Text { width: parent.width; visible: text.length > 0; text: note.detail; color: Theme.secondary; font.pixelSize: Theme.metaSize; wrapMode: Text.Wrap }
        }

        Row {
            id: buttons
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            Pill {
                visible: note.signingIn
                raised: true
                label: qsTr("Not now")
                onClicked: { note.signingIn = false; note.closed() }
            }
            Pill {
                visible: note.signingIn
                accent: true
                label: qsTr("Sign in")
                onClicked: { note.helpers.signIn(); note.signingIn = false; note.closed() }
            }
            Pill {
                visible: !note.signingIn && (note.helpers.working === "fit" || note.helpers.working === "make")
                raised: true
                label: qsTr("Stop")
                onClicked: note.helpers.cancel()
            }
            Pill {
                visible: !note.signingIn && !note.working && note.trouble
                raised: true
                label: qsTr("OK")
                onClicked: { note.helpers.dismiss(); note.closed() }
            }
            Pill {
                visible: !note.signingIn && !note.working && !note.trouble && note.waiting
                raised: true
                label: qsTr("Undo")
                onClicked: { note.helpers.undo(); note.closed() }
            }
            Pill {
                visible: !note.signingIn && !note.working && !note.trouble && note.waiting
                accent: true
                label: note.helpers.result.many ? qsTr("Keep all") : qsTr("Keep")
                onClicked: { note.helpers.keep(); note.closed() }
            }
        }
    }
}
