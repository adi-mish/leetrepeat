#pragma once
#include <QAbstractListModel>
#include "domain/Problem.h"
namespace lr {
class ProblemListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    explicit ProblemListModel(QObject *parent=nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent={}) const override { return parent.isValid() ? 0 : int(m_visible.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int,QByteArray> roleNames() const override { return {{Qt::UserRole+1,"problem"}}; }
    void setProblems(QList<Problem> problems);
    Q_INVOKABLE void filter(const QString &search, const QString &status, const QString &difficulty,
                            const QString &tags, const QString &sort);
signals:
    void countChanged();
private:
    void rebuild();
    QList<Problem> m_all,m_visible;
    QString m_search,m_status,m_difficulty,m_sort;
    QStringList m_tags;
};
}
