#include <QtTest>
#include "scheduling/FixedIntervalScheduler.h"
using namespace lr;
class SchedulerTest : public QObject {
    Q_OBJECT
private slots:
    void progression() {
        FixedIntervalScheduler s;
        QDate today(2024,2,28);
        auto state=s.initial(today);
        QCOMPARE(state.due,QDate(2024,2,29)); QCOMPARE(state.state.stage,0);
        for (int i=1;i<12;++i) {
            today=state.due; state=s.pass(state.state,today);
            int stage=std::min(i,8);
            QCOMPARE(state.state.stage,stage); QCOMPARE(state.due,today.addDays(Settings{}.intervals[stage]));
        }
        state=s.fail(state.state,QDate(2024,12,31));
        QCOMPARE(state.state.stage,0); QCOMPARE(state.due,QDate(2025,1,1));
        state=s.pass(state.state,state.due);
        QCOMPARE(state.state.stage,1); QCOMPARE(state.due,QDate(2025,1,3));
    }
    void validation() {
        for (const QList<int> &invalid : QList<QList<int>>{{},{0},{-1,2},{2,1},{1,1},{1,36501},QList<int>(33,1)})
            QVERIFY_EXCEPTION_THROWN(FixedIntervalScheduler::validateIntervals(invalid),std::runtime_error);
        FixedIntervalScheduler custom({3,5,10});
        QCOMPARE(custom.initial(QDate(2026,1,1)).due,QDate(2026,1,2));
        QCOMPARE(custom.pass({},QDate(2026,1,1)).due,QDate(2026,1,6));
        QCOMPARE(custom.fail({},QDate(2026,1,1)).due,QDate(2026,1,2));
        FixedIntervalScheduler single({7});
        QCOMPARE(single.pass({},QDate(2026,1,1)).state.stage,0);
        QVERIFY_EXCEPTION_THROWN(single.initial({}),std::runtime_error);
    }
    void jitter() {
        FixedIntervalScheduler s(Settings{}.intervals,true);
        const QDate today(2026,1,1);
        for (int i=0;i<500;++i) {
            QCOMPARE(s.initial(today).due,today.addDays(1));
            QCOMPARE(s.pass({},today).due,today.addDays(2));
            SchedulerState before; before.stage=3;
            auto result=s.pass(before,today);
            QVERIFY(today.daysTo(result.due)>=13 && today.daysTo(result.due)<=15);
            before.stage=8; result=s.pass(before,today);
            QVERIFY(today.daysTo(result.due)>=216 && today.daysTo(result.due)<=264);
        }
    }
};
QTEST_GUILESS_MAIN(SchedulerTest)
#include "test_scheduler.moc"
