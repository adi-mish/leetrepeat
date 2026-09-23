#include "controllers/AppController.h"
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QFileInfo>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>

static int startupFailure(QGuiApplication &app, const QString &message, bool smokeTest) {
    std::fprintf(stderr, "LeetRepeat could not start: %s\n", qPrintable(message));
    if (smokeTest) return 1;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("startupError", message);
    engine.load(QUrl(QStringLiteral("qrc:/qml/StartupError.qml")));
    if (!engine.rootObjects().isEmpty()) app.exec();
    return 1;
}

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    // This desktop UI does not scale or rotate text. Use native font rasterization
    // on Wayland instead of Qt Quick's distance-field glyph shader.
    // Set the default before any QML text items (including startup errors) exist.
    if (QGuiApplication::platformName().startsWith("wayland"))
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QCoreApplication::setOrganizationName("LeetRepeat");
    QCoreApplication::setApplicationName("LeetRepeat");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Local problem memorization with spaced repetition");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"database","Use an alternate SQLite database (for testing or a restored copy).","path"});
    parser.addOption({"smoke-test","Load the UI and exit after one second."});
    parser.process(app);
    auto path=parser.value("database");
    if (path.isEmpty()) path=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/leetrepeat.sqlite";
    path=QFileInfo(path).absoluteFilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return startupFailure(app, "Cannot create data directory: " + QFileInfo(path).absolutePath(), parser.isSet("smoke-test"));
    QLockFile lock(path+".lock");
    if (!lock.tryLock())
        return startupFailure(app, "The database is already in use, or its lock cannot be created: " + path, parser.isSet("smoke-test"));
    try {
        lr::Database database(path);
        lr::Repository repository(database);
        lr::AppController controller(repository);
        qmlRegisterUncreatableType<lr::ProblemListModel>("LeetRepeat",1,0,"ProblemListModel","Provided by the application");
        qmlRegisterUncreatableType<lr::ReviewController>("LeetRepeat",1,0,"ReviewController","Provided by the application");
        if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) QQuickStyle::setStyle("Fusion");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app",&controller);
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (engine.rootObjects().isEmpty()) return 1;
        if (parser.isSet("smoke-test")) QTimer::singleShot(1000,&app,&QCoreApplication::quit);
        return app.exec();
    } catch (const std::exception &e) {
        return startupFailure(app, QString::fromUtf8(e.what()) + "\nDatabase: " + path, parser.isSet("smoke-test"));
    }
}
