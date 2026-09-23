import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ApplicationWindow {
    visible: true
    width: 640; height: 340
    minimumWidth: 480; minimumHeight: 300
    title: "LeetRepeat — unable to start"
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18
        Label { textFormat: Text.PlainText; text: "LeetRepeat could not start"; font.pixelSize: 24; font.bold: true }
        TextArea { text: startupError; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; Layout.fillHeight: true }
        Button { text: "Close"; onClicked: Qt.quit() }
    }
}
