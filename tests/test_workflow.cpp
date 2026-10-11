#include <QtTest>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QLockFile>
#include <QProcess>
#include <QScopeGuard>
#include <QSqlError>
#include <QSysInfo>
#include <QtEndian>
#include "controllers/AppController.h"
#include "scheduling/FixedIntervalScheduler.h"
using namespace lr;
class WorkflowTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Fusion");
        qmlRegisterUncreatableType<ProblemListModel>("LeetRepeat",1,0,"ProblemListModel","Application-owned");
        qmlRegisterUncreatableType<ReviewController>("LeetRepeat",1,0,"ReviewController","Application-owned");
    }
    void fullWorkflow() {
        QTemporaryDir dir; Database db(dir.filePath("workflow.sqlite")); Repository repo(db);
        Problem a; a.title="Review problem"; a.url="https://leetcode.com/problems/review"; a.notes="Hidden note"; a.solution="Hidden solution"; a.tags={"Arrays","Hashing"};
        auto reviewId=repo.add(a);
        FixedIntervalScheduler scheduler;
        repo.learn(reviewId,scheduler,QDateTime::currentDateTime().addDays(-10),3);
        Problem failed=a; failed.title="Failed review"; failed.url="https://leetcode.com/problems/failed";
        auto failedId=repo.add(failed); repo.learn(failedId,scheduler,QDateTime::currentDateTime().addDays(-2),3);
        Settings settings; settings.shuffle=false; repo.saveSettings(settings);
        Problem b; b.title="New problem"; b.url="https://leetcode.com/problems/new"; auto newId=repo.add(b);
        AppController controller(repo);
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors){for (const auto &e : errors) warnings.append(e.toString());});
        engine.rootContext()->setContextProperty("app",&controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(),qPrintable(warnings.join('\n')));
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        auto show=[&](const char *page){ return QMetaObject::invokeMethod(window,"showPage",Q_ARG(QVariant,QString::fromUtf8(page))); };
        auto stack=window->findChild<QObject *>("pageStack"); QVERIFY(stack);
        auto current=[&] { return stack->property("currentItem").value<QObject *>(); };
        auto screenshot=[&](const QString &name) {
            auto path=qEnvironmentVariable("LEETREPEAT_TEST_SCREENSHOT");
            if (!path.isEmpty()) window->grabWindow().save(path+"-"+name+".png");
        };
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QVERIFY(controller.review()->start()); QVERIFY(show("review")); QTest::qWait(100);
        QVERIFY(controller.review()->active()); QCOMPARE(controller.review()->current()["id"].toLongLong(),reviewId);
        QVERIFY(!controller.review()->revealed());
        screenshot("review");
        auto notes=window->findChild<QQuickItem *>("studyNotes"); QVERIFY(notes); QVERIFY(!notes->isVisible());
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(controller.review()->revealed()); QVERIFY(notes->isVisible());
        QTest::keyClick(window,Qt::Key_P); QTRY_COMPARE(repo.history(reviewId).size(),1);
        QCOMPARE(controller.review()->current()["id"].toLongLong(),failedId);
        QTest::keyClick(window,Qt::Key_F); QTRY_COMPARE(repo.history(failedId).size(),1);
        QCOMPARE(repo.problem(failedId).state.stage,0);
        QCOMPARE(repo.problem(failedId).nextReview,QDate::currentDate().addDays(1));
        QCOMPARE(controller.review()->current()["id"].toLongLong(),newId);
        QVERIFY(!controller.review()->revealed());
        notes->setProperty("text","My study notes");
        auto solution=window->findChild<QQuickItem *>("studySolution"); QVERIFY(solution); solution->setProperty("text","My solution");
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        screenshot("study");
        // Letters typed during initial study must never grade a problem.
        notes->forceActiveFocus(); QTest::keyClick(window,Qt::Key_P);
        QCOMPARE(repo.history(newId).size(),0);
        auto mark=window->findChild<QObject *>("markLearnedButton"); QVERIFY(mark); QVERIFY(QMetaObject::invokeMethod(mark,"clicked"));
        QVERIFY(repo.problem(newId).learned); QCOMPARE(repo.problem(newId).solution,QString("My solution"));
        QCOMPARE(repo.history(newId).size(),0); QCOMPARE(repo.problem(newId).nextReview,QDate::currentDate().addDays(1));
        QVERIFY(!controller.review()->active()); QCOMPARE(controller.stats()["remaining"].toInt(),0);
        QVERIFY(show("today")); QVERIFY(show("library")); QTest::qWait(50);
        controller.problems()->filter("Review problem","Learned","All difficulties","arrays;HASHING","Title"); QCOMPARE(controller.problems()->rowCount(),1);
        controller.problems()->filter("Review problem","All","All difficulties","trees","Title"); QCOMPARE(controller.problems()->rowCount(),0);
        QVERIFY(controller.selectProblem(reviewId)); QVERIFY(show("detail")); QTest::qWait(50);
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        screenshot("detail");
        auto title=window->findChild<QObject *>("problemTitle"); QVERIFY(title); title->setProperty("text","Renamed problem");
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        QVERIFY(QMetaObject::invokeMethod(window,"navigate",Q_ARG(QVariant,QString("today"))));
        QCOMPARE(window->property("currentPage").toString(),QString("detail")); // Unsaved navigation blocked.
        // Close the confirmation without discarding, then save.
        QTest::keyClick(window,Qt::Key_Escape);
        auto save=window->findChild<QObject *>("saveProblemButton"); QVERIFY(save); QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QCOMPARE(repo.problem(reviewId).title,QString("Renamed problem")); QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QVERIFY(show("settings")); QTest::qWait(50); QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        screenshot("settings");
        QVERIFY(!controller.saveSettings(3,"1,2,2",true,false)); QVERIFY(controller.messageIsError());
        QVERIFY(controller.saveSettings(5,"1,3,7",false,true)); QCOMPARE(repo.settings().newPerDay,5);
        QVERIFY(show("import")); QTest::qWait(50);
        QVERIFY(show("today")); QTest::qWait(100);
        if (!qEnvironmentVariableIsEmpty("LEETREPEAT_TEST_SCREENSHOT")) window->grabWindow().save(qEnvironmentVariable("LEETREPEAT_TEST_SCREENSHOT"));
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void settingsUnsavedNavigation() {
        QTemporaryDir dir; Database db(dir.filePath("settings.sqlite")); Repository repo(db);
        AppController controller(repo);
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("app",&controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        auto stack=window->findChild<QObject *>("pageStack"); QVERIFY(stack);
        auto current=[&] { return stack->property("currentItem").value<QObject *>(); };
        auto click=[&](QQuickItem *item) {
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                              item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
        };
        auto settingsTab=window->findChild<QQuickItem *>("settingsTab"); QVERIFY(settingsTab);
        auto todayTab=window->findChild<QQuickItem *>("todayTab"); QVERIFY(todayTab);
        auto dialog=window->findChild<QObject *>("discardEditsDialog"); QVERIFY(dialog);
        click(settingsTab);
        QTRY_COMPARE(window->property("currentPage").toString(),QString("settings"));
        auto daily=current()->findChild<QQuickItem *>("dailyTarget"); QVERIFY(daily);
        auto editor=daily->property("contentItem").value<QQuickItem *>(); QVERIFY(editor);
        auto save=current()->findChild<QQuickItem *>("saveSettingsButton"); QVERIFY(save);
        auto intervals=current()->findChild<QQuickItem *>("reviewIntervals"); QVERIFY(intervals);
        auto type=[&](QQuickItem *field,const QString &text) {
            field->forceActiveFocus(); QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
            for (QChar ch : text) QTest::keyClick(window,ch.toLatin1());
        };
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        editor->forceActiveFocus(); QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        QTest::keyClick(window,Qt::Key_Backspace);
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        QVERIFY(!save->isEnabled());
        type(editor,"3");
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        // Typing has to count before SpinBox commits on Enter or focus loss.
        type(editor,"8");
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        QVERIFY(save->isEnabled());
        click(todayTab);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(window->property("currentPage").toString(),QString("settings"));
        QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(repo.settings().newPerDay,3);
        // Re-selecting the current tab is not navigation and must retain edits.
        click(settingsTab);
        QVERIFY(!dialog->property("visible").toBool());
        QCOMPARE(current()->findChild<QQuickItem *>("dailyTarget"),daily);
        // Save directly from the numeric editor, without first pressing Enter.
        type(editor,"9"); click(save);
        QTRY_COMPARE(repo.settings().newPerDay,9);
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        click(todayTab);
        QTRY_COMPARE(window->property("currentPage").toString(),QString("today"));
        QVERIFY(!dialog->property("visible").toBool());
        click(settingsTab);
        QTRY_COMPARE(window->property("currentPage").toString(),QString("settings"));
        intervals=current()->findChild<QQuickItem *>("reviewIntervals"); QVERIFY(intervals);
        save=current()->findChild<QQuickItem *>("saveSettingsButton"); QVERIFY(save);
        // Invalid saves stay dirty; normalized successful saves become clean.
        type(intervals,"1,2,2"); click(save);
        QVERIFY(controller.messageIsError());
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        click(todayTab); QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        type(intervals,"1, 3, 7"); click(save);
        QCOMPARE(repo.settings().intervals,QVector<int>({1,3,7}));
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        // Refreshing saved settings updates a clean form; appearance auto-save
        // and refresh must not erase other pending edits.
        QVERIFY(controller.saveSettings(4,"1,4,8",false,true));
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QCOMPARE(intervals->property("text").toString(),QString("1,4,8"));
        type(intervals,"1,5,10");
        QVERIFY(controller.setAppearance("Dark"));
        QVERIFY(controller.refresh());
        QCOMPARE(intervals->property("text").toString(),QString("1,5,10"));
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        // Discard really leaves and reloads saved values on return.
        click(todayTab); QTRY_VERIFY(dialog->property("visible").toBool());
        auto footer=dialog->property("footer").value<QObject *>(); QVERIFY(footer);
        QQuickItem *discard=nullptr;
        for (auto item : footer->findChildren<QQuickItem *>())
            if (item->property("text").toString()=="Discard" && item->property("down").isValid()) discard=item;
        QVERIFY(discard); click(discard);
        QTRY_COMPARE(window->property("currentPage").toString(),QString("today"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        click(settingsTab);
        QTRY_COMPARE(window->property("currentPage").toString(),QString("settings"));
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QCOMPARE(current()->findChild<QObject *>("reviewIntervals")->property("text").toString(),QString("1,4,8"));
        // Reverting a toggle or changing only the auto-saved theme is clean.
        auto shuffle=current()->findChild<QQuickItem *>("shuffleReviews"); QVERIFY(shuffle);
        auto jitter=current()->findChild<QQuickItem *>("jitterIntervals"); QVERIFY(jitter);
        for (auto toggle : {shuffle,jitter}) {
            click(toggle); QVERIFY(current()->property("hasUnsavedChanges").toBool());
            click(toggle); QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        }
        QVERIFY(controller.setAppearance("Light"));
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        // Closing the window uses the same guard and cancel keeps the draft.
        intervals=current()->findChild<QQuickItem *>("reviewIntervals"); QVERIFY(intervals);
        type(intervals,"1,6,12");
        QVERIFY(!window->close());
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyClick(window,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(window->isVisible());
        QCOMPARE(intervals->property("text").toString(),QString("1,6,12"));
        save=current()->findChild<QQuickItem *>("saveSettingsButton"); QVERIFY(save);
        click(save);
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QVERIFY(window->close());
        QVERIFY(!dialog->property("visible").toBool());
        QCOMPARE(warnings.count(),0);
    }
    void detailScrolling() {
        QTemporaryDir dir; Database db(dir.filePath("scrolling.sqlite")); Repository repo(db);
        Problem a; a.title="Long solution"; a.url="https://leetcode.com/problems/long";
        a.notes=QString("Remember this invariant.\n").repeated(30);
        a.solution=QString("    // reproduce the implementation from memory\n").repeated(100);
        const auto first=repo.add(a);
        Problem b; b.title="Another problem"; b.url="https://leetcode.com/problems/another";
        const auto second=repo.add(b);
        AppController controller(repo);
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("app",&controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->resize(800,640); window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        QVERIFY(controller.selectProblem(first));
        QVERIFY(QMetaObject::invokeMethod(window,"showPage",Q_ARG(QVariant,QString("detail"))));
        auto scroll=window->findChild<QQuickItem *>("detailScroll"); QVERIFY(scroll);
        auto bar=window->findChild<QQuickItem *>("detailScrollBar"); QVERIFY(bar);
        auto form=window->findChild<QQuickItem *>("detailForm"); QVERIFY(form);
        auto notes=window->findChild<QQuickItem *>("detailNotes"); QVERIFY(notes);
        auto solution=window->findChild<QQuickItem *>("detailSolution"); QVERIFY(solution);
        auto viewport=scroll->property("contentItem").value<QQuickItem *>(); QVERIFY(viewport);
        QTRY_VERIFY(scroll->property("contentHeight").toReal()>scroll->height()*2);
        QTRY_COMPARE(viewport->property("contentY").toReal(),0.0);
        // The scrollbar must sit outside the form and both editor backgrounds.
        auto gutterClear=[&] {
            const qreal left=bar->mapToScene(QPointF(0,0)).x();
            for (auto item : {form,notes,solution})
                if (item->mapToScene(QPointF(item->width(),0)).x()>left-8) return false;
            return true;
        };
        QTRY_VERIFY(gutterClear());
        const QPoint thumb=bar->mapToScene(QPointF(bar->width()/2,bar->height()*bar->property("size").toReal()/2)).toPoint();
        const QPoint lower=bar->mapToScene(QPointF(bar->width()/2,bar->height()*0.65)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,thumb);
        QTest::mouseMove(window,lower,100);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,lower);
        QTRY_VERIFY(viewport->property("contentY").toReal()>100);
        if (!qEnvironmentVariableIsEmpty("LEETREPEAT_TEST_SCREENSHOT")) {
            QTest::qWait(50);
            window->grabWindow().save(qEnvironmentVariable("LEETREPEAT_TEST_SCREENSHOT")+"-detail-scrolled.png");
        }
        const qreal scrolled=viewport->property("contentY").toReal();
        const qreal formY=form->mapToScene(QPointF(0,0)).y();
        QVERIFY(formY<scroll->mapToScene(QPointF(0,0)).y());
        // Saving this problem preserves position; selecting another resets it.
        notes->setProperty("text",a.notes+"Saved note");
        auto save=window->findChild<QObject *>("saveProblemButton"); QVERIFY(save);
        QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QTRY_VERIFY(qAbs(viewport->property("contentY").toReal()-scrolled)<2);
        QVERIFY(controller.selectProblem(second));
        QTRY_COMPARE(viewport->property("contentY").toReal(),0.0);
        QVERIFY(controller.selectProblem(first));
        QTRY_COMPARE(viewport->property("contentY").toReal(),0.0);
        window->resize(1120,820);
        QTRY_VERIFY(gutterClear());
        QCOMPARE(warnings.count(),0);
    }
    void gradingDoubleClick_data() {
        QTest::addColumn<bool>("passed");
        QTest::newRow("PASS") << true;
        QTest::newRow("FAIL") << false;
    }
    void gradingDoubleClick() {
        QFETCH(bool,passed);
        QTemporaryDir dir; Database db(dir.filePath("double-click.sqlite")); Repository repo(db);
        Problem a; a.title="First"; a.url="https://leetcode.com/problems/first"; const auto first=repo.add(a);
        Problem b; b.title="Second"; b.url="https://leetcode.com/problems/second"; const auto second=repo.add(b);
        Settings settings; settings.shuffle=false; repo.saveSettings(settings); FixedIntervalScheduler scheduler;
        const auto learned=QDateTime::currentDateTime().addDays(-2);
        repo.learn(first,scheduler,learned,3); repo.learn(second,scheduler,learned,3);
        AppController app(repo); QQmlApplicationEngine engine; QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("app",&app);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        QVERIFY(app.review()->start());
        QVERIFY(QMetaObject::invokeMethod(window,"showPage",Q_ARG(QVariant,QString("review"))));
        auto button=window->findChild<QQuickItem *>(passed ? "passButton" : "failButton"); QVERIFY(button);
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,
                          button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint(),50);
        QCOMPARE(repo.history(first).size(),1); QCOMPARE(repo.history(second).size(),0);
        QCOMPARE(repo.history(first).first().toMap()["result"].toString(),QString(passed ? "PASS" : "FAIL"));
        QCOMPARE(app.review()->current()["id"].toLongLong(),second); QCOMPARE(app.review()->completed(),1);
        // A deliberate keyboard grade immediately afterwards still advances.
        QTest::keyClick(window,passed ? Qt::Key_F : Qt::Key_P);
        QTRY_COMPARE(repo.history(second).size(),1); QVERIFY(!app.review()->active());
        QCOMPARE(warnings.count(),0);
    }
    void learningDoubleClick() {
        QTemporaryDir dir; Database db(dir.filePath("learning-click.sqlite")); Repository repo(db);
        Problem a; a.title="First"; a.url="https://leetcode.com/problems/first"; const auto first=repo.add(a);
        Problem b; b.title="Second"; b.url="https://leetcode.com/problems/second"; const auto second=repo.add(b);
        AppController app(repo); QQmlApplicationEngine engine; QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("app",&app);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        QVERIFY(app.review()->start());
        QVERIFY(QMetaObject::invokeMethod(window,"showPage",Q_ARG(QVariant,QString("review"))));
        auto notes=window->findChild<QQuickItem *>("studyNotes"); QVERIFY(notes); notes->setProperty("text","Studied the first card");
        auto button=window->findChild<QQuickItem *>("markLearnedButton"); QVERIFY(button);
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,
                          button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint(),50);
        QVERIFY(repo.problem(first).learned); QVERIFY(!repo.problem(second).learned);
        QCOMPARE(repo.learnedOn(QDate::currentDate()),1); QCOMPARE(repo.problem(first).notes,QString("Studied the first card"));
        QCOMPARE(app.review()->current()["id"].toLongLong(),second); QCOMPARE(app.review()->completed(),1);
        QCOMPARE(repo.history(first).size(),0); QCOMPARE(warnings.count(),0);
    }
    void studyScrolling() {
        QTemporaryDir dir; Database db(dir.filePath("study-scroll.sqlite")); Repository repo(db);
        Problem a; a.title="First"; a.url="https://leetcode.com/problems/first";
        a.notes=QString("Remember the invariant.\n").repeated(100); const auto first=repo.add(a);
        Problem b=a; b.title="Second"; b.url="https://leetcode.com/problems/second"; const auto second=repo.add(b);
        AppController app(repo); QQmlApplicationEngine engine; QSignalSpy warnings(&engine,&QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("app",&app);
        engine.load(QUrl::fromLocalFile(QStringLiteral(QML_SOURCE_DIR "/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->resize(800,640); window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window));
        QVERIFY(app.review()->start());
        QVERIFY(QMetaObject::invokeMethod(window,"showPage",Q_ARG(QVariant,QString("review"))));
        auto scroll=window->findChild<QQuickItem *>("studyScroll"); QVERIFY(scroll);
        auto viewport=scroll->property("contentItem").value<QQuickItem *>(); QVERIFY(viewport);
        auto notes=window->findChild<QQuickItem *>("studyNotes"); QVERIFY(notes);
        auto solution=window->findChild<QQuickItem *>("studySolution"); QVERIFY(solution);
        QTRY_VERIFY(scroll->property("contentHeight").toReal()>scroll->height()*2);
        QTRY_COMPARE(viewport->property("contentY").toReal(),0.0);
        notes->setProperty("text",a.notes+"Saved note");
        viewport->setProperty("contentY",500); QTest::qWait(50);
        // Saving the same card preserves reading position.
        QVERIFY(app.review()->saveStudy(notes->property("text").toString(),solution->property("text").toString()));
        QTest::qWait(50); QCOMPARE(viewport->property("contentY").toReal(),500.0);
        QCOMPARE(app.review()->current()["id"].toLongLong(),first);
        auto button=window->findChild<QQuickItem *>("markLearnedButton"); QVERIFY(button);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                         button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        QCOMPARE(app.review()->current()["id"].toLongLong(),second);
        QTRY_COMPARE(viewport->property("contentY").toReal(),0.0);
        QTest::qWait(50); QCOMPARE(viewport->property("contentY").toReal(),0.0);
        QCOMPARE(notes->property("text").toString(),b.notes); QCOMPARE(warnings.count(),0);
    }
    void importPreviewAndCommit() {
        QTemporaryDir dir; Database db(dir.filePath("imports.sqlite")); Repository repo(db); AppController app(repo);
        auto path=dir.filePath("input.csv");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("title,url,tags\nA,https://leetcode.com/problems/a,Arrays;Hashing\nB,broken,Arrays\n"); file.close();
        QVERIFY(app.previewImport(QUrl::fromLocalFile(path)));
        QVERIFY(!app.importSummary()["canImport"].toBool()); QVERIFY(!app.commitImport()); QVERIFY(repo.problems().isEmpty());
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
        file.write("title,url,tags\nA,https://leetcode.com/problems/a,Arrays;Hashing\nB,https://leetcode.com/problems/b,Arrays\n a ,https://leetcode.com/problems/duplicate,Other\n"); file.close();
        QVERIFY(app.previewImport(QUrl::fromLocalFile(path))); QCOMPARE(app.importSummary()["ready"].toInt(),2);
        QCOMPARE(app.importSummary()["duplicates"].toInt(),1); QVERIFY(app.commitImport());
        QCOMPARE(repo.problems().size(),2);
        QMap<QString,QStringList> importedTags;
        for (const auto &p : repo.problems()) importedTags.insert(p.title,p.tags);
        QCOMPARE(importedTags["A"],QStringList({"Arrays","Hashing"}));
        QCOMPARE(importedTags["B"],QStringList({"Arrays"}));
        QCOMPARE(app.stats()["newCount"].toInt(),2); QVERIFY(!app.commitImport());
        auto exported=dir.filePath("export.csv"); QVERIFY(app.exportCsv(QUrl::fromLocalFile(exported)));
        QCOMPARE(CsvService::preview(exported,{}).ready.size(),2);
    }
    void lateReadFailurePreservesExportAndCachedLibrary() {
        QTemporaryDir dir; const auto path=dir.filePath("read-error.sqlite");
        Database db(path); Repository repo(db);
        QList<Problem> corpus;
        for (int i=0; i<200; ++i) {
            Problem p; p.title=QString("Read problem %1").arg(i);
            p.url=QString("https://leetcode.com/problems/read-%1").arg(i);
            p.notes=QString(1800,'x'); corpus.append(p);
        }
        QCOMPARE(repo.importProblems(corpus),200);
        AppController app(repo);
        const auto exported=dir.filePath("export.csv");
        QVERIFY(app.exportCsv(QUrl::fromLocalFile(exported)));
        QFile output(exported); QVERIFY(output.open(QIODevice::ReadOnly));
        const auto before=output.readAll(); output.close();

        int pageSize, rootPage;
        {
            auto size=query(db.connection(),"PRAGMA page_size"); QVERIFY(size.next()); pageSize=size.value(0).toInt();
            auto root=query(db.connection(),"SELECT rootpage FROM sqlite_master WHERE name='problems'");
            QVERIFY(root.next()); rootPage=root.value(0).toInt();
        }
        // Damage a middle table leaf in this disposable database. Earlier rows
        // remain readable, so exec() succeeds and the error occurs during next().
        auto connection=db.connection(); connection.close();
        QFile file(path); QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.seek(qint64(rootPage-1)*pageSize));
        const auto root=file.read(pageSize); QCOMPARE(root.size(),pageSize);
        QCOMPARE(quint8(root[0]),quint8(0x05)); // Interior table page, not page 1.
        const auto cells=qFromBigEndian<quint16>(root.constData()+3); QVERIFY(cells>2);
        const auto cell=qFromBigEndian<quint16>(root.constData()+12+2*(cells/2));
        QVERIFY(cell+4<=root.size());
        const auto leaf=qFromBigEndian<quint32>(root.constData()+cell);
        const auto offset=qint64(leaf-1)*pageSize;
        QVERIFY(file.seek(offset)); QCOMPARE(file.read(1),QByteArray(1,char(0x0d)));
        QVERIFY(file.seek(offset)); QCOMPARE(file.write(QByteArray(1,'\0')),qint64(1)); file.close();
        QVERIFY(connection.open());
        {
            auto raw=query(connection,"SELECT * FROM problems WHERE deleted=0 ORDER BY id");
            int rows=0; while (raw.next()) ++rows; // Deliberately inspect the driver's late error.
            QVERIFY(rows>0 && rows<200); QVERIFY(raw.lastError().isValid());
        }
        QVERIFY_EXCEPTION_THROWN(repo.problems(),std::runtime_error);
        QVERIFY(!app.refresh()); QVERIFY(app.messageIsError());
        QCOMPARE(app.problems()->rowCount(),200); QCOMPARE(app.stats()["total"].toInt(),200);
        QVERIFY(!app.exportCsv(QUrl::fromLocalFile(exported))); QVERIFY(app.messageIsError());
        QVERIFY(output.open(QIODevice::ReadOnly)); QCOMPARE(output.readAll(),before);
        QVERIFY(!app.review()->start()); QVERIFY(!app.review()->active());
    }
    void exportProtectsResolvedDatabaseAndSidecars() {
        QTemporaryDir dir; const auto real=dir.filePath("real.sqlite"), alias=dir.filePath("alias.sqlite");
        { Database db(real); }
        if (!QFile::link(real,alias) || !QFileInfo(alias).isSymbolicLink())
            QSKIP("Filesystem symlinks are unavailable.");
        const auto folderAlias=dir.filePath("folder-alias"); QVERIFY(QFile::link(dir.path(),folderAlias));
        Database db(alias); Repository repo(db); AppController app(repo);
        QLockFile lock(db.path()+".lock"); QVERIFY(lock.tryLock());
        for (const auto &suffix : QStringList{"", "-wal", "-shm", ".lock"}) {
            QFile file(real+suffix); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto before=file.readAll(); file.close();
            for (const auto &destination : QStringList{real+suffix,folderAlias+"/real.sqlite"+suffix}) {
                QVERIFY(!app.exportCsv(QUrl::fromLocalFile(destination))); QVERIFY(app.messageIsError());
                QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),before); file.close();
            }
        }
        QVERIFY(!app.exportCsv(QUrl::fromLocalFile(alias)));
        const auto lockAlias=dir.filePath("lock-alias"); QVERIFY(QFile::link(db.path()+".lock",lockAlias));
        QVERIFY(!app.exportCsv(QUrl::fromLocalFile(lockAlias)));
        QVERIFY(app.exportCsv(QUrl::fromLocalFile(dir.filePath("normal.csv"))));
    }
    void databaseAliasesShareInstanceLock() {
        QTemporaryDir dir; const auto real=dir.filePath("real.sqlite"), alias=dir.filePath("alias.sqlite");
        { Database db(real); }
        if (!QFile::link(real,alias) || !QFileInfo(alias).isSymbolicLink())
            QSKIP("Filesystem symlinks are unavailable.");
        auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM","offscreen"); environment.insert("QT_QUICK_BACKEND","software");
        QProcess first; first.setProcessEnvironment(environment);
        const auto cleanup=qScopeGuard([&]{ first.terminate(); first.waitForFinished(5000); });
        first.start(QStringLiteral(LEETREPEAT_BINARY_PATH),{"--database",alias});
        QVERIFY(first.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(real+".lock"),5000);
        QVERIFY(first.state()!=QProcess::NotRunning);
        for (const auto &path : QStringList{real,alias}) {
            QProcess second; second.setProcessEnvironment(environment);
            second.start(QStringLiteral(LEETREPEAT_BINARY_PATH),{"--database",path,"--smoke-test"});
            QVERIFY(second.waitForFinished(5000)); QCOMPARE(second.exitCode(),1);
            QVERIFY(second.readAllStandardError().contains("database is already in use"));
        }
    }
    void wslProblemLinksUseWindowsChrome() {
#ifdef Q_OS_LINUX
        if (!QSysInfo::kernelVersion().contains("microsoft",Qt::CaseInsensitive))
            QSKIP("This test exercises the WSL browser bridge.");
        QTemporaryDir dir; const auto folder=dir.filePath("Windows Chrome");
        QVERIFY(QDir().mkpath(folder));
        QFile chrome(folder+"/chrome.exe"); QVERIFY(chrome.open(QIODevice::WriteOnly));
        chrome.write("#!/bin/sh\nprintf '%s\\n' \"$#\" \"$@\" > \"$LEETREPEAT_BROWSER_TEST_OUTPUT\"\n"); chrome.close();
        QVERIFY(chrome.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto oldPath=qgetenv("PATH"), oldOutput=qgetenv("LEETREPEAT_BROWSER_TEST_OUTPUT");
        const auto restore=qScopeGuard([&]{
            if (oldPath.isNull()) qunsetenv("PATH"); else qputenv("PATH",oldPath);
            if (oldOutput.isNull()) qunsetenv("LEETREPEAT_BROWSER_TEST_OUTPUT");
            else qputenv("LEETREPEAT_BROWSER_TEST_OUTPUT",oldOutput);
        });
        qputenv("PATH",QFile::encodeName(folder)+':'+oldPath);
        const auto output=dir.filePath("received-url.txt");
        qputenv("LEETREPEAT_BROWSER_TEST_OUTPUT",QFile::encodeName(output));
        const auto received=[&]{ QFile file(output); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; };
        Database db(dir.filePath("browser.sqlite")); Repository repo(db);
        Problem p; p.title="Browser handoff";
        p.url="https://leetcode.com/problems/concatenation-of-array/?a=one&b=%22two%20words%22#solution";
        repo.add(p); AppController app(repo);
        const auto expected=QByteArray("1\n")+QUrl(p.url).toEncoded()+'\n';
        // Both detail and session actions must pass the entire URL as one argument,
        // even with a spaced executable path and shell punctuation in the query.
        QVERIFY(app.openProblemUrl(p.url)); QTRY_COMPARE(received(),expected);
        QVERIFY(QFile::remove(output));
        QVERIFY(app.review()->start()); QVERIFY(app.review()->openUrl()); QTRY_COMPARE(received(),expected);
        QVERIFY(QFile::remove(output));
        QVERIFY(!app.openProblemUrl("file:///tmp/should-not-open")); QVERIFY(app.messageIsError());
        QVERIFY(!QFileInfo::exists(output));
#else
        QSKIP("This test exercises the WSL browser bridge.");
#endif
    }
    void controllerFailuresAndRestart() {
        QTemporaryDir dir; auto path=dir.filePath("state.sqlite");
        qint64 id;
        {
            Database db(path); Repository repo(db); AppController app(repo);
            app.newProblem();
            QVERIFY(app.saveProblem({{"title","One"},{"url","https://leetcode.com/problems/one"},{"difficulty","Easy"},{"tags","Arrays;arrays"}}));
            id=app.detail()["id"].toLongLong();
            QVERIFY(app.review()->start()); QVERIFY(app.review()->markLearned("notes","solution"));
            QVERIFY(!app.exportCsv(QUrl::fromLocalFile(path)));
            QVERIFY(app.backup(QUrl::fromLocalFile(dir.filePath("backup.sqlite"))));
        }
        Database db(path); Repository repo(db); AppController app(repo);
        QCOMPARE(app.stats()["learnedToday"].toInt(),1); QCOMPARE(app.stats()["remaining"].toInt(),0);
        QCOMPARE(repo.problem(id).notes,QString("notes"));
        QVERIFY(app.review()->start()); QVERIFY(!app.review()->active());
    }
};
QTEST_MAIN(WorkflowTest)
#include "test_workflow.moc"
