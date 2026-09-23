#include "Repository.h"
#include "scheduling/FixedIntervalScheduler.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlRecord>
#include <stdexcept>
namespace lr {
static QString json(const SchedulerState &state) {
    return QString::fromUtf8(QJsonDocument(state.toJson()).toJson(QJsonDocument::Compact));
}
static QString stamp(QDateTime now) { return now.toUTC().toString(Qt::ISODateWithMs); }
static QString text(const QString &s) { return s.isNull() ? QStringLiteral("") : s; }
static Problem readProblem(const QSqlQuery &q) {
    Problem p;
    p.id = q.value("id").toLongLong(); p.title = q.value("title").toString();
    p.url = q.value("url").toString(); p.difficulty = q.value("difficulty").toString();
    p.notes = q.value("notes").toString(); p.solution = q.value("solution").toString();
    p.learned = q.value("learned").toBool();
    p.firstLearned = QDate::fromString(q.value("first_learned").toString(), Qt::ISODate);
    p.lastReview = QDate::fromString(q.value("last_review").toString(), Qt::ISODate);
    p.nextReview = QDate::fromString(q.value("next_review").toString(), Qt::ISODate);
    p.state = SchedulerState::fromJson(QJsonDocument::fromJson(q.value("scheduler_state").toByteArray()).object());
    p.passes = q.value("passes").toInt(); p.failures = q.value("failures").toInt();
    p.createdAt = q.value("created_at").toString(); p.updatedAt = q.value("updated_at").toString();
    return p;
}
QList<Problem> Repository::problems() const {
    auto db = m_database.connection();
    auto q = query(db, "SELECT * FROM problems WHERE deleted=0 ORDER BY id");
    QList<Problem> result;
    QHash<qint64, qsizetype> positions;
    while (q.next()) { positions.insert(q.value("id").toLongLong(), result.size()); result.append(readProblem(q)); }
    auto tags = query(db, "SELECT pt.problem_id,t.name FROM problem_tags pt JOIN tags t ON t.id=pt.tag_id ORDER BY t.name_key");
    while (tags.next()) {
        auto it = positions.constFind(tags.value(0).toLongLong());
        if (it != positions.cend()) result[*it].tags.append(tags.value(1).toString());
    }
    return result;
}
Problem Repository::problem(qint64 id) const {
    auto db = m_database.connection();
    auto q = query(db, "SELECT * FROM problems WHERE id=? AND deleted=0", {id});
    if (!q.next()) throw std::runtime_error("Problem no longer exists.");
    auto p = readProblem(q);
    auto tags = query(db, "SELECT t.name FROM tags t JOIN problem_tags pt ON pt.tag_id=t.id WHERE pt.problem_id=? ORDER BY t.name_key", {id});
    while (tags.next()) p.tags.append(tags.value(0).toString());
    return p;
}
void Repository::writeTags(const Problem &p) {
    auto db = m_database.connection();
    query(db, "DELETE FROM problem_tags WHERE problem_id=?", {p.id});
    for (const auto &tag : p.tags) {
        query(db, "INSERT OR IGNORE INTO tags(name,name_key) VALUES(?,?)", {tag, tag.toCaseFolded()});
        query(db, "INSERT INTO problem_tags(problem_id,tag_id) SELECT ?,id FROM tags WHERE name_key=?", {p.id, tag.toCaseFolded()});
    }
}
void Repository::validateUnique(const Problem &p) const {
    auto q = query(m_database.connection(),
                   "SELECT title FROM problems WHERE deleted=0 AND id<>? AND (title_key=? OR url_key=?) LIMIT 1",
                   {p.id, normalizedTitle(p.title), normalizedUrl(p.url)});
    if (q.next())
        throw std::runtime_error(QString("A problem with this title or URL already exists: %1").arg(q.value(0).toString()).toStdString());
}
qint64 Repository::insert(Problem p) {
    validateProblem(p);
    p.id = 0;
    validateUnique(p);
    auto now = stamp(QDateTime::currentDateTime());
    auto q = query(m_database.connection(), "INSERT INTO problems(title,title_key,url,url_key,difficulty,notes,solution,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?)",
                   {p.title, normalizedTitle(p.title), p.url, normalizedUrl(p.url), p.difficulty, text(p.notes), text(p.solution), now, now});
    p.id = q.lastInsertId().toLongLong();
    writeTags(p);
    return p.id;
}
qint64 Repository::add(Problem p) {
    Transaction tx(m_database.connection());
    auto id = insert(p); tx.commit(); return id;
}
int Repository::importProblems(const QList<Problem> &problems) {
    Transaction tx(m_database.connection());
    for (const auto &p : problems) insert(p);
    tx.commit(); return int(problems.size());
}
void Repository::edit(Problem p) {
    validateProblem(p);
    Transaction tx(m_database.connection());
    problem(p.id);
    validateUnique(p);
    query(m_database.connection(), "UPDATE problems SET title=?,title_key=?,url=?,url_key=?,difficulty=?,notes=?,solution=?,updated_at=? WHERE id=?",
          {p.title, normalizedTitle(p.title), p.url, normalizedUrl(p.url), p.difficulty, text(p.notes), text(p.solution), stamp(QDateTime::currentDateTime()), p.id});
    writeTags(p); tx.commit();
}
void Repository::reset(qint64 id) {
    Transaction tx(m_database.connection()); problem(id);
    query(m_database.connection(), "UPDATE problems SET learned=0,first_learned=NULL,last_review=NULL,next_review=NULL,scheduler_state='{}',passes=0,failures=0,updated_at=? WHERE id=?",
          {stamp(QDateTime::currentDateTime()), id});
    tx.commit();
}
void Repository::remove(qint64 id) {
    Transaction tx(m_database.connection()); problem(id);
    // Tombstones preserve foreign keys and immutable attempts, even after explicit deletion.
    query(m_database.connection(), "UPDATE problems SET deleted=1,updated_at=? WHERE id=?", {stamp(QDateTime::currentDateTime()), id});
    tx.commit();
}
void Repository::learn(qint64 id, const IScheduler &scheduler, QDateTime now, int dailyLimit) {
    Transaction tx(m_database.connection()); auto p = problem(id);
    if (p.learned) throw std::runtime_error("This problem is already learned.");
    if (learnedOn(now.date()) >= dailyLimit) throw std::runtime_error("Today's new-problem target is complete.");
    auto next = scheduler.initial(now.date());
    query(m_database.connection(), "UPDATE problems SET learned=1,first_learned=?,next_review=?,scheduler_state=?,updated_at=? WHERE id=?",
          {now.date().toString(Qt::ISODate), next.due.toString(Qt::ISODate), json(next.state), stamp(now), id});
    query(m_database.connection(), "INSERT INTO learning_events(problem_id,local_date,timestamp) VALUES(?,?,?)",
          {id, now.date().toString(Qt::ISODate), stamp(now)});
    tx.commit();
}
void Repository::review(qint64 id, bool passed, const IScheduler &scheduler, QDateTime now) {
    Transaction tx(m_database.connection()); auto p = problem(id);
    if (!p.isDue(now.date())) throw std::runtime_error("This problem is not due for review.");
    auto next = passed ? scheduler.pass(p.state, now.date()) : scheduler.fail(p.state, now.date());
    query(m_database.connection(), "INSERT INTO attempts(problem_id,timestamp,local_date,result,state_before,state_after,previous_due,resulting_due) VALUES(?,?,?,?,?,?,?,?)",
          {id, stamp(now), now.date().toString(Qt::ISODate), passed ? "PASS" : "FAIL", json(p.state), json(next.state), p.nextReview.toString(Qt::ISODate), next.due.toString(Qt::ISODate)});
    query(m_database.connection(), "UPDATE problems SET last_review=?,next_review=?,scheduler_state=?,passes=passes+?,failures=failures+?,updated_at=? WHERE id=?",
          {now.date().toString(Qt::ISODate), next.due.toString(Qt::ISODate), json(next.state), passed ? 1 : 0, passed ? 0 : 1, stamp(now), id});
    tx.commit();
}
QVariantList Repository::history(qint64 id) const {
    auto q = query(m_database.connection(), "SELECT * FROM attempts WHERE problem_id=? ORDER BY id DESC", {id});
    QVariantList rows;
    while (q.next()) {
        QVariantMap row;
        for (int i=0; i<q.record().count(); ++i) row.insert(q.record().fieldName(i), q.value(i));
        rows.append(row);
    }
    return rows;
}
int Repository::learnedOn(QDate date) const {
    auto q = query(m_database.connection(), "SELECT COUNT(*) FROM learning_events WHERE local_date=?", {date.toString(Qt::ISODate)});
    q.next(); return q.value(0).toInt();
}
QVariantMap Repository::stats(QDate date) const {
    int learned=0, due=0, overdue=0;
    auto all = problems();
    for (const auto &p : all) { learned += p.learned; due += p.isDue(date); overdue += p.isDue(date) && p.nextReview < date; }
    auto q = query(m_database.connection(), "SELECT COALESCE(SUM(result='PASS'),0),COALESCE(SUM(result='FAIL'),0) FROM attempts WHERE local_date=?", {date.toString(Qt::ISODate)});
    q.next(); int passes=q.value(0).toInt(), failures=q.value(1).toInt();
    int learnedToday = learnedOn(date);
    int unseen = int(all.size()) - learned;
    int newCount = std::min(unseen, std::max(0, settings().newPerDay - learnedToday));
    return {{"date", date}, {"total", all.size()}, {"learned", learned}, {"unlearned", unseen}, {"due", due}, {"overdue", overdue},
            {"learnedToday", learnedToday}, {"passesToday", passes}, {"failuresToday", failures},
            {"completedToday", passes + failures + learnedToday}, {"newCount", newCount}, {"remaining", due + newCount}};
}
Settings Repository::settings() const {
    Settings s;
    auto q = query(m_database.connection(), "SELECT key,value FROM settings");
    while (q.next()) {
        auto key=q.value(0).toString(), value=q.value(1).toString();
        if (key=="newPerDay") s.newPerDay=value.toInt();
        if (key=="shuffle") s.shuffle=value=="true";
        if (key=="jitter") s.jitter=value=="true";
        if (key=="intervals") { s.intervals.clear(); for (const auto &n : value.split(',')) s.intervals.append(n.toInt()); }
    }
    FixedIntervalScheduler::validateIntervals(s.intervals);
    if (s.newPerDay < 0 || s.newPerDay > 1000) throw std::runtime_error("Invalid saved daily target.");
    return s;
}
void Repository::saveSettings(const Settings &s) {
    FixedIntervalScheduler::validateIntervals(s.intervals);
    if (s.newPerDay < 0 || s.newPerDay > 1000) throw std::runtime_error("New problems per day must be between 0 and 1000.");
    QStringList intervals; for (int n : s.intervals) intervals.append(QString::number(n));
    QVariantMap values{{"newPerDay", QString::number(s.newPerDay)}, {"intervals", intervals.join(',')},
                       {"shuffle", s.shuffle ? "true" : "false"}, {"jitter", s.jitter ? "true" : "false"}};
    Transaction tx(m_database.connection());
    for (auto it=values.cbegin(); it!=values.cend(); ++it)
        query(m_database.connection(), "INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value", {it.key(), it.value()});
    tx.commit();
}
}
