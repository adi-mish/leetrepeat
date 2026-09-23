import QtQuick

Palette {
    id: theme
    property bool darkMode: false
    property color foreground: darkMode ? "#e6e9ef" : "#20242b"
    property color muted: darkMode ? "#929baa" : "#697382"

    window: darkMode ? "#20242b" : "#f5f6f8"
    windowText: foreground
    base: darkMode ? "#171b21" : "#ffffff"
    alternateBase: darkMode ? "#282e37" : "#edf0f4"
    text: foreground
    button: darkMode ? "#303741" : "#e8ecf1"
    buttonText: foreground
    brightText: "#ffffff"
    placeholderText: muted
    highlight: darkMode ? "#8ab4f8" : "#255dad"
    highlightedText: darkMode ? "#141a23" : "#ffffff"
    light: darkMode ? "#46505e" : "#ffffff"
    midlight: darkMode ? "#39424f" : "#edf0f4"
    mid: darkMode ? "#4d5867" : "#b5beca"
    dark: darkMode ? "#14181e" : "#8994a3"
    shadow: darkMode ? "#0c1015" : "#657181"
    toolTipBase: darkMode ? "#303741" : "#ffffff"
    toolTipText: foreground
    link: darkMode ? "#8ab4f8" : "#255dad"
    linkVisited: darkMode ? "#c4a7e7" : "#6e429c"
    disabled.windowText: muted
    disabled.text: muted
    disabled.buttonText: muted
    disabled.highlight: darkMode ? "#414d5d" : "#c5cdd8"
    disabled.highlightedText: muted
}
