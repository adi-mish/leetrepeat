#include <QtTest>
#include "services/DailyQueue.h"
using namespace lr;
class QueueTest : public QObject {
    Q_OBJECT
private slots:
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
