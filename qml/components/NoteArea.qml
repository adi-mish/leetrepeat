import QtQuick
import QtQuick.Controls
TextArea {
    id: editor
    padding: 10
    selectByMouse: true
    wrapMode: TextEdit.Wrap
    textFormat: TextEdit.PlainText
    background: Rectangle {
        color: editor.palette.base
        border.color: editor.activeFocus ? editor.palette.highlight : editor.palette.mid
        radius: 3
    }
}
