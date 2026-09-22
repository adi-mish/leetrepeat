#include "ProblemListModel.h"
#include <algorithm>
namespace lr {
QVariant ProblemListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row()<0 || index.row()>=m_visible.size() || role!=Qt::UserRole+1) return {};
    return m_visible[index.row()].toVariant();
}
void ProblemListModel::setProblems(QList<Problem> problems) { m_all=std::move(problems); rebuild(); }
void ProblemListModel::filter(const QString &search,const QString &status,const QString &difficulty,const QString &tags,const QString &sort) {
    m_search=search.trimmed(); m_status=status; m_difficulty=difficulty; m_sort=sort;
    m_tags.clear(); for (const auto &t : tags.split(';',Qt::SkipEmptyParts)) m_tags.append(t.simplified().toCaseFolded());
    rebuild();
}
void ProblemListModel::rebuild() {
    beginResetModel(); m_visible.clear(); auto today=QDate::currentDate();
    for (const auto &p : m_all) {
        if (!p.title.contains(m_search,Qt::CaseInsensitive)) continue;
        if (m_status=="Not learned" && p.learned) continue;
        if (m_status=="Learned" && !p.learned) continue;
        if (m_status=="Due" && !p.isDue(today)) continue;
        if (m_status=="Overdue" && (!p.isDue(today) || p.nextReview>=today)) continue;
        if (!m_difficulty.isEmpty() && m_difficulty!="All difficulties" && p.difficulty!=m_difficulty) continue;
        bool match=true;
        for (const auto &tag : m_tags)
            if (std::none_of(p.tags.begin(),p.tags.end(),[&](const auto &t){return t.toCaseFolded()==tag;})) { match=false; break; }
        if (match) m_visible.append(p);
    }
    std::stable_sort(m_visible.begin(),m_visible.end(),[&](const Problem &a,const Problem &b){
        if (m_sort=="Next review" && a.nextReview!=b.nextReview) {
            if (!a.nextReview.isValid()) return false;
            if (!b.nextReview.isValid()) return true;
            return a.nextReview<b.nextReview;
        }
        if (m_sort=="Most failures" && a.failures!=b.failures) return a.failures>b.failures;
        if (m_sort=="Stage" && a.state.stage!=b.state.stage) return a.state.stage>b.state.stage;
        if (m_sort=="Difficulty" && a.difficulty!=b.difficulty) {
            const QStringList order{"Easy","Medium","Hard","Unknown"};
            return order.indexOf(a.difficulty)<order.indexOf(b.difficulty);
        }
        return QString::localeAwareCompare(a.title,b.title)<0;
    });
    endResetModel(); emit countChanged();
}
}
