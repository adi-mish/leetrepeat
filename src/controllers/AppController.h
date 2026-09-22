#pragma once
#include "models/ProblemListModel.h"
#include "services/CsvService.h"
#include "ReviewController.h"
#include <QTimer>
#include <QUrl>
#include <functional>
namespace lr {
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(lr::ProblemListModel *problems READ problems CONSTANT)
    Q_PROPERTY(lr::ReviewController *review READ review CONSTANT)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY refreshed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY refreshed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY detailChanged)
    Q_PROPERTY(QStringList tags READ tags NOTIFY refreshed)
    Q_PROPERTY(QVariantList importRows READ importRows NOTIFY importChanged)
    Q_PROPERTY(QVariantMap importSummary READ importSummary NOTIFY importChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)
public:
    explicit AppController(Repository &repository,QObject *parent=nullptr);
    ProblemListModel *problems() { return &m_problems; }
    ReviewController *review() { return &m_review; }
    QVariantMap stats() const { return m_stats; }
    QVariantMap settings() const { return m_settings; }
    QVariantMap detail() const { return m_detail; }
    QVariantList history() const { return m_history; }
    QStringList tags() const { return m_tags; }
    QVariantList importRows() const { return m_preview.rows; }
    QVariantMap importSummary() const;
    QString databasePath() const { return m_repo.database().path(); }
    QString message() const { return m_message; }
    bool messageIsError() const { return m_error; }
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool selectProblem(qint64 id);
    Q_INVOKABLE void newProblem();
    Q_INVOKABLE bool saveProblem(const QVariantMap &fields);
    Q_INVOKABLE bool resetProblem(qint64 id);
    Q_INVOKABLE bool deleteProblem(qint64 id);
    Q_INVOKABLE bool saveSettings(int newPerDay,const QString &intervals,bool shuffle,bool jitter);
    Q_INVOKABLE bool previewImport(const QUrl &url);
    Q_INVOKABLE bool commitImport();
    Q_INVOKABLE bool exportCsv(const QUrl &url);
    Q_INVOKABLE bool backup(const QUrl &url);
    Q_INVOKABLE bool openDatabaseFolder();
    Q_INVOKABLE bool openProblemUrl(const QString &url);
    Q_INVOKABLE void clearMessage();
signals:
    void refreshed();
    void detailChanged();
    void importChanged();
    void messageChanged();
private:
    bool run(const std::function<void()> &operation,const QString &success={});
    void showMessage(const QString &message,bool error);
    void reload();
    Repository &m_repo;
    ProblemListModel m_problems;
    ReviewController m_review;
    QVariantMap m_stats,m_settings,m_detail;
    QVariantList m_history;
    QStringList m_tags;
    ImportPreview m_preview;
    QString m_message;
    bool m_error=false;
    QDate m_date;
    QTimer m_timer;
};
}
