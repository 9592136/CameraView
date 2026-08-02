#include "CameraMainWindow.h"
#include "CameraViewTheme.h"
#include "ai/YoloModelRegistry.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QIcon>
#include <QPixmap>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>

namespace {

int importYoloManifest(const QString& manifestPath)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical().noquote() << QStringLiteral("Cannot open YOLO manifest: %1").arg(file.errorString());
        return 2;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCritical().noquote() << QStringLiteral("Invalid YOLO manifest: %1").arg(parseError.errorString());
        return 2;
    }

    YoloModelRegistry registry;
    QString error;
    if (!registry.load(&error)) {
        qCritical().noquote() << QStringLiteral("Cannot load YOLO registry: %1").arg(error);
        return 2;
    }

    const QJsonArray entries = document.object().value(QStringLiteral("models")).toArray();
    if (entries.isEmpty()) {
        qCritical() << "YOLO manifest contains no models.";
        return 2;
    }
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        const QString path = QFileInfo(entry.value(QStringLiteral("path")).toString()).absoluteFilePath();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const YoloTask task = yoloTaskFromKey(entry.value(QStringLiteral("task")).toString());
        const QJsonObject metrics = entry.value(QStringLiteral("metrics")).toObject();
        QStringList classes;
        for (const QJsonValue& classValue : entry.value(QStringLiteral("classes")).toArray()) {
            classes.push_back(classValue.toString());
        }

        const YoloModelRecord* existing = nullptr;
        for (const YoloModelRecord& model : registry.models()) {
            if (model.name == name && model.task == task) {
                existing = &model;
                break;
            }
        }

        QString id;
        if (existing) {
            id = existing->id;
            if (!registry.updateMetadata(id, task, classes, metrics, &error)) {
                qCritical().noquote() << QStringLiteral("Cannot update %1: %2").arg(name, error);
                return 2;
            }
            qInfo().noquote() << QStringLiteral("Updated YOLO model: %1").arg(name);
        } else {
            YoloModelRecord imported;
            if (!registry.registerTrainingArtifact(path, task, name, metrics, &imported, &error)
                || !registry.updateMetadata(imported.id, task, classes, metrics, &error)) {
                qCritical().noquote() << QStringLiteral("Cannot import %1: %2").arg(name, error);
                return 2;
            }
            id = imported.id;
            qInfo().noquote() << QStringLiteral("Imported YOLO model: %1").arg(name);
        }
        if (entry.value(QStringLiteral("active")).toBool() && !registry.setActive(id, &error)) {
            qCritical().noquote() << QStringLiteral("Cannot activate %1: %2").arg(name, error);
            return 2;
        }
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("CameraView"));
    application.setApplicationDisplayName(QStringLiteral("CameraView · Qt"));
    application.setOrganizationName(QStringLiteral("CameraView"));
    const QIcon application_icon(QStringLiteral(":/icons/CameraView.png"));
    if (application_icon.isNull()) {
        qCritical() << "CameraView application icon resource is missing.";
        return 4;
    }
    application.setWindowIcon(application_icon);
    applyCameraViewTheme(application);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CameraView Qt industrial camera application"));
    parser.addHelpOption();
    const QCommandLineOption smoke_test(
        QStringLiteral("smoke-test"),
        QStringLiteral("Initialize the Qt application and exit automatically."));
    const QCommandLineOption import_yolo_manifest(
        QStringLiteral("import-yolo-manifest"),
        QStringLiteral("Import trained YOLO models from a JSON manifest and exit."),
        QStringLiteral("path"));
    const QCommandLineOption ui_snapshot(
        QStringLiteral("ui-snapshot"),
        QStringLiteral("Render the main window to an image and exit."),
        QStringLiteral("path"));
    const QCommandLineOption workspace_tab(
        QStringLiteral("workspace-tab"),
        QStringLiteral("Select the workspace tab by zero-based index."),
        QStringLiteral("index"));
    const QCommandLineOption focus_widget(
        QStringLiteral("focus-widget"),
        QStringLiteral("Scroll a named widget into view before rendering a UI snapshot."),
        QStringLiteral("object-name"));
    parser.addOption(smoke_test);
    parser.addOption(import_yolo_manifest);
    parser.addOption(ui_snapshot);
    parser.addOption(workspace_tab);
    parser.addOption(focus_widget);
    parser.process(application);

    if (parser.isSet(import_yolo_manifest)) {
        return importYoloManifest(parser.value(import_yolo_manifest));
    }

    CameraMainWindow window;
    if (parser.isSet(workspace_tab)) {
        bool valid = false;
        const int index = parser.value(workspace_tab).toInt(&valid);
        if (QTabWidget* tabs = window.findChild<QTabWidget*>(QStringLiteral("FunctionTabs"));
            valid && tabs && index >= 0 && index < tabs->count()) {
            tabs->setCurrentIndex(index);
        }
    }
    if (parser.isSet(ui_snapshot)) {
        window.show();
        if (parser.isSet(focus_widget)) {
            if (QWidget* target = window.findChild<QWidget*>(parser.value(focus_widget))) {
                QWidget* ancestor = target->parentWidget();
                while (ancestor) {
                    if (auto* scroll = qobject_cast<QScrollArea*>(ancestor)) {
                        scroll->ensureWidgetVisible(target, 12, 12);
                        break;
                    }
                    ancestor = ancestor->parentWidget();
                }
            }
        }
        const QString snapshot_path = parser.value(ui_snapshot);
        QTimer::singleShot(1000, &application, [&application, &window, snapshot_path] {
            const bool saved = window.grab().save(snapshot_path);
            if (!saved) qCritical().noquote() << QStringLiteral("Cannot save UI snapshot: %1").arg(snapshot_path);
            application.exit(saved ? 0 : 3);
        });
    } else if (parser.isSet(smoke_test)) {
        QTimer::singleShot(750, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}
