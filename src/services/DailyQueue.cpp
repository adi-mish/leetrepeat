#include "DailyQueue.h"
#include <QRandomGenerator>
#include <algorithm>
namespace lr {
DailyQueue DailyQueue::build(const QList<Problem> &problems, QDate today, const Settings &s, int learnedToday) {
    DailyQueue queue;
    int remaining = std::max(0, s.newPerDay - learnedToday);
    for (const auto &p : problems) {
        if (p.isDue(today)) queue.reviews.append(p.id);
        else if (!p.learned && queue.fresh.size() < remaining) queue.fresh.append(p.id);
    }
    if (s.shuffle) std::shuffle(queue.reviews.begin(), queue.reviews.end(), *QRandomGenerator::global());
    return queue;
}
}
