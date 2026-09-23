#include "Problem.h"
#include <QJsonDocument>
#include <QSet>
#include <QUrl>
#include <stdexcept>
#include <algorithm>

namespace lr {
QJsonObject SchedulerState::toJson() const {
    return {{"stage", stage}, {"scheduler", name}, {"version", version}, {"metadata", metadata}};
}
SchedulerState SchedulerState::fromJson(const QJsonObject &j) {
    return {j.value("stage").toInt(), j.value("scheduler").toString("fixed"),
            j.value("version").toInt(1), j.value("metadata").toObject()};
}
QVariantMap Problem::toVariant() const {
    return {{"id", id}, {"title", title}, {"url", url}, {"difficulty", difficulty},
            {"notes", notes}, {"solution", solution}, {"tags", tags}, {"learned", learned},
            {"firstLearned", firstLearned.toString(Qt::ISODate)}, {"lastReview", lastReview.toString(Qt::ISODate)},
            {"nextReview", nextReview.toString(Qt::ISODate)}, {"stage", state.stage},
            {"schedulerState", QString::fromUtf8(QJsonDocument(state.toJson()).toJson(QJsonDocument::Compact))},
            {"passes", passes}, {"failures", failures}, {"createdAt", createdAt}, {"updatedAt", updatedAt}};
}
QString normalizedTitle(const QString &s) { return s.simplified().toCaseFolded(); }
QString normalizedUrl(const QString &s) {
    QUrl url(s.trimmed());
    url.setScheme(url.scheme().toLower());
    url.setHost(url.host().toLower());
    url.setFragment({});
    url.setQuery(QString{});
    QString path = url.path();
    while (path.endsWith('/')) path.chop(1);
    url.setPath(path);
    return url.toString(QUrl::FullyEncoded);
}
QStringList normalizedTags(const QStringList &tags) {
    QStringList result;
    QSet<QString> seen;
    for (const auto &tag : tags) {
        auto name = tag.simplified();
        if (!name.isEmpty() && !seen.contains(name.toCaseFolded())) {
            if (name.contains(';')) throw std::runtime_error("Tags cannot contain semicolons.");
            seen.insert(name.toCaseFolded()); result.append(name);
        }
    }
    return result;
}
void validateProblem(Problem &p) {
    p.title = p.title.trimmed(); p.url = p.url.trimmed();
    QUrl url(p.url);
    if (p.title.isEmpty()) throw std::runtime_error("A problem title is required.");
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "https" && url.scheme() != "http"))
        throw std::runtime_error("Enter a valid http or https problem URL.");
    const QStringList difficulties{"Easy", "Medium", "Hard", "Unknown"};
    const auto it = std::find_if(difficulties.begin(), difficulties.end(), [&](const auto &d) {
        return d.compare(p.difficulty.trimmed(), Qt::CaseInsensitive) == 0;
    });
    if (it == difficulties.end()) throw std::runtime_error("Difficulty must be Easy, Medium, Hard, or Unknown.");
    p.difficulty = *it;
    p.tags = normalizedTags(p.tags);
}
}
