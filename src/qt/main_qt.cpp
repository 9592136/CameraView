#include "CameraMainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("CameraView"));
    application.setApplicationDisplayName(QStringLiteral("CameraView · Qt"));
    application.setOrganizationName(QStringLiteral("CameraView"));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CameraView Qt industrial camera application"));
    parser.addHelpOption();
    const QCommandLineOption smoke_test(
        QStringLiteral("smoke-test"),
        QStringLiteral("Initialize the Qt application and exit automatically."));
    parser.addOption(smoke_test);
    parser.process(application);

    CameraMainWindow window;
    if (parser.isSet(smoke_test)) {
        QTimer::singleShot(750, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}
