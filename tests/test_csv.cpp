#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "services/CsvService.h"
using namespace lr;
class CsvTest : public QObject {
    Q_OBJECT
private slots:
    void quotingAndRoundTrip() {
        QTemporaryDir dir; auto path=dir.filePath("problems.csv");
        Problem p; p.title="A, \"quoted\" title"; p.url="https://leetcode.com/problems/a/";
        p.notes="Unicode: λ\nSecond line\r\nThird line"; p.solution="return \"a,b\";"; p.tags={"Arrays","Custom"};
        CsvService::exportProblems(path,{p});
        auto preview=CsvService::preview(path,{}); QVERIFY(preview.errors.isEmpty()); QCOMPARE(preview.ready.size(),1);
        QCOMPARE(preview.ready[0].notes,p.notes); QCOMPARE(preview.ready[0].title,p.title); QCOMPARE(preview.ready[0].tags,p.tags);
        auto duplicate=CsvService::preview(path,{p}); QCOMPARE(duplicate.duplicates,1); QVERIFY(duplicate.ready.isEmpty());
    }
    void invalidUtf8() {
        QTemporaryDir dir; auto path=dir.filePath("invalid.csv");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("title,url\nA,https://leetcode.com/problems/a/");
        file.write(QByteArray::fromHex("e282")); file.close();
        QVERIFY_EXCEPTION_THROWN(CsvService::preview(path,{}),std::runtime_error);
    }
    void invalidAndDuplicates() {
        QVERIFY_EXCEPTION_THROWN(CsvService::parse("a,\"broken"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(CsvService::parse("a,\"b\"x"),std::runtime_error);
        QTemporaryDir dir; auto path=dir.filePath("input.csv");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xef\xbb\xbftitle,url,topics\r\nA,https://leetcode.com/problems/a/,Arrays;Hashing\r\n a ,https://leetcode.com/problems/other,Other\r\nB,invalid,Other\r\nC,https://leetcode.com/problems/c,Extra,field\r\n"); file.close();
        auto preview=CsvService::preview(path,{});
        QCOMPARE(preview.ready.size(),1); QCOMPARE(preview.duplicates,1); QCOMPARE(preview.errors.size(),2);
        QCOMPARE(preview.ready[0].tags,QStringList({"Arrays","Hashing"}));
    }
};
QTEST_GUILESS_MAIN(CsvTest)
#include "test_csv.moc"
