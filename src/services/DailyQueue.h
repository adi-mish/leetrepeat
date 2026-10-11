#pragma once
#include "domain/Problem.h"
namespace lr {
struct DailyQueue {
    QList<qint64> reviews, fresh;
    QList<qint64> combined() const { auto result=reviews; result.append(fresh); return result; }
    static QList<qint64> shuffledNewOrder(const QList<Problem> &problems);
    static DailyQueue build(const QList<Problem> &problems, QDate today, const Settings &settings, int learnedToday);
};
}
