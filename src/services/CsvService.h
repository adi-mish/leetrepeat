#pragma once
#include "domain/Problem.h"
namespace lr {
struct ImportPreview {
    QList<Problem> ready;
    QVariantList rows;
    QStringList errors;
    int duplicates = 0;
};
class CsvService {
public:
    static QList<QStringList> parse(const QString &text);
    static ImportPreview preview(const QString &path, const QList<Problem> &existing);
    static void exportProblems(const QString &path, const QList<Problem> &problems);
};
}
