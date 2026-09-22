import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Page {
    id: page
    property bool hasUnsavedChanges: daily.value !== app.settings.newPerDay || intervals.text !== app.settings.intervals ||
        shuffle.checked !== app.settings.shuffle || jitter.checked !== app.settings.jitter
    signal backupRequested()
    padding: 28
    function syncSettings() { daily.value = app.settings.newPerDay; intervals.text = app.settings.intervals; shuffle.checked = app.settings.shuffle; jitter.checked = app.settings.jitter }
    Component.onCompleted: syncSettings()
    Connections { target: app; function onRefreshed() { if (!page.hasUnsavedChanges) page.syncSettings() } }
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 18
            Label { text: "Settings"; font.pixelSize: 28; font.bold: true }
            Label { text: "New problems per day"; font.bold: true }
            SpinBox { id: daily; from: 0; to: 1000; editable: true }
            Label { text: "Set to 0 to focus only on reviews. The default is 3."; opacity: 0.7 }
            Label { text: "Review intervals (days)"; font.bold: true }
            TextField { id: intervals; Layout.fillWidth: true; placeholderText: "1,2,4,7,14,30,60,120,240" }
            Label { text: "Use 1–32 strictly increasing positive integers, up to 36500 days. Initial learning and FAIL always schedule tomorrow. Changes affect future scheduling, leaving existing due dates intact."; wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.7 }
            CheckBox { id: shuffle; text: "Shuffle review order" }
            CheckBox { id: jitter; text: "Vary long intervals slightly (±10%, starting at 14 days)" }
            Button { text: "Save settings"; highlighted: true; enabled: page.hasUnsavedChanges; onClicked: { if (app.saveSettings(daily.value, intervals.text, shuffle.checked, jitter.checked)) page.syncSettings() } }
            Label { text: "Your data"; font.pixelSize: 22; font.bold: true; Layout.topMargin: 20 }
            Label { text: "Database location"; font.bold: true }
            TextArea { text: app.databasePath; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; textFormat: TextEdit.PlainText }
            RowLayout {
                Button { text: "Open data folder"; onClicked: app.openDatabaseFolder() }
                Button { text: "Back up database…"; onClicked: page.backupRequested() }
            }
            Label { text: "Backups include problems, notes, tags, settings, and full attempt history. Choose a new filename for each backup. To restore, close LeetRepeat and replace its database with a backup (see README). CSV exports are available in Problems."; wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.7 }
        }
    }
}
