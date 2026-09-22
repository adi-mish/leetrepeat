#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
namespace lr {
QSqlQuery query(QSqlDatabase db, const QString &sql, const QVariantList &values = {});
class Transaction {
public:
    explicit Transaction(QSqlDatabase db);
    ~Transaction();
    void commit();
    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;
private:
    QSqlDatabase m_db;
    bool m_done = false;
};
class Database {
public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    QSqlDatabase connection() const { return m_db; }
    QString path() const { return m_db.databaseName(); }
    void backup(const QString &destination) const;
private:
    void migrate();
    QString m_connectionName;
    QSqlDatabase m_db;
};
}
