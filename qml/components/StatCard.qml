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
        Label { text: value; font.pixelSize: 30; font.bold: true }
        Label { text: caption; opacity: 0.75; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
}
