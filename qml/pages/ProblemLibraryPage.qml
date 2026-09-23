import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Page {
    id: page
    property bool hasUnsavedChanges: false
    signal editProblem(var problemId)
    signal addProblem()
    signal importProblems()
    signal exportProblems()
    padding: 24
    function applyFilters() { app.problems.filter(search.text, status.currentText, difficulty.currentText, tags.text, sorting.currentText) }
    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        RowLayout {
            Label { textFormat: Text.PlainText; text: "Problems"; font.pixelSize: 28; font.bold: true; Layout.fillWidth: true }
            Button { text: "Add problem"; onClicked: page.addProblem() }
            Button { text: "Import CSV"; onClicked: page.importProblems() }
            Button { text: "Export CSV"; onClicked: page.exportProblems() }
        }
        TextField {
            id: search
            objectName: "librarySearch"
            Layout.fillWidth: true
            placeholderText: "Search by title…"
            placeholderTextColor: palette.text
            onTextChanged: page.applyFilters()
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox { id: status; model: ["All", "Not learned", "Learned", "Due", "Overdue"]; onActivated: page.applyFilters() }
            ComboBox { id: difficulty; model: ["All difficulties", "Easy", "Medium", "Hard", "Unknown"]; onActivated: page.applyFilters() }
            ComboBox { id: sorting; model: ["Title", "Next review", "Most failures", "Stage", "Difficulty"]; onActivated: page.applyFilters(); Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: tags
                Layout.fillWidth: true
                placeholderText: "Filter by tags (all must match; separate with ;)"
                placeholderTextColor: palette.text
                onTextChanged: page.applyFilters()
            }
            ComboBox {
                id: tagPicker
                model: ["Add tag filter…"].concat(app.tags)
                onActivated: { if (currentIndex > 0) tags.text += (tags.text.trim().length ? "; " : "") + currentText; currentIndex = 0 }
            }
        }
        Label { textFormat: Text.PlainText; text: app.problems.count + " problems"; opacity: 0.7 }
        ListView {
            id: list
            objectName: "problemList"
            Layout.fillWidth: true; Layout.fillHeight: true
            model: app.problems
            clip: true
            spacing: 6
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property var problem
                required property int index
                width: list.width
                height: 94
                highlighted: ListView.isCurrentItem
                onClicked: { list.currentIndex = index; page.editProblem(problem.id) }
                contentItem: ColumnLayout {
                    spacing: 4
                    RowLayout {
                        Label { textFormat: Text.PlainText; text: problem.title; font.bold: true; font.pixelSize: 16; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { textFormat: Text.PlainText; text: problem.difficulty; opacity: 0.8 }
                    }
                    Label { textFormat: Text.PlainText;
                        text: problem.learned ? "Learned  ·  Stage " + (problem.stage + 1) + "  ·  Next: " + problem.nextReview + "  ·  PASS " + problem.passes + "  /  FAIL " + problem.failures : "Not learned"
                        elide: Text.ElideRight; Layout.fillWidth: true; opacity: 0.8
                    }
                    Label { textFormat: Text.PlainText; text: problem.tags.join(" · "); elide: Text.ElideRight; Layout.fillWidth: true; opacity: 0.6 }
                }
                Keys.onReturnPressed: page.editProblem(problem.id)
            }
            Label { textFormat: Text.PlainText; anchors.centerIn: parent; visible: list.count === 0; text: "No matching problems."; opacity: 0.7 }
        }
    }
    Component.onCompleted: applyFilters()
}
