#include <QtTest>
#include <QTemporaryDir>
#include "repositories/Repository.h"
#include "scheduling/FixedIntervalScheduler.h"
using namespace lr;
class DatabaseTest : public QObject {
    Q_OBJECT
private slots:
    void persistenceAndHistory() {
        QTemporaryDir dir; auto path=dir.filePath("test.sqlite");
        FixedIntervalScheduler scheduler;
        QDateTime first(QDate(2026,9,1),QTime(12,0)); qint64 id;
        {
            Database db(path); Repository repo(db);
            Problem p; p.title="Two Sum"; p.url="https://leetcode.com/problems/two-sum/";
            p.tags={"Arrays","HASHING","arrays"}; id=repo.add(p);
            repo.learn(id,scheduler,first,3);
            QCOMPARE(repo.history(id).size(),0); QCOMPARE(repo.learnedOn(first.date()),1);
            QCOMPARE(repo.problem(id).nextReview,first.date().addDays(1));
            QVERIFY_EXCEPTION_THROWN(repo.review(id,true,scheduler,first),std::runtime_error);
            repo.review(id,true,scheduler,first.addDays(10));
            QCOMPARE(repo.problem(id).nextReview,first.date().addDays(12));
            QCOMPARE(repo.problem(id).passes,1); QCOMPARE(repo.history(id).size(),1);
            QVERIFY_EXCEPTION_THROWN(repo.review(id,false,scheduler,first.addDays(10)),std::runtime_error);
            repo.review(id,false,scheduler,first.addDays(12));
            auto history=repo.history(id); QCOMPARE(history.size(),2);
            QCOMPARE(history[0].toMap()["result"].toString(),QString("FAIL"));
            QVERIFY(!history[0].toMap()["state_before"].toString().isEmpty());
            Settings settings; settings.newPerDay=7; settings.intervals={1,3,9}; settings.jitter=true; repo.saveSettings(settings);
            db.backup(dir.filePath("backup.sqlite"));
            QVERIFY_EXCEPTION_THROWN(db.backup(path),std::runtime_error);
        }
        {
            Database db(path); Repository repo(db);
            QCOMPARE(repo.problem(id).tags.size(),2); QCOMPARE(repo.problem(id).failures,1);
            QCOMPARE(repo.settings().newPerDay,7); QCOMPARE(repo.settings().intervals,QList<int>({1,3,9}));
            QCOMPARE(repo.history(id).size(),2);
            QVERIFY_EXCEPTION_THROWN(query(db.connection(),"DELETE FROM attempts"),std::runtime_error);
            repo.reset(id); QVERIFY(!repo.problem(id).learned); QCOMPARE(repo.history(id).size(),2);
            QCOMPARE(repo.learnedOn(first.date()),1);
            repo.remove(id); QVERIFY(repo.problems().isEmpty()); QCOMPARE(repo.history(id).size(),2);
        }
        Database backup(dir.filePath("backup.sqlite")); Repository restored(backup);
        QCOMPARE(restored.problem(id).failures,1); QCOMPARE(restored.history(id).size(),2);
    }
    void thousandProblemCorpus() {
        QTemporaryDir dir; auto path=dir.filePath("corpus.sqlite");
        {
            Database db(path); Repository repo(db);
            QList<Problem> corpus;
            for (int i=0; i<1000; ++i) {
                Problem p; p.title=QString("Problem %1").arg(i);
                p.url=QString("https://leetcode.com/problems/example-%1/").arg(i);
                p.tags={"Arrays", "Custom tag", "ARRAYS"}; corpus.append(p);
            }
            QCOMPARE(repo.importProblems(corpus),1000);
            auto saved=repo.problems(); QCOMPARE(saved.size(),1000);
            FixedIntervalScheduler scheduler;
            for (int i=0; i<20; ++i) repo.learn(saved[i].id,scheduler,QDateTime::currentDateTime().addDays(-10),20);
        }
        Database db(path); Repository repo(db);
        auto stats=repo.stats(QDate::currentDate());
        QCOMPARE(stats["total"].toInt(),1000); QCOMPARE(stats["learned"].toInt(),20);
        QCOMPARE(stats["due"].toInt(),20); QCOMPARE(stats["overdue"].toInt(),20);
        QCOMPARE(stats["newCount"].toInt(),3); QCOMPARE(stats["remaining"].toInt(),23);
        QCOMPARE(repo.problems().last().tags.size(),2);
    }
    void atomicWritesAndQuota() {
        Database db(":memory:"); Repository repo(db); FixedIntervalScheduler scheduler;
        Problem a; a.title="A"; a.url="https://leetcode.com/problems/a";
        Problem b=a; b.title="B"; b.url="https://leetcode.com/problems/b";
        auto id=repo.add(a);
        QVERIFY_EXCEPTION_THROWN(repo.importProblems({b,a}),std::runtime_error);
        QCOMPARE(repo.problems().size(),1);
        auto second=repo.add(b); auto now=QDateTime::currentDateTime();
        repo.learn(id,scheduler,now,1);
        QVERIFY_EXCEPTION_THROWN(repo.learn(second,scheduler,now,1),std::runtime_error);
        QVERIFY(!repo.problem(second).learned);
        query(db.connection(),"CREATE TRIGGER fail_review BEFORE UPDATE ON problems BEGIN SELECT RAISE(ABORT,'injected failure'); END");
        QVERIFY_EXCEPTION_THROWN(repo.review(id,true,scheduler,now.addDays(1)),std::runtime_error);
        QCOMPARE(repo.history(id).size(),0); QCOMPARE(repo.problem(id).passes,0);
        auto fk=query(db.connection(),"PRAGMA foreign_keys"); QVERIFY(fk.next()); QCOMPARE(fk.value(0).toInt(),1);
    }
};
QTEST_GUILESS_MAIN(DatabaseTest)
#include "test_database.moc"
