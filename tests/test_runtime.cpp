#include "platform/RuntimeEnvironment.h"
#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

class RuntimeTest : public QObject {
    Q_OBJECT
    QByteArray originalRuntime, originalDisplay;
    QTemporaryDir fixture;
    int socketFd = -1;
    QString socketPath;
private slots:
    void init() {
        originalRuntime = qgetenv("XDG_RUNTIME_DIR");
        originalDisplay = qgetenv("WAYLAND_DISPLAY");
        QVERIFY(fixture.isValid());
        socketPath = fixture.filePath("wayland-test");
        socketFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(socketFd >= 0);
        sockaddr_un address {};
        address.sun_family = AF_UNIX;
        const auto path = QFile::encodeName(socketPath);
        QVERIFY(path.size() < qsizetype(sizeof(address.sun_path)));
        std::memcpy(address.sun_path, path.constData(), size_t(path.size() + 1));
        QCOMPARE(::bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)), 0);
        qputenv("WAYLAND_DISPLAY", "wayland-test");
    }
    void cleanup() {
        if (originalRuntime.isNull()) qunsetenv("XDG_RUNTIME_DIR");
        else qputenv("XDG_RUNTIME_DIR", originalRuntime);
        if (originalDisplay.isNull()) qunsetenv("WAYLAND_DISPLAY");
        else qputenv("WAYLAND_DISPLAY", originalDisplay);
        if (socketFd >= 0) ::close(socketFd);
        QFile::remove(socketPath);
    }
    void validSessionIsPreserved() {
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(fixture.path()));
        QVERIFY(!lr::prepareWslgRuntime(fixture.path()));
        QCOMPARE(qEnvironmentVariable("XDG_RUNTIME_DIR"), fixture.path());
        QCOMPARE(qgetenv("WAYLAND_DISPLAY"), QByteArray("wayland-test"));
    }
    void unusableRuntime_data() {
        QTest::addColumn<QString>("kind");
        for (const auto *kind : {"missing", "file", "shared", "unset", "relative"})
            QTest::newRow(kind) << QString(kind);
    }
    void unusableRuntime() {
        QFETCH(QString, kind);
        QTemporaryDir candidate;
        QVERIFY(candidate.isValid());
        auto runtime = candidate.path();
        if (kind == "missing") runtime += "/missing";
        if (kind == "relative") runtime = "relative-runtime";
        if (kind == "shared") QCOMPARE(::chmod(QFile::encodeName(runtime).constData(), 0777), 0);
        if (kind == "file") {
            runtime += "/file";
            QFile file(runtime);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(runtime));
        if (kind == "unset") qunsetenv("XDG_RUNTIME_DIR");
        QString fallbackPath;
        {
            const auto fallback = lr::prepareWslgRuntime(fixture.path());
            QVERIFY(fallback);
            fallbackPath = fallback->path();
            QCOMPARE(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation), fallbackPath);
            QCOMPARE(qEnvironmentVariable("WAYLAND_DISPLAY"), socketPath);
            struct stat info {};
            QCOMPARE(::stat(QFile::encodeName(fallbackPath).constData(), &info), 0);
            QCOMPARE(info.st_uid, ::geteuid());
            QCOMPARE(info.st_mode & 0777, mode_t(0700));
            QTemporaryFile buffer(fallbackPath + "/cursor-XXXXXX");
            QVERIFY(buffer.open());
            QCOMPARE(buffer.write("cursor pixels"), qint64(13));
        }
        QVERIFY(!QFileInfo::exists(fallbackPath));
        if (kind == "shared") {
            struct stat info {};
            QCOMPARE(::stat(QFile::encodeName(runtime).constData(), &info), 0);
            QCOMPARE(info.st_mode & 0777, mode_t(0777));
        }
    }
    void existingSocketIsPreserved_data() {
        QTest::addColumn<bool>("absolute");
        QTest::newRow("relative") << false;
        QTest::newRow("absolute") << true;
    }
    void existingSocketIsPreserved() {
        QFETCH(bool, absolute);
        QTemporaryDir otherWslg;
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(fixture.path()));
        if (absolute) qputenv("WAYLAND_DISPLAY", QFile::encodeName(socketPath));
        QCOMPARE(::chmod(QFile::encodeName(fixture.path()).constData(), 0777), 0);
        const auto fallback = lr::prepareWslgRuntime(otherWslg.path());
        QCOMPARE(::chmod(QFile::encodeName(fixture.path()).constData(), 0700), 0);
        QVERIFY(fallback);
        QCOMPARE(qEnvironmentVariable("WAYLAND_DISPLAY"), socketPath);
    }
    void unrelatedSessionIsPreserved() {
        qputenv("XDG_RUNTIME_DIR", "/missing/runtime");
        qputenv("WAYLAND_DISPLAY", "unrelated-display");
        QVERIFY(!lr::prepareWslgRuntime(fixture.path()));
        QCOMPARE(qgetenv("XDG_RUNTIME_DIR"), QByteArray("/missing/runtime"));
        QCOMPARE(qgetenv("WAYLAND_DISPLAY"), QByteArray("unrelated-display"));
    }
};
QTEST_GUILESS_MAIN(RuntimeTest)
#include "test_runtime.moc"
