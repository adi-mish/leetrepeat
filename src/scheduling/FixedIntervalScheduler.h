#pragma once
#include "IScheduler.h"
namespace lr {
class FixedIntervalScheduler final : public IScheduler {
public:
    explicit FixedIntervalScheduler(QList<int> intervals = Settings{}.intervals, bool jitter = false);
    static void validateIntervals(const QList<int> &intervals);
    Schedule initial(QDate today) const override;
    Schedule pass(const SchedulerState &before, QDate today) const override;
    Schedule fail(const SchedulerState &before, QDate today) const override;
    QString name() const override { return QStringLiteral("fixed"); }
    int version() const override { return 1; }
private:
    Schedule schedule(int stage, QDate today, bool tomorrow) const;
    QList<int> m_intervals;
    bool m_jitter;
};
}
