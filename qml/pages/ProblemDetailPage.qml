import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
Page {
    id: page
    property var problem: app.detail
    property var displayedProblemId: null
    property bool hasUnsavedChanges: titleField.text !== problem.title || urlField.text !== problem.url ||
        difficulty.currentText !== problem.difficulty || tags.text !== problem.tags.join("; ") ||
        notes.text !== problem.notes || solution.text !== problem.solution
    signal back()
    padding: 24
    function syncFields() {
        titleField.text = problem.title; urlField.text = problem.url
        difficulty.currentIndex = difficulty.model.indexOf(problem.difficulty)
        tags.text = problem.tags.join("; "); notes.text = problem.notes; solution.text = problem.solution
        if (displayedProblemId !== problem.id) {
            displayedProblemId = problem.id
            Qt.callLater(detailScroll.scrollToTop)
        }
    }
    function save() { return app.saveProblem({title: titleField.text, url: urlField.text, difficulty: difficulty.currentText, tags: tags.text, notes: notes.text, solution: solution.text}) }
    Component.onCompleted: syncFields()
    Connections { target: app; function onDetailChanged() { page.syncFields() } }
    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            Button { text: "‹ Problems"; onClicked: page.back() }
            Label { textFormat: Text.PlainText; text: problem.id ? "Problem details" : "Add problem"; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
            Button { objectName: "saveProblemButton"; text: "Save  Ctrl+S"; highlighted: true; enabled: page.hasUnsavedChanges; onClicked: page.save() }
        }
        ScrollView {
            id: detailScroll
            objectName: "detailScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: detailForm.implicitHeight
            // Keep a stable gutter outside the form, including when the bar is idle.
            rightPadding: detailScrollBar.width + 10
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: detailScrollBar
                objectName: "detailScrollBar"
                parent: detailScroll
                x: detailScroll.width - width
                y: detailScroll.topPadding
                height: detailScroll.availableHeight
                policy: ScrollBar.AlwaysOn
                visible: detailScroll.contentHeight > detailScroll.availableHeight
            }
            function scrollToTop() {
                contentItem.cancelFlick()
                contentItem.contentY = 0
            }
            ColumnLayout {
                id: detailForm
                objectName: "detailForm"
                width: detailScroll.availableWidth
                height: implicitHeight
                spacing: 12
                Label { textFormat: Text.PlainText; text: "Title" }
                TextField { id: titleField; objectName: "problemTitle"; Layout.fillWidth: true; placeholderText: "Problem title" }
                Label { textFormat: Text.PlainText; text: "LeetCode URL" }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: urlField; objectName: "problemUrl"; Layout.fillWidth: true; placeholderText: "https://leetcode.com/problems/…" }
                    Button { text: "Open"; onClicked: app.openProblemUrl(urlField.text) }
                }
                Label { textFormat: Text.PlainText; text: "Difficulty" }
                ComboBox { id: difficulty; model: ["Easy", "Medium", "Hard", "Unknown"] }
                Label { textFormat: Text.PlainText; text: "Tags (separate with semicolons)" }
                TextField { id: tags; Layout.fillWidth: true; placeholderText: "Arrays; Hashing; Custom tag" }
                Label { textFormat: Text.PlainText; text: "Notes" }
                NoteArea { id: notes; objectName: "detailNotes"; Layout.fillWidth: true; Layout.minimumHeight: 120; wrapMode: TextEdit.Wrap; selectByMouse: true; textFormat: TextEdit.PlainText }
                Label { textFormat: Text.PlainText; text: "Canonical solution" }
                NoteArea { id: solution; objectName: "detailSolution"; Layout.fillWidth: true; Layout.minimumHeight: 220; font.family: "monospace"; wrapMode: TextEdit.Wrap; selectByMouse: true; textFormat: TextEdit.PlainText }
                Label { textFormat: Text.PlainText; visible: problem.id > 0; text: "Review progress"; font.pixelSize: 20; font.bold: true; Layout.topMargin: 16 }
                Label { textFormat: Text.PlainText; visible: problem.id > 0; text: problem.learned ? "Learned  ·  Stage " + (problem.stage + 1) + "  ·  PASS " + problem.passes + "  /  FAIL " + problem.failures : "Not learned" }
                Label { textFormat: Text.PlainText; visible: problem.id > 0; text: "First learned: " + (problem.firstLearned || "—") + "   ·   Last review: " + (problem.lastReview || "—") + "   ·   Next review: " + (problem.nextReview || "—"); wrapMode: Text.Wrap; Layout.fillWidth: true }
                Label { textFormat: Text.PlainText; visible: problem.id > 0; text: "Created: " + problem.createdAt + "   ·   Updated: " + problem.updatedAt; wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.7 }
                NoteArea { visible: problem.id > 0; text: problem.schedulerState; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; textFormat: TextEdit.PlainText }
                RowLayout {
                    visible: problem.id > 0
                    Button { text: "Reset progress / mark unlearned"; onClicked: resetDialog.open() }
                    Button { text: "Delete problem"; onClicked: deleteDialog.open() }
                }
                Label { textFormat: Text.PlainText; visible: problem.id > 0; text: "Review history (" + app.history.length + ")"; font.pixelSize: 20; font.bold: true; Layout.topMargin: 16 }
                Label { textFormat: Text.PlainText; visible: problem.id > 0 && app.history.length === 0; text: "No review attempts yet. Initial learning is not a PASS."; opacity: 0.7 }
                Repeater {
                    model: app.history
                    Frame {
                        required property var modelData
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            Label { textFormat: Text.PlainText; text: modelData.result + "  ·  " + modelData.timestamp; font.bold: true }
                            Label { textFormat: Text.PlainText; text: "Due " + modelData.previous_due + "  →  " + modelData.resulting_due; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            NoteArea { text: "Before: " + modelData.state_before + "\nAfter: " + modelData.state_after; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; textFormat: TextEdit.PlainText }
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: resetDialog
        title: "Reset review progress?"
        anchors.centerIn: parent
        width: Math.min(480, page.width - 40)
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label { textFormat: Text.PlainText; width: parent.width; text: "This marks the problem unlearned and clears its current schedule and counters. Historical attempts and today's learning count remain. Unsaved edits will be discarded."; wrapMode: Text.Wrap }
        onAccepted: app.resetProblem(problem.id)
    }
    Dialog {
        id: deleteDialog
        title: "Delete this problem?"
        anchors.centerIn: parent
        width: Math.min(480, page.width - 40)
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label { textFormat: Text.PlainText; width: parent.width; text: "Remove this problem from the library and daily queues? Historical attempts are retained in the database. Unsaved edits will be discarded."; wrapMode: Text.Wrap }
        onAccepted: { if (app.deleteProblem(problem.id)) page.back() }
    }
    Shortcut { sequence: "Ctrl+S"; enabled: page.visible; onActivated: page.save() }
    Shortcut { sequence: "Escape"; enabled: page.visible; onActivated: page.back() }
}
