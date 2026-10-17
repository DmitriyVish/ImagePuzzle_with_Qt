#include "ui/mainwindow.h"
#include "selfcheck.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QCommandLineParser>

Q_LOGGING_CATEGORY(lcStartup, "imagepuzzle.app")

/**
 * @brief Создаёт QApplication и главное окно.
 * @param argc Количество аргументов командной строки.
 * @param argv Аргументы, включая параметры платформы Qt.
 * @return Код завершения цикла событий Qt.
 */
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ImagePuzzle"));
    QCoreApplication::setApplicationVersion(QStringLiteral(IMAGEPUZZLE_VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ImagePuzzle"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("self-test"), QStringLiteral("Check installed image plugins and puzzle logic without a window.")});
    parser.addOption({QStringLiteral("self-test-report"), QStringLiteral("Write the self-check result as JSON."), QStringLiteral("path")});
    parser.process(application);
    if (parser.isSet(QStringLiteral("self-test")))
        return SelfCheck::run(parser.value(QStringLiteral("self-test-report")));
    qCInfo(lcStartup) << "Starting ImagePuzzle, Qt:" << qVersion();
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [] {
        qCInfo(lcStartup) << "ImagePuzzle is shutting down";
    });

    MainWindow window;
    window.show();
    return application.exec();
}
