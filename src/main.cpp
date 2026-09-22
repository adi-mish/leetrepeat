#include "controllers/AppController.h"
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
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
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { std::fprintf(stderr,"Cannot create data directory.\n"); return 1; }
    QLockFile lock(path+".lock");
    if (!lock.tryLock()) { std::fprintf(stderr,"The database is already in use, or its lock cannot be created: %s\n",qPrintable(path)); return 1; }
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
        std::fprintf(stderr,"LeetRepeat could not start: %s\nDatabase: %s\n",e.what(),qPrintable(path));
        return 1;
    }
}
