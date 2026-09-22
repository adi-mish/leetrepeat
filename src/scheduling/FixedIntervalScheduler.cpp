#include "FixedIntervalScheduler.h"
#include <QJsonArray>
#include <QRandomGenerator>
#include <algorithm>
#include <stdexcept>
namespace lr {
FixedIntervalScheduler::FixedIntervalScheduler(QList<int> intervals, bool jitter)
    : m_intervals(std::move(intervals)), m_jitter(jitter) { validateIntervals(m_intervals); }
void FixedIntervalScheduler::validateIntervals(const QList<int> &intervals) {
    if (intervals.isEmpty() || intervals.size() > 32)
        throw std::runtime_error("Use between 1 and 32 intervals.");
    int previous = 0;
    for (int value : intervals) {
        if (value <= previous || value > 36500)
            throw std::runtime_error("Intervals must be strictly increasing positive integers, at most 36500 days.");
        previous = value;
    }
}
Schedule FixedIntervalScheduler::schedule(int stage, QDate today, bool tomorrow) const {
    if (!today.isValid()) throw std::runtime_error("Invalid scheduling date.");
    stage = std::clamp(stage, 0, int(m_intervals.size()) - 1);
    int days = tomorrow ? 1 : m_intervals[stage];
    if (m_jitter && !tomorrow && days >= 14) {
        int spread = std::max(1, days / 10);
        days += QRandomGenerator::global()->bounded(2 * spread + 1) - spread;
    }
    QJsonArray intervals;
    for (int i : m_intervals) intervals.append(i);
    return {{stage, name(), version(), {{"intervals", intervals}, {"jitter", m_jitter}, {"scheduledDays", days}}},
            today.addDays(std::max(1, days))};
}
Schedule FixedIntervalScheduler::initial(QDate today) const { return schedule(0, today, true); }
Schedule FixedIntervalScheduler::pass(const SchedulerState &before, QDate today) const {
    if (before.name != name() || before.version != version())
        throw std::runtime_error("This scheduler cannot read the saved scheduling state.");
    return schedule(std::clamp(before.stage, 0, int(m_intervals.size()) - 1) + 1, today, false);
}
Schedule FixedIntervalScheduler::fail(const SchedulerState &, QDate today) const { return schedule(0, today, true); }
}
