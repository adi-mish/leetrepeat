#pragma once
#include "domain/Problem.h"
namespace lr {
struct DailyQueue {
    QList<qint64> reviews, fresh;
    QList<qint64> combined() const { auto result=reviews; result.append(fresh); return result; }
    static DailyQueue build(const QList<Problem> &problems, QDate today, const Settings &settings, int learnedToday);
};
}
