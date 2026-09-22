#pragma once
#include "database/Database.h"
#include "domain/Problem.h"
#include "scheduling/IScheduler.h"
#include <QDateTime>
#include <optional>
namespace lr {
class Repository {
public:
    explicit Repository(Database &database) : m_database(database) {}
    QList<Problem> problems() const;
    Problem problem(qint64 id) const;
    qint64 add(Problem problem);
    int importProblems(const QList<Problem> &problems);
    void edit(Problem problem);
    void reset(qint64 id);
    void remove(qint64 id);
    void learn(qint64 id, const IScheduler &scheduler, QDateTime now, int dailyLimit);
    void review(qint64 id, bool passed, const IScheduler &scheduler, QDateTime now);
    QVariantList history(qint64 id) const;
    int learnedOn(QDate date) const;
    QVariantMap stats(QDate date) const;
    Settings settings() const;
    void saveSettings(const Settings &settings);
    Database &database() { return m_database; }
private:
    qint64 insert(Problem problem);
    void writeTags(const Problem &problem);
    Database &m_database;
};
}
