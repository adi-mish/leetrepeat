import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
Page {
    id: page
    property bool hasUnsavedChanges: false
    signal startSession()
    signal openLibrary()
    padding: 28
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 24
            Label { textFormat: Text.PlainText; text: "Today"; font.pixelSize: 30; font.bold: true }
            Label { textFormat: Text.PlainText; text: Qt.formatDate(app.stats.date, "dddd, MMMM d"); opacity: 0.7 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                StatCard { caption: "New"; value: app.stats.newCount; Layout.fillWidth: true }
                StatCard { caption: "Reviews due"; value: app.stats.due; Layout.fillWidth: true }
                StatCard { caption: "Remaining"; value: app.stats.remaining; Layout.fillWidth: true }
            }
            Label { textFormat: Text.PlainText;
                text: app.stats.total === 0 ? "Add problems or import a CSV to begin." :
                      app.stats.remaining === 0 ? "You're done for today. Your next reviews will appear when due." :
                      "Reproduce each solution from memory. Mark PASS or FAIL and keep going."
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Button {
                objectName: "startSessionButton"
                text: "Start Session  ↵"
                highlighted: true
                enabled: app.stats.remaining > 0
                implicitHeight: 48
                onClicked: page.startSession()
            }
            Button { text: app.stats.total === 0 ? "Add or import problems" : "Browse problems"; onClicked: page.openLibrary() }
            Label { textFormat: Text.PlainText; text: "Your corpus"; font.pixelSize: 20; font.bold: true; Layout.topMargin: 20 }
            GridLayout {
                columns: page.width > 1000 ? 4 : 2
                Layout.fillWidth: true
                columnSpacing: 16; rowSpacing: 16
                StatCard { caption: "Total problems"; value: app.stats.total; Layout.fillWidth: true }
                StatCard { caption: "Learned"; value: app.stats.learned; Layout.fillWidth: true }
                StatCard { caption: "Unlearned"; value: app.stats.unlearned; Layout.fillWidth: true }
                StatCard { caption: "Overdue (included above)"; value: app.stats.overdue; Layout.fillWidth: true }
            }
            Label { textFormat: Text.PlainText; text: "Completed today: " + app.stats.completedToday + "  ·  Learned: " + app.stats.learnedToday + "  ·  PASS: " + app.stats.passesToday + "  ·  FAIL: " + app.stats.failuresToday; wrapMode: Text.Wrap; Layout.fillWidth: true }
        }
    }
    Shortcut { sequence: "Return"; enabled: page.visible && app.stats.remaining > 0; onActivated: page.startSession() }
}
