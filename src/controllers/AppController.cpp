#include "AppController.h"
#include "scheduling/FixedIntervalScheduler.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QRegularExpression>
#include <stdexcept>
namespace lr {
AppController::AppController(Repository &repo,QObject *parent) : QObject(parent),m_repo(repo),m_problems(this),m_review(repo,this) {
    connect(&m_review,&ReviewController::persisted,this,&AppController::refresh);
    connect(&m_review,&ReviewController::error,this,[this](const QString &s){showMessage(s,true);});
    m_date=QDate::currentDate();
    connect(&m_timer,&QTimer::timeout,this,[this]{ if (m_date!=QDate::currentDate()) {m_date=QDate::currentDate(); refresh();} });
    m_timer.start(30000); reload();
}
void AppController::showMessage(const QString &s,bool error) { m_message=s; m_error=error; emit messageChanged(); }
void AppController::clearMessage() { showMessage({},false); }
bool AppController::run(const std::function<void()> &operation,const QString &success) {
    try { operation(); if (!success.isEmpty()) showMessage(success,false); return true; }
    catch (const std::exception &e) { showMessage(QString::fromUtf8(e.what()),true); return false; }
}
void AppController::reload() {
    auto all=m_repo.problems(); m_tags.clear();
    for (const auto &p : all) for (const auto &tag : p.tags) if (!m_tags.contains(tag)) m_tags.append(tag);
    m_tags.sort(Qt::CaseInsensitive); m_problems.setProblems(all);
    m_stats=m_repo.stats(QDate::currentDate()); auto s=m_repo.settings();
    QStringList intervals; for (int n : s.intervals) intervals.append(QString::number(n));
    m_settings={{"newPerDay",s.newPerDay},{"intervals",intervals.join(',')},{"shuffle",s.shuffle},{"jitter",s.jitter}};
    emit refreshed();
}
bool AppController::refresh() { return run([&]{reload();}); }
bool AppController::selectProblem(qint64 id) {
    return run([&]{m_detail=m_repo.problem(id).toVariant(); m_history=m_repo.history(id); emit detailChanged();});
}
void AppController::newProblem() { m_detail=Problem{}.toVariant(); m_history.clear(); emit detailChanged(); }
bool AppController::saveProblem(const QVariantMap &fields) {
    return run([&]{
        qint64 id=m_detail.value("id").toLongLong();
        Problem p=id ? m_repo.problem(id) : Problem{};
        p.title=fields.value("title").toString(); p.url=fields.value("url").toString();
        p.difficulty=fields.value("difficulty").toString(); p.notes=fields.value("notes").toString(); p.solution=fields.value("solution").toString();
        p.tags=fields.value("tags").toString().split(';',Qt::SkipEmptyParts);
        if (id) m_repo.edit(p); else id=m_repo.add(p);
        reload(); selectProblem(id);
    },"Problem saved.");
}
bool AppController::resetProblem(qint64 id) { return run([&]{m_repo.reset(id); reload(); selectProblem(id);},"Progress reset. Review history has been retained."); }
bool AppController::deleteProblem(qint64 id) { return run([&]{m_repo.remove(id); reload(); newProblem();},"Problem deleted from the library. Historical attempts have been retained in the database."); }
bool AppController::saveSettings(int newPerDay,const QString &intervals,bool shuffle,bool jitter) {
    return run([&]{
        Settings s; s.newPerDay=newPerDay; s.shuffle=shuffle; s.jitter=jitter; s.intervals.clear();
        for (const auto &field : intervals.split(',')) {
            bool ok=false; int value=field.trimmed().toInt(&ok);
            if (!ok || !QRegularExpression("^[0-9]+$").match(field.trimmed()).hasMatch()) throw std::runtime_error("Enter comma-separated positive integer intervals.");
            s.intervals.append(value);
        }
        m_repo.saveSettings(s); reload();
    },"Settings saved. Existing due dates stay in place; future reviews use these intervals.");
}
QVariantMap AppController::importSummary() const {
    return {{"ready",m_preview.ready.size()},{"duplicates",m_preview.duplicates},{"errors",m_preview.errors},
            {"canImport",m_preview.errors.isEmpty() && !m_preview.ready.isEmpty()}};
}
static QString localPath(const QUrl &url) {
    if (!url.isLocalFile()) throw std::runtime_error("Choose a local file.");
    return url.toLocalFile();
}
bool AppController::previewImport(const QUrl &url) {
    m_preview={}; emit importChanged();
    return run([&]{m_preview=CsvService::preview(localPath(url),m_repo.problems()); emit importChanged();});
}
bool AppController::commitImport() {
    return run([&]{
        if (!m_preview.errors.isEmpty() || m_preview.ready.isEmpty()) throw std::runtime_error("Choose a valid CSV and resolve every error before importing.");
        int count=m_repo.importProblems(m_preview.ready); m_preview={}; emit importChanged(); reload();
        showMessage(QString("Imported %1 problems.").arg(count),false);
    });
}
bool AppController::exportCsv(const QUrl &url) { return run([&]{
    auto path=localPath(url);
    // Never let a file export replace the live database or one of its sidecars.
    auto dbPath=QFileInfo(databasePath()).canonicalFilePath();
    auto canonical=QFileInfo(path).canonicalFilePath();
    auto absolute=QFileInfo(path).absoluteFilePath();
    for (const auto &suffix : QStringList{"", "-wal", "-shm", ".lock"}) {
        if ((!canonical.isEmpty() && canonical==dbPath+suffix) || absolute==dbPath+suffix)
            throw std::runtime_error("Cannot export over the active database or its supporting files.");
    }
    CsvService::exportProblems(path,m_repo.problems());
},"CSV exported."); }
bool AppController::backup(const QUrl &url) { return run([&]{m_repo.database().backup(localPath(url));},"Database backup saved."); }
bool AppController::openDatabaseFolder() { return run([&]{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(databasePath()).absolutePath()))) throw std::runtime_error("Could not open the database folder.");
}); }
bool AppController::openProblemUrl(const QString &url) { return run([&]{
    QUrl target(url);
    if ((target.scheme()!="https" && target.scheme()!="http") || !QDesktopServices::openUrl(target)) throw std::runtime_error("Could not open the problem URL.");
}); }
}
