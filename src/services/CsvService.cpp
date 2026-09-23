#include "CsvService.h"
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QStringDecoder>
#include <stdexcept>
namespace lr {
static void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
QList<QStringList> CsvService::parse(const QString &input) {
    QString text = input;
    if (text.startsWith(QChar(0xfeff))) text.remove(0,1);
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted=false, closed=false, started=false;
    for (qsizetype i=0; i<text.size(); ++i) {
        QChar c=text[i];
        if (quoted) {
            if (c=='"') {
                if (i+1<text.size() && text[i+1]=='"') { field+='"'; ++i; }
                else { quoted=false; closed=true; }
            } else field+=c;
        } else if (c==',' || c=='\n' || c=='\r') {
            row.append(field); field.clear(); closed=false; started=false;
            if (c!=',') {
                if (c=='\r' && i+1<text.size() && text[i+1]=='\n') ++i;
                if (row.size()!=1 || !row[0].isEmpty()) rows.append(row);
                row.clear();
            }
        } else if (closed) fail("Unexpected text after a closing CSV quote.");
        else if (c=='"') {
            if (started) fail("CSV quote must be at the beginning of a field.");
            quoted=true; started=true;
        } else { field+=c; started=true; }
    }
    if (quoted) fail("Unterminated quoted CSV field.");
    if (started || closed || !row.isEmpty()) { row.append(field); rows.append(row); }
    return rows;
}
ImportPreview CsvService::preview(const QString &path, const QList<Problem> &existing) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail(file.errorString());
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString content=decoder.decode(file.readAll());
    if (decoder.hasError()) fail("CSV must use UTF-8 encoding.");
    auto records=parse(content);
    if (records.isEmpty()) fail("CSV is empty.");
    auto headers=records.takeFirst();
    QHash<QString,int> columns;
    for (int i=0; i<headers.size(); ++i) {
        auto name=headers[i].trimmed().toLower();
        if (columns.contains(name)) fail("Duplicate CSV header: " + name);
        columns.insert(name,i);
    }
    if (!columns.contains("title") || !columns.contains("url")) fail("CSV requires title and url columns.");
    QSet<QString> urls,titles;
    for (const auto &p : existing) { urls.insert(normalizedUrl(p.url)); titles.insert(normalizedTitle(p.title)); }
    ImportPreview result;
    for (qsizetype i=0; i<records.size(); ++i) {
        const auto &row=records[i];
        auto value=[&](const QString &key) { return columns.contains(key) ? row.value(columns.value(key)) : QString{}; };
        Problem p; p.title=value("title"); p.url=value("url"); p.notes=value("notes"); p.solution=value("solution");
        if (!value("difficulty").trimmed().isEmpty()) p.difficulty=value("difficulty");
        p.tags=value("tags").split(';',Qt::SkipEmptyParts) + value("topics").split(';',Qt::SkipEmptyParts);
        QString status="Ready";
        try {
            if (row.size()!=headers.size()) fail("Column count does not match the header.");
            validateProblem(p);
            if (urls.contains(normalizedUrl(p.url)) || titles.contains(normalizedTitle(p.title))) {
                status="Duplicate — skipped"; ++result.duplicates;
            } else {
                result.ready.append(p); urls.insert(normalizedUrl(p.url)); titles.insert(normalizedTitle(p.title));
            }
        } catch (const std::exception &e) {
            status=QString::fromUtf8(e.what());
            result.errors.append(QString("Record %1: %2").arg(i+2).arg(status));
        }
        result.rows.append(QVariantMap{{"record",i+2},{"title",p.title},{"url",p.url},{"status",status}});
    }
    return result;
}
static QString escaped(QString field) { field.replace('"',"\"\""); return '"'+field+'"'; }
void CsvService::exportProblems(const QString &path, const QList<Problem> &problems) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) fail(file.errorString());
    QString data="title,url,difficulty,notes,solution,tags,learned,first_learned,last_review,next_review,scheduler_state,passes,failures,created_at,updated_at\r\n";
    for (const auto &p : problems) {
        QStringList fields{p.title,p.url,p.difficulty,p.notes,p.solution,p.tags.join(';'),p.learned ? "true" : "false",
            p.firstLearned.toString(Qt::ISODate),p.lastReview.toString(Qt::ISODate),p.nextReview.toString(Qt::ISODate),
            p.toVariant().value("schedulerState").toString(),QString::number(p.passes),QString::number(p.failures),p.createdAt,p.updatedAt};
        for (auto &field : fields) field=escaped(field);
        data+=fields.join(',')+"\r\n";
    }
    auto bytes=data.toUtf8();
    if (file.write(bytes)!=bytes.size() || !file.commit()) fail(file.errorString());
}
}
