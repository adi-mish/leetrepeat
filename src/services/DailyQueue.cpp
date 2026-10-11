#include "DailyQueue.h"
#include <QRandomGenerator>
#include <algorithm>
#include <array>
namespace lr {
QList<qint64> DailyQueue::shuffledNewOrder(const QList<Problem> &problems) {
    std::array<QList<qint64>,4> buckets;
    for (const auto &p : problems) {
        if (p.learned) continue;
        const int bucket=p.difficulty=="Easy" ? 0 : p.difficulty=="Medium" ? 1 : p.difficulty=="Hard" ? 2 : 3;
        buckets[bucket].append(p.id);
    }
    for (auto &bucket : buckets)
        std::shuffle(bucket.begin(), bucket.end(), *QRandomGenerator::global());
    QList<qint64> order;
    // Save this order once, so restarting a partial day does not select a new mix.
    while (!buckets[0].isEmpty() || !buckets[1].isEmpty() || !buckets[2].isEmpty()) {
        for (int bucket : {0,1,1,2})
            if (!buckets[bucket].isEmpty()) order.append(buckets[bucket].takeLast());
    }
    order.append(buckets[3]);
    return order;
}
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
