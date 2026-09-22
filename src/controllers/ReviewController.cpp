#include "ReviewController.h"
#include "services/DailyQueue.h"
#include "scheduling/FixedIntervalScheduler.h"
#include <QDesktopServices>
#include <QUrl>
namespace lr {
bool ReviewController::start() {
    try {
        auto today=QDate::currentDate();
        m_queue=DailyQueue::build(m_repo.problems(),today,m_repo.settings(),m_repo.learnedOn(today)).combined();
        m_completed=0; advance(); return true;
    } catch (const std::exception &e) { emit error(QString::fromUtf8(e.what())); return false; }
}
void ReviewController::advance() {
    m_revealed=false; m_active=!m_queue.isEmpty();
    m_current=m_active ? m_repo.problem(m_queue.first()) : Problem{};
    emit changed();
}
bool ReviewController::saveStudy(const QString &notes,const QString &solution) {
    try {
        if (!m_active || m_current.learned) return false;
        auto updated=m_repo.problem(m_current.id); updated.notes=notes; updated.solution=solution;
        m_repo.edit(updated); m_current=updated; emit persisted(); emit changed(); return true;
    } catch (const std::exception &e) { emit error(QString::fromUtf8(e.what())); return false; }
}
bool ReviewController::markLearned(const QString &notes,const QString &solution) {
    if (!saveStudy(notes,solution)) return false;
    try {
        auto s=m_repo.settings(); FixedIntervalScheduler scheduler(s.intervals,s.jitter);
        m_repo.learn(m_current.id,scheduler,QDateTime::currentDateTime(),s.newPerDay);
        m_queue.removeFirst(); ++m_completed; emit persisted(); advance(); return true;
    } catch (const std::exception &e) { emit error(QString::fromUtf8(e.what())); return false; }
}
bool ReviewController::grade(bool passed) {
    try {
        if (!m_active || !m_current.learned) return false;
        auto s=m_repo.settings(); FixedIntervalScheduler scheduler(s.intervals,s.jitter);
        m_repo.review(m_current.id,passed,scheduler,QDateTime::currentDateTime());
        m_queue.removeFirst(); ++m_completed; emit persisted(); advance(); return true;
    } catch (const std::exception &e) { emit error(QString::fromUtf8(e.what())); return false; }
}
void ReviewController::end() { m_queue.clear(); m_active=false; m_revealed=false; emit changed(); }
bool ReviewController::openUrl() {
    if (m_active && QDesktopServices::openUrl(QUrl(m_current.url))) return true;
    emit error("Could not open the problem URL in your browser."); return false;
}
}
