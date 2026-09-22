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
        QTest::qWait(100);
        QVERIFY(controller.review()->start()); QVERIFY(show("review")); QTest::qWait(100);
        QVERIFY(controller.review()->active()); QCOMPARE(controller.review()->current()["id"].toLongLong(),reviewId);
        QVERIFY(!controller.review()->revealed());
        auto notes=window->findChild<QQuickItem *>("studyNotes"); QVERIFY(notes); QVERIFY(!notes->isVisible());
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(controller.review()->revealed()); QVERIFY(notes->isVisible());
        QTest::keyClick(window,Qt::Key_P); QTRY_COMPARE(repo.history(reviewId).size(),1);
        QCOMPARE(controller.review()->current()["id"].toLongLong(),newId);
        QVERIFY(!controller.review()->revealed());
        notes->setProperty("text","My study notes");
        auto solution=window->findChild<QQuickItem *>("studySolution"); QVERIFY(solution); solution->setProperty("text","My solution");
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        // Letters typed during initial study must never grade a problem.
        notes->forceActiveFocus(); QTest::keyClick(window,Qt::Key_P);
        QCOMPARE(repo.history(newId).size(),0);
        auto mark=window->findChild<QObject *>("markLearnedButton"); QVERIFY(mark); QVERIFY(QMetaObject::invokeMethod(mark,"clicked"));
        QVERIFY(repo.problem(newId).learned); QCOMPARE(repo.problem(newId).solution,QString("My solution"));
        QCOMPARE(repo.history(newId).size(),0); QCOMPARE(repo.problem(newId).nextReview,QDate::currentDate().addDays(1));
        QVERIFY(!controller.review()->active()); QCOMPARE(controller.stats()["remaining"].toInt(),0);
        QVERIFY(show("today")); QVERIFY(show("library")); QTest::qWait(50);
        controller.problems()->filter("Review","Learned","All difficulties","arrays;HASHING","Title"); QCOMPARE(controller.problems()->rowCount(),1);
        controller.problems()->filter("Review","All","All difficulties","trees","Title"); QCOMPARE(controller.problems()->rowCount(),0);
        QVERIFY(controller.selectProblem(reviewId)); QVERIFY(show("detail")); QTest::qWait(50);
        QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        auto title=window->findChild<QObject *>("problemTitle"); QVERIFY(title); title->setProperty("text","Renamed problem");
        QVERIFY(current()->property("hasUnsavedChanges").toBool());
        QVERIFY(QMetaObject::invokeMethod(window,"navigate",Q_ARG(QVariant,QString("today"))));
        QCOMPARE(window->property("currentPage").toString(),QString("detail")); // Unsaved navigation blocked.
        // Close the confirmation without discarding, then save.
        QTest::keyClick(window,Qt::Key_Escape);
        auto save=window->findChild<QObject *>("saveProblemButton"); QVERIFY(save); QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QCOMPARE(repo.problem(reviewId).title,QString("Renamed problem")); QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QVERIFY(show("settings")); QTest::qWait(50); QVERIFY(!current()->property("hasUnsavedChanges").toBool());
        QVERIFY(!controller.saveSettings(3,"1,2,2",true,false)); QVERIFY(controller.messageIsError());
        QVERIFY(controller.saveSettings(5,"1,3,7",false,true)); QCOMPARE(repo.settings().newPerDay,5);
        QVERIFY(show("import")); QTest::qWait(50);
        QVERIFY(show("today")); QTest::qWait(100);
        if (!qEnvironmentVariableIsEmpty("LEETREPEAT_TEST_SCREENSHOT")) window->grabWindow().save(qEnvironmentVariable("LEETREPEAT_TEST_SCREENSHOT"));
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
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
