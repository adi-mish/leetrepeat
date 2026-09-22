#pragma once
#include <QDate>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace lr {
struct SchedulerState {
    int stage = 0;
    QString name = QStringLiteral("fixed");
    int version = 1;
    QJsonObject metadata;
    QJsonObject toJson() const;
    static SchedulerState fromJson(const QJsonObject &json);
};
struct Problem {
    qint64 id = 0;
    QString title, url, difficulty = QStringLiteral("Unknown"), notes, solution;
    QStringList tags;
    bool learned = false;
    QDate firstLearned, lastReview, nextReview;
    SchedulerState state;
    int passes = 0, failures = 0;
    QString createdAt, updatedAt;
    bool isDue(QDate today) const { return learned && nextReview.isValid() && nextReview <= today; }
    QVariantMap toVariant() const;
};
struct Settings {
    int newPerDay = 3;
    QList<int> intervals{1, 2, 4, 7, 14, 30, 60, 120, 240};
    bool shuffle = true, jitter = false;
};
QString normalizedTitle(const QString &text);
QString normalizedUrl(const QString &text);
QStringList normalizedTags(const QStringList &tags);
void validateProblem(Problem &problem);
}
