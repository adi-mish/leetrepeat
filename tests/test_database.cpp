#include <QtTest>
#include <QTemporaryDir>
#include "repositories/Repository.h"
#include "scheduling/FixedIntervalScheduler.h"
#include "services/DailyQueue.h"
using namespace lr;
class DatabaseTest : public QObject {
    Q_OBJECT
private slots:
    void shuffledIntakePersistsAcrossPartialDaysAndImports() {
        QTemporaryDir dir; const auto path=dir.filePath("mixed.sqlite");
        const QDateTime today(QDate(2026,10,11),QTime(12,0));
        Settings settings; settings.newPerDay=4; settings.shuffle=false;
        FixedIntervalScheduler scheduler;
        QList<qint64> savedOrder, firstDay;
        {
            Database db(path); Repository repo(db); repo.saveSettings(settings);
            QList<Problem> corpus;
            for (const auto &difficulty : QStringList{"Easy","Medium","Hard"}) {
                const int count=difficulty=="Medium" ? 6 : 3;
                for (int i=0; i<count; ++i) {
                    Problem p; p.title=QString("%1 %2").arg(difficulty).arg(i);
                    p.url=QString("https://leetcode.com/problems/%1-%2").arg(difficulty.toLower()).arg(i);
                    p.difficulty=" "+difficulty.toLower()+" ";
                    p.notes="Saved notes"; p.solution="Saved solution"; p.tags={"Saved tag"};
                    p.learned=true; // A corpus import must ignore supplied progress.
                    corpus.append(p);
                }
            }
            QCOMPARE(repo.importProblems(corpus),12);
            for (const auto &p : repo.problems()) { savedOrder.append(p.id); QVERIFY(!p.learned); }
            firstDay=DailyQueue::build(repo.problems(),today.date(),settings,0).fresh;
            QStringList mix;
            for (auto id : firstDay) mix.append(repo.problem(id).difficulty);
            mix.sort(); QCOMPARE(mix,QStringList({"Easy","Hard","Medium","Medium"}));
            repo.learn(firstDay[0],scheduler,today,4);
            QCOMPARE(DailyQueue::build(repo.problems(),today.date(),settings,repo.learnedOn(today.date())).fresh,firstDay.mid(1));
        }
        Database db(path); Repository repo(db);
        QList<qint64> reopened;
        for (const auto &p : repo.problems()) {
            reopened.append(p.id); QCOMPARE(p.notes,QString("Saved notes"));
            QCOMPARE(p.solution,QString("Saved solution")); QCOMPARE(p.tags,QStringList{"Saved tag"});
        }
        QCOMPARE(reopened,savedOrder);
        QCOMPARE(DailyQueue::build(repo.problems(),today.date(),repo.settings(),repo.learnedOn(today.date())).fresh,firstDay.mid(1));
        for (auto id : firstDay.mid(1)) repo.learn(id,scheduler,today,4);
        QVERIFY(DailyQueue::build(repo.problems(),today.date(),repo.settings(),repo.learnedOn(today.date())).fresh.isEmpty());
        auto tomorrow=DailyQueue::build(repo.problems(),today.date().addDays(1),repo.settings(),0);
        QCOMPARE(tomorrow.reviews,firstDay); QCOMPARE(tomorrow.fresh,savedOrder.mid(4,4));
        QStringList mix;
        for (auto id : tomorrow.fresh) mix.append(repo.problem(id).difficulty);
        mix.sort(); QCOMPARE(mix,QStringList({"Easy","Hard","Medium","Medium"}));
        Problem extra; extra.title="Later"; extra.url="https://leetcode.com/problems/later";
        repo.add(extra);
        QList<Problem> later;
        for (int i=0; i<4; ++i) {
            Problem p; p.title=QString("Imported later %1").arg(i);
            p.url=QString("https://leetcode.com/problems/later-%1").arg(i);
            p.difficulty=QStringList{"Easy","Medium","Medium","Hard"}[i]; later.append(p);
        }
        QCOMPARE(repo.importProblems(later),4);
        QList<qint64> extended;
        for (const auto &p : repo.problems()) extended.append(p.id);
        QCOMPARE(extended.mid(0,12),savedOrder);
        QCOMPARE(DailyQueue::build(repo.problems(),today.date().addDays(1),repo.settings(),0).fresh,tomorrow.fresh);
    }
    void importOrderingFailureRollsBackEntireBatch() {
        Database db(":memory:"); Repository repo(db);
        Problem saved; saved.title="Saved"; saved.url="https://leetcode.com/problems/saved";
        saved.notes="Keep me"; saved.tags={"Original"}; const auto id=repo.add(saved);
        const auto before=repo.problem(id).toVariant();
        query(db.connection(),"CREATE TRIGGER fail_order BEFORE UPDATE OF study_order ON problems WHEN NEW.title='Incoming 3' BEGIN SELECT RAISE(ABORT,'injected order failure'); END");
        QList<Problem> batch;
        for (int i=0; i<4; ++i) {
            Problem p; p.title=QString("Incoming %1").arg(i);
            p.url=QString("https://leetcode.com/problems/incoming-%1").arg(i);
            p.difficulty=QStringList{"Easy","Medium","Medium","Hard"}[i]; p.tags={"Transient"}; batch.append(p);
        }
        QVERIFY_EXCEPTION_THROWN(repo.importProblems(batch),std::runtime_error);
        QCOMPARE(repo.problems().size(),1); QCOMPARE(repo.problem(id).toVariant(),before);
        auto tags=query(db.connection(),"SELECT COUNT(*) FROM tags"); QVERIFY(nextRow(tags)); QCOMPARE(tags.value(0).toInt(),1);
        auto order=query(db.connection(),"SELECT study_order FROM problems WHERE id=?",{id});
        QVERIFY(nextRow(order)); QCOMPARE(order.value(0).toLongLong(),qint64(1));
    }
    void v1MigrationPreservesDataAndRollsBackFailure() {
        QTemporaryDir dir; const auto path=dir.filePath("migration.sqlite");
        const QDateTime first(QDate(2026,9,1),QTime(12,0));
        FixedIntervalScheduler scheduler; qint64 learned, unseen, deleted;
        QVariantMap before; QVariantList history; Settings settings;
        settings.newPerDay=4; settings.appearance="Dark"; settings.intervals={1,3,9};
        {
            Database db(path); Repository repo(db);
            Problem p; p.title="Learned"; p.url="https://leetcode.com/problems/learned";
            p.notes="Preserve notes"; p.solution="Preserve solution"; p.tags={"Arrays","Custom"};
            learned=repo.add(p); repo.learn(learned,scheduler,first,4); repo.review(learned,false,scheduler,first.addDays(1));
            p.title="Deleted"; p.url="https://leetcode.com/problems/deleted"; deleted=repo.add(p); repo.remove(deleted);
            p.title="Unseen"; p.url="https://leetcode.com/problems/unseen"; unseen=repo.add(p);
            repo.saveSettings(settings); before=repo.problem(learned).toVariant(); history=repo.history(learned);
            // Remove only the v2 additions to recreate the deployed v1 schema.
            query(db.connection(),"DROP INDEX problems_study_order");
            query(db.connection(),"ALTER TABLE problems DROP COLUMN study_order");
            query(db.connection(),"UPDATE schema_version SET version=1");
            query(db.connection(),"CREATE TRIGGER fail_migration BEFORE UPDATE ON schema_version BEGIN SELECT RAISE(ABORT,'injected migration failure'); END");
            QVERIFY_EXCEPTION_THROWN(Database{path},std::runtime_error);
            auto version=query(db.connection(),"SELECT version FROM schema_version");
            QVERIFY(nextRow(version)); QCOMPARE(version.value(0).toInt(),1); version.finish();
            auto columns=query(db.connection(),"SELECT name FROM pragma_table_info('problems') WHERE name='study_order'");
            QVERIFY(!nextRow(columns));
            QCOMPARE(repo.problem(learned).toVariant(),before); QCOMPARE(repo.history(learned),history);
            query(db.connection(),"DROP TRIGGER fail_migration");
        }
        {
            Database db(path); Repository repo(db);
            QCOMPARE(repo.problems().size(),2); QCOMPARE(repo.problems()[0].id,learned); QCOMPARE(repo.problems()[1].id,unseen);
            QCOMPARE(repo.problem(learned).toVariant(),before); QCOMPARE(repo.history(learned),history);
            QCOMPARE(repo.learnedOn(first.date()),1); QCOMPARE(repo.settings().newPerDay,4);
            QCOMPARE(repo.settings().appearance,settings.appearance); QCOMPARE(repo.settings().intervals,settings.intervals);
            auto tombstone=query(db.connection(),"SELECT deleted FROM problems WHERE id=?",{deleted});
            QVERIFY(nextRow(tombstone)); QCOMPARE(tombstone.value(0).toInt(),1);
            auto version=query(db.connection(),"SELECT version FROM schema_version"); QVERIFY(nextRow(version)); QCOMPARE(version.value(0).toInt(),2);
            QVERIFY_EXCEPTION_THROWN(query(db.connection(),"DELETE FROM attempts"),std::runtime_error);
            query(db.connection(),"UPDATE schema_version SET version=3");
        }
        QVERIFY_EXCEPTION_THROWN(Database{path},std::runtime_error);
    }
    void symlinkDatabasePaths() {
        QTemporaryDir dir; const auto real=dir.filePath("real.sqlite"), alias=dir.filePath("alias.sqlite");
        { Database db(real); }
        if (!QFile::link(real,alias) || !QFileInfo(alias).isSymbolicLink())
            QSKIP("Filesystem symlinks are unavailable.");
        qint64 id;
        {
            Database db(alias); Repository repo(db);
            Problem p; p.title="Via alias"; p.url="https://leetcode.com/problems/alias";
            id=repo.add(p);
            QCOMPARE(db.path(),QFileInfo(real).canonicalFilePath());
            QVERIFY(!QFileInfo::exists(alias+"-wal"));
        }
        { Database db(real); Repository repo(db); QCOMPARE(repo.problem(id).title,QString("Via alias")); }

        const auto folder=dir.filePath("data"), folderAlias=dir.filePath("data-alias");
        QVERIFY(QDir().mkpath(folder)); QVERIFY(QFile::link(folder,folderAlias));
        {
            Database db(folderAlias+"/new/nested.sqlite"); Repository repo(db);
            QCOMPARE(db.path(),QFileInfo(folder+"/new/nested.sqlite").canonicalFilePath());
            Problem p; p.title="New database"; p.url="https://leetcode.com/problems/new"; repo.add(p);
        }
        { Database db(folder+"/new/nested.sqlite"); Repository repo(db); QCOMPARE(repo.problems().size(),1); }
        const auto target=folder+"/new/created.sqlite", dangling=dir.filePath("dangling.sqlite");
        QVERIFY(QFile::link(target,dangling));
        Database db(dangling); QCOMPARE(db.path(),QFileInfo(target).canonicalFilePath());
    }
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
