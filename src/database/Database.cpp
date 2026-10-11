#include "Database.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QUuid>
#include <stdexcept>
namespace lr {
static void error(const QString &message) { throw std::runtime_error(message.toStdString()); }
QString resolvedFilePath(const QString &path) {
    QFileInfo file(path);
    QStringList links;
    // Follow even a dangling file symlink, so a new database gets the same lock
    // whether opened through the symlink or its target.
    while (file.isSymbolicLink()) {
        const auto link=file.absoluteFilePath();
        if (links.contains(link)) error("Circular file symlink: " + path);
        links.append(link);
        file.setFile(file.symLinkTarget());
    }
    const auto canonical=file.canonicalFilePath();
    if (!canonical.isEmpty()) return canonical;
    // A new file has no canonical path yet; its existing parent can still be a symlink.
    const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();
    return QDir(parent.isEmpty() ? file.absolutePath() : parent).filePath(file.fileName());
}
QSqlQuery query(QSqlDatabase db, const QString &sql, const QVariantList &values) {
    QSqlQuery q(db);
    if (!q.prepare(sql)) error(q.lastError().text());
    for (const auto &value : values) q.addBindValue(value);
    if (!q.exec()) error(q.lastError().text());
    return q;
}
bool nextRow(QSqlQuery &q) {
    if (q.next()) return true;
    // SQLite can fail after exec() succeeds, while fetching a later row.
    if (q.lastError().isValid()) error(q.lastError().text());
    return false;
}
Transaction::Transaction(QSqlDatabase db) : m_db(db) {
    // Take the write lock before reading state, preventing stale read-modify-write updates.
    query(m_db, "BEGIN IMMEDIATE");
}
Transaction::~Transaction() { if (!m_done) m_db.rollback(); }
void Transaction::commit() {
    if (!m_db.commit()) error(m_db.lastError().text());
    m_done = true;
}
Database::Database(const QString &path) : m_connectionName(QUuid::createUuid().toString()) {
    auto resolved=path==":memory:" ? path : resolvedFilePath(path);
    if (resolved != ":memory:") {
        if (!QDir().mkpath(QFileInfo(resolved).absolutePath())) error("Cannot create database directory.");
        resolved=resolvedFilePath(resolved);
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(resolved);
    m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    try {
        if (!m_db.open()) error(m_db.lastError().text());
        query(m_db, "PRAGMA foreign_keys=ON");
        query(m_db, "PRAGMA journal_mode=WAL");
        query(m_db, "PRAGMA synchronous=FULL");
        migrate();
    } catch (...) {
        m_db.close(); m_db = {}; QSqlDatabase::removeDatabase(m_connectionName); throw;
    }
}
Database::~Database() { m_db.close(); m_db = {}; QSqlDatabase::removeDatabase(m_connectionName); }
void Database::migrate() {
    Transaction tx(m_db);
    query(m_db, "CREATE TABLE IF NOT EXISTS schema_version(version INTEGER NOT NULL)");
    auto q = query(m_db, "SELECT version FROM schema_version");
    int version = nextRow(q) ? q.value(0).toInt() : 0;
    q.finish();
    if (version > 2) error("This database was created by a newer version of LeetRepeat.");
    if (version == 0) {
        const QStringList statements{
            "CREATE TABLE problems(id INTEGER PRIMARY KEY, title TEXT NOT NULL, title_key TEXT NOT NULL, url TEXT NOT NULL, url_key TEXT NOT NULL, difficulty TEXT NOT NULL CHECK(difficulty IN ('Easy','Medium','Hard','Unknown')), notes TEXT NOT NULL DEFAULT '', solution TEXT NOT NULL DEFAULT '', learned INTEGER NOT NULL DEFAULT 0 CHECK(learned IN (0,1)), first_learned TEXT, last_review TEXT, next_review TEXT, scheduler_state TEXT NOT NULL DEFAULT '{}', passes INTEGER NOT NULL DEFAULT 0, failures INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0)",
            "CREATE UNIQUE INDEX problems_url ON problems(url_key) WHERE deleted=0",
            "CREATE UNIQUE INDEX problems_title ON problems(title_key) WHERE deleted=0",
            "CREATE INDEX problems_due ON problems(next_review) WHERE learned=1 AND deleted=0",
            "CREATE TABLE attempts(id INTEGER PRIMARY KEY, problem_id INTEGER NOT NULL REFERENCES problems(id), timestamp TEXT NOT NULL, local_date TEXT NOT NULL, result TEXT NOT NULL CHECK(result IN ('PASS','FAIL')), state_before TEXT NOT NULL, state_after TEXT NOT NULL, previous_due TEXT NOT NULL, resulting_due TEXT NOT NULL)",
            "CREATE INDEX attempts_problem ON attempts(problem_id, id)",
            "CREATE INDEX attempts_date ON attempts(local_date)",
            "CREATE TRIGGER attempts_no_update BEFORE UPDATE ON attempts BEGIN SELECT RAISE(ABORT, 'Attempt history is immutable'); END",
            "CREATE TRIGGER attempts_no_delete BEFORE DELETE ON attempts BEGIN SELECT RAISE(ABORT, 'Attempt history is immutable'); END",
            "CREATE TABLE learning_events(id INTEGER PRIMARY KEY, problem_id INTEGER NOT NULL REFERENCES problems(id), local_date TEXT NOT NULL, timestamp TEXT NOT NULL)",
            "CREATE INDEX learning_date ON learning_events(local_date)",
            "CREATE TABLE tags(id INTEGER PRIMARY KEY, name TEXT NOT NULL, name_key TEXT NOT NULL UNIQUE)",
            "CREATE TABLE problem_tags(problem_id INTEGER NOT NULL REFERENCES problems(id), tag_id INTEGER NOT NULL REFERENCES tags(id), PRIMARY KEY(problem_id, tag_id))",
            "CREATE INDEX problem_tags_tag ON problem_tags(tag_id, problem_id)",
            "CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)",
            "INSERT INTO schema_version VALUES(1)"
        };
        for (const auto &sql : statements) query(m_db, sql);
    }
    if (version < 2) {
        // Existing databases retain their order and all saved data until explicitly shuffled.
        query(m_db, "ALTER TABLE problems ADD COLUMN study_order INTEGER NOT NULL DEFAULT 0");
        query(m_db, "UPDATE problems SET study_order=id");
        query(m_db, "CREATE INDEX problems_study_order ON problems(study_order,id)");
        query(m_db, "UPDATE schema_version SET version=2");
    }
    tx.commit();
}
void Database::backup(const QString &destination) const {
    if (destination.isEmpty() || QFileInfo::exists(destination)) error("Choose a new backup filename; existing files are never overwritten.");
    // SQLite creates a consistent standalone snapshot, including committed WAL data.
    query(m_db, "VACUUM INTO ?", {destination});
}
}
