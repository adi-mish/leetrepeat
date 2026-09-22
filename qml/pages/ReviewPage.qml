import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Page {
    id: page
    property var review: app.review
    property var problem: review.current
    property bool hasUnsavedChanges: review.active && !problem.learned &&
                                         (notes.text !== problem.notes || solution.text !== problem.solution)
    property bool reviewing: review.active && problem.learned
    signal back()
    padding: 28
    function syncNotes() { notes.text = problem.notes || ""; solution.text = problem.solution || "" }
    Component.onCompleted: { syncNotes(); forceActiveFocus() }
    Connections { target: page.review; function onChanged() { page.syncNotes() } }
    ColumnLayout {
        anchors.fill: parent
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Button { text: "‹ Today  Esc"; onClicked: page.back() }
            Item { Layout.fillWidth: true }
            Label { text: review.completed + " completed  ·  " + review.remaining + " remaining" }
        }
        Label { visible: !review.active; text: "Session complete"; font.pixelSize: 30; font.bold: true }
        Label { visible: !review.active; text: "Your progress is saved. Come back when the next reviews are due."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button { visible: !review.active; text: "Back to Today"; highlighted: true; onClicked: page.back() }
        Label { visible: review.active; text: problem.learned ? "REVIEW" : "NEW PROBLEM"; opacity: 0.7; font.letterSpacing: 2 }
        Label { visible: review.active; text: problem.title; font.pixelSize: 28; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { visible: review.active; text: problem.difficulty + (problem.tags.length ? "  ·  " + problem.tags.join(" · ") : ""); opacity: 0.8; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button { visible: review.active; text: "Open on LeetCode  O"; onClicked: review.openUrl() }
        Label { visible: review.active; text: problem.learned ? "Reproduce the solution from memory." : "Study or solve this problem, then save what you want to recall."; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Button { visible: page.reviewing; text: review.revealed ? "Hide Notes / Solution  Space" : "Reveal Notes / Solution  Space"; onClicked: review.revealed = !review.revealed }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth
            visible: review.active
            ColumnLayout {
                width: parent.width
                spacing: 12
                Label { visible: !problem.learned || review.revealed; text: "Notes"; font.bold: true }
                TextArea {
                    id: notes
                    objectName: "studyNotes"
                    visible: !problem.learned || review.revealed
                    Layout.fillWidth: true
                    Layout.minimumHeight: 110
                    readOnly: problem.learned
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    placeholderText: problem.learned ? "No notes saved." : "Algorithm, invariants, pitfalls…"
                    textFormat: TextEdit.PlainText
                }
                Label { visible: !problem.learned || review.revealed; text: "Canonical solution"; font.bold: true }
                TextArea {
                    id: solution
                    objectName: "studySolution"
                    visible: !problem.learned || review.revealed
                    Layout.fillWidth: true
                    Layout.minimumHeight: 180
                    readOnly: problem.learned
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    font.family: "monospace"
                    placeholderText: problem.learned ? "No solution saved." : "Paste your complete solution here…"
                    textFormat: TextEdit.PlainText
                }
            }
        }
        RowLayout {
            visible: page.reviewing
            Layout.fillWidth: true
            spacing: 20
            Button { objectName: "failButton"; text: "FAIL  F"; Layout.fillWidth: true; implicitHeight: 56; onClicked: review.grade(false) }
            Button { objectName: "passButton"; text: "PASS  P"; highlighted: true; Layout.fillWidth: true; implicitHeight: 56; onClicked: review.grade(true) }
        }
        RowLayout {
            visible: review.active && !problem.learned
            Button { text: "Save notes"; enabled: page.hasUnsavedChanges; onClicked: review.saveStudy(notes.text, solution.text) }
            Button { objectName: "markLearnedButton"; text: "Mark Learned"; highlighted: true; onClicked: review.markLearned(notes.text, solution.text) }
            Label { text: "First review: tomorrow"; opacity: 0.7 }
        }
    }
    Shortcut { sequence: "P"; autoRepeat: false; enabled: page.visible && page.reviewing; onActivated: review.grade(true) }
    Shortcut { sequence: "F"; autoRepeat: false; enabled: page.visible && page.reviewing; onActivated: review.grade(false) }
    Shortcut { sequence: "Space"; autoRepeat: false; enabled: page.visible && page.reviewing; onActivated: review.revealed = !review.revealed }
    Shortcut { sequence: "O"; autoRepeat: false; enabled: page.visible && page.reviewing; onActivated: review.openUrl() }
    Shortcut { sequence: "Escape"; enabled: page.visible; onActivated: page.back() }
    Shortcut { sequence: "Return"; enabled: page.visible && !review.active; onActivated: page.back() }
}
