#pragma once
#include "repositories/Repository.h"
#include <QObject>
namespace lr {
class ReviewController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int remaining READ remaining NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(bool revealed READ revealed WRITE setRevealed NOTIFY changed)
public:
    explicit ReviewController(Repository &repository,QObject *parent=nullptr) : QObject(parent),m_repo(repository) {}
    QVariantMap current() const { return m_current.toVariant(); }
    bool active() const { return m_active; }
    int remaining() const { return int(m_queue.size()); }
    int completed() const { return m_completed; }
    bool revealed() const { return m_revealed; }
    void setRevealed(bool value) { m_revealed=value; emit changed(); }
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool markLearned(const QString &notes,const QString &solution);
    Q_INVOKABLE bool grade(bool passed);
    Q_INVOKABLE bool saveStudy(const QString &notes,const QString &solution);
    Q_INVOKABLE void end();
    Q_INVOKABLE bool openUrl();
signals:
    void changed();
    void persisted();
    void error(const QString &message);
private:
    void advance();
    Repository &m_repo;
    QList<qint64> m_queue;
    Problem m_current;
    int m_completed=0;
    bool m_active=false,m_revealed=false;
};
}
