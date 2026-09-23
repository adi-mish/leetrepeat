#include <QtTest>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
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
        QCOMPARE(repo.problems().size(),2); QCOMPARE(repo.problems()[0].tags.size(),2);
        QCOMPARE(app.stats()["newCount"].toInt(),2); QVERIFY(!app.commitImport());
        auto exported=dir.filePath("export.csv"); QVERIFY(app.exportCsv(QUrl::fromLocalFile(exported)));
        QCOMPARE(CsvService::preview(exported,{}).ready.size(),2);
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
