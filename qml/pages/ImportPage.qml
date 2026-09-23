import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
Page {
    id: page
    property bool hasUnsavedChanges: false
    signal back()
    padding: 24
    ColumnLayout {
        anchors.fill: parent
        spacing: 16
        RowLayout {
            Button { text: "‹ Problems"; onClicked: page.back() }
            Label { textFormat: Text.PlainText; text: "Import CSV"; font.pixelSize: 26; font.bold: true; Layout.fillWidth: true }
            Button { text: "Choose CSV…"; onClicked: picker.open() }
        }
        Label { textFormat: Text.PlainText; text: "Required: title,url. Optional: difficulty,notes,solution,tags (or topics). Separate tags with semicolons. Use UTF-8; quoted fields may contain commas and newlines."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { textFormat: Text.PlainText; text: "Duplicates match URL or normalized title and will be skipped. Existing problems are never overwritten. Any invalid record blocks the entire import."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { textFormat: Text.PlainText; text: app.importSummary.ready + " ready  ·  " + app.importSummary.duplicates + " duplicates  ·  " + app.importSummary.errors.length + " errors"; font.bold: true }
        ListView {
            id: preview
            Layout.fillWidth: true; Layout.fillHeight: true
            model: app.importRows
            clip: true
            spacing: 6
            ScrollBar.vertical: ScrollBar {}
            delegate: Frame {
                required property var modelData
                width: preview.width
                height: content.implicitHeight + 24
                ColumnLayout {
                    id: content
                    anchors.fill: parent
                    Label { textFormat: Text.PlainText; text: "Record " + modelData.record + "  ·  " + modelData.title; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { textFormat: Text.PlainText; text: modelData.url; elide: Text.ElideRight; Layout.fillWidth: true; opacity: 0.7 }
                    Label { textFormat: Text.PlainText; text: modelData.status; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }
        }
        Button { objectName: "confirmImportButton"; text: "Import " + app.importSummary.ready + " problems"; highlighted: true; enabled: app.importSummary.canImport; onClicked: { if (app.commitImport()) page.back() } }
    }
    FileDialog { id: picker; title: "Choose problem CSV"; nameFilters: ["CSV files (*.csv)", "All files (*)"]; onAccepted: app.previewImport(selectedFile) }
}
