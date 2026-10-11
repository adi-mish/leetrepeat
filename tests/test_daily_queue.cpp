#include <QtTest>
#include "services/DailyQueue.h"
using namespace lr;
class QueueTest : public QObject {
    Q_OBJECT
private slots:
    void mixedNewProblemsAndExhaustedDifficulties() {
        QList<Problem> problems;
        QHash<qint64,QString> difficulties;
        const QStringList input{"Easy","Easy","Easy","Medium","Medium","Medium","Medium","Medium","Medium","Hard","Hard","Unknown"};
        QSet<qint64> expected;
        for (const auto &difficulty : input) {
            Problem p; p.id=problems.size()+1; p.difficulty=difficulty;
            problems.append(p); difficulties.insert(p.id,difficulty); expected.insert(p.id);
        }
        Problem learned; learned.id=100; learned.difficulty="Easy"; learned.learned=true;
        problems.append(learned);
        auto order=DailyQueue::shuffledNewOrder(problems);
        QCOMPARE(order.size(),expected.size());
        QCOMPARE(QSet<qint64>(order.cbegin(),order.cend()),expected);
        for (int day=0; day<2; ++day) {
            QStringList mix;
            for (int i=day*4; i<day*4+4; ++i) mix.append(difficulties[order[i]]);
            mix.sort(); QCOMPARE(mix,QStringList({"Easy","Hard","Medium","Medium"}));
        }
        // Exhausting Hard must still allow every remaining problem to be learned.
        QStringList remainder;
        for (int i=8; i<order.size(); ++i) remainder.append(difficulties[order[i]]);
        remainder.sort(); QCOMPARE(remainder,QStringList({"Easy","Medium","Medium","Unknown"}));
        QCOMPARE(difficulties[order.last()],QString("Unknown"));
        QVERIFY(DailyQueue::shuffledNewOrder({learned}).isEmpty());
    }
    void dailyCountsAndSkippedDays() {
        Settings s; s.shuffle=false;
        QDate today(2026,9,22);
        QVERIFY(DailyQueue::build({},today,s,0).combined().isEmpty());
        QList<Problem> problems;
        for (int i=1;i<=6;++i) { Problem p; p.id=i; problems.append(p); }
        QCOMPARE(DailyQueue::build(problems,today,s,0).fresh.size(),3);
        QCOMPARE(DailyQueue::build(problems,today,s,2).fresh.size(),1);
        QVERIFY(DailyQueue::build(problems,today,s,3).fresh.isEmpty());
        for (int i=0;i<4;++i) { problems[i].learned=true; problems[i].nextReview=today.addDays(i-2); }
        auto queue=DailyQueue::build(problems,today,s,0);
        QCOMPARE(queue.reviews,QList<qint64>({1,2,3})); QCOMPARE(queue.fresh,QList<qint64>({5,6}));
        queue=DailyQueue::build(problems,today.addDays(10),s,0);
        QCOMPARE(queue.reviews,QList<qint64>({1,2,3,4})); QCOMPARE(queue.fresh.size(),2);
        s.shuffle=true;
        auto shuffled=DailyQueue::build(problems,today.addDays(10),s,0).reviews;
        std::sort(shuffled.begin(),shuffled.end()); QCOMPARE(shuffled,queue.reviews);
    }
};
QTEST_GUILESS_MAIN(QueueTest)
#include "test_daily_queue.moc"
