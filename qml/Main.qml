import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "pages"
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    visible: true
    width: 1120; height: 820
    minimumWidth: 800; minimumHeight: 640
    title: "LeetRepeat"
    property string currentPage: "today"
    property var pendingAction: null
    property bool closingApproved: false
    function guarded(action) {
        if (stack.currentItem && stack.currentItem.hasUnsavedChanges) { pendingAction = action; discardDialog.open() }
        else action()
    }
    function navigate(page) { guarded(function() { showPage(page) }) }
    function showPage(page) {
        if (currentPage === "review") app.review.end()
        currentPage = page
        app.refresh()
        var component = page === "today" ? todayPage : page === "library" ? libraryPage : page === "settings" ? settingsPage : page === "detail" ? detailPage : page === "import" ? importPage : reviewPage
        stack.replace(component)
    }
    onClosing: function(close) {
        if (!closingApproved && stack.currentItem && stack.currentItem.hasUnsavedChanges) {
            close.accepted = false
            guarded(function() { closingApproved = true; window.close() })
        }
    }
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20
            Label { textFormat: Text.PlainText; text: "LeetRepeat"; font.bold: true; font.pixelSize: 20; Layout.rightMargin: 24 }
            ToolButton { text: "Today"; checked: window.currentPage === "today" || window.currentPage === "review"; onClicked: window.navigate("today") }
            ToolButton { text: "Problems"; checked: ["library", "detail", "import"].indexOf(window.currentPage) >= 0; onClicked: window.navigate("library") }
            ToolButton { text: "Settings"; checked: window.currentPage === "settings"; onClicked: window.navigate("settings") }
            Item { Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: app.stats.remaining + " remaining today"; opacity: 0.7 }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Frame {
            visible: app.message.length > 0
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                Label { textFormat: Text.PlainText; text: (app.messageIsError ? "Error: " : "") + app.message; wrapMode: Text.Wrap; Layout.fillWidth: true; font.bold: app.messageIsError }
                ToolButton { text: "Dismiss"; onClicked: app.clearMessage() }
            }
        }
        StackView { id: stack; objectName: "pageStack"; Layout.fillWidth: true; Layout.fillHeight: true; initialItem: todayPage; replaceEnter: Transition {} replaceExit: Transition {} }
    }
    Component { id: todayPage; TodayPage { onStartSession: { if (app.review.start()) window.showPage("review") } onOpenLibrary: window.navigate("library") } }
    Component { id: reviewPage; ReviewPage { onBack: window.navigate("today") } }
    Component {
        id: libraryPage
        ProblemLibraryPage {
            onEditProblem: function(problemId) { if (app.selectProblem(problemId)) window.showPage("detail") }
            onAddProblem: { app.newProblem(); window.showPage("detail") }
            onImportProblems: window.showPage("import")
            onExportProblems: exportDialog.open()
        }
    }
    Component { id: detailPage; ProblemDetailPage { onBack: window.navigate("library") } }
    Component { id: importPage; ImportPage { onBack: window.navigate("library") } }
    Component { id: settingsPage; SettingsPage { onBackupRequested: backupDialog.open() } }
    Dialog {
        id: discardDialog
        title: "Discard unsaved edits?"
        anchors.centerIn: parent
        width: 420
        modal: true
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label { textFormat: Text.PlainText; width: parent.width; text: "Your edits have not been saved. Discard them to continue, or cancel and save first."; wrapMode: Text.Wrap }
        onDiscarded: { var action = window.pendingAction; window.pendingAction = null; if (action) action() }
        onRejected: window.pendingAction = null
    }
    FileDialog { id: exportDialog; title: "Export problems"; fileMode: FileDialog.SaveFile; defaultSuffix: "csv"; nameFilters: ["CSV files (*.csv)"]; onAccepted: app.exportCsv(selectedFile) }
    FileDialog { id: backupDialog; title: "Back up database (new filename)"; fileMode: FileDialog.SaveFile; defaultSuffix: "sqlite"; nameFilters: ["SQLite databases (*.sqlite)"]; onAccepted: app.backup(selectedFile) }
}
