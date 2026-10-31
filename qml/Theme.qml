pragma Singleton
import QtQuick

// The suite's colour roles and sizes (Kadunce docs/ITASCA-VISUAL-LANGUAGE.md).
// The desk follows the colour scheme; the page is the document's own colour.
QtObject {
    id: theme

    readonly property SystemPalette system: SystemPalette { colorGroup: SystemPalette.Active }
    // The desktop's own light or dark setting where Qt reports one (6.5 and
    // later), otherwise how dark the scheme's window colour is.
    readonly property int scheme: Qt.styleHints.colorScheme !== undefined ? Qt.styleHints.colorScheme : 0
    readonly property bool dark: scheme === 2 ? true : scheme === 1 ? false : system.window.hslLightness < 0.5

    // The desk the paper lies on, and the chrome one step above it.
    readonly property color desk: dark ? "#141414" : "#ECECF0"
    readonly property color chrome: dark ? "#1C1C1C" : "#F8F8FF"
    readonly property color raised: dark ? "#242424" : "#E2E2EA"
    readonly property color divider: dark ? "#333333" : "#D6D6DE"
    readonly property color text: dark ? "#F8F8FF" : "#141414"
    readonly property color secondary: dark ? Qt.rgba(1, 1, 1, 0.66) : Qt.rgba(0.08, 0.08, 0.08, 0.66)
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.125) : Qt.rgba(0, 0, 0, 0.07)
    readonly property color selected: dark ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(0, 0, 0, 0.2)
    // The Plasma accent, for the cursor, the selection and the one big action.
    readonly property color accent: system.highlight
    readonly property color accentInk: "#102729"
    readonly property color error: "#FFB5A8"

    readonly property int paperRadius: 10
    readonly property int noteRadius: 12
    readonly property int control: 44
    readonly property int tight: 4
    readonly property int compact: 8
    readonly property int standard: 12
    readonly property int comfortable: 16
    readonly property int surface: 24

    readonly property int bodySize: 16
    readonly property int metaSize: 13

    readonly property int micro: 140
    readonly property int stateTime: 180
    readonly property int spatial: 250
}
