import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Frame {
    property string caption
    property string value
    implicitWidth: 160
    implicitHeight: 108
    ColumnLayout {
        anchors.fill: parent
        Label { textFormat: Text.PlainText; text: value; font.pixelSize: 30; font.bold: true }
        Label { textFormat: Text.PlainText; text: caption; opacity: 0.75; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
}
