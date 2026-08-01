#include "qt/ai/YoloModelRegistry.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

namespace {

bool require(bool value, const char* message)
{
    if (!value) std::cerr << "FAILED: " << message << '\n';
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!require(temporary.isValid(), "temporary directory")) return 1;

    const QString source = temporary.filePath(QStringLiteral("sample-seg.pt"));
    QFile model(source);
    if (!require(model.open(QIODevice::WriteOnly), "create source model")) return 1;
    model.write("fake-yolo-model");
    model.close();

    const QString library = temporary.filePath(QStringLiteral("library"));
    YoloModelRegistry registry(library);
    YoloModelRecord imported;
    QString error;
    if (!require(registry.importModel(source, YoloTask::Segmentation, true, &imported, &error),
            "import model")) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }
    if (!require(imported.active, "first model is active") ||
        !require(QFile::exists(imported.filePath), "managed model copied") ||
        !require(YoloModelRegistry::inferTaskFromFileName(source) == YoloTask::Segmentation,
            "task inferred from file name")) return 1;

    if (!require(registry.updateMetadata(imported.id, YoloTask::Segmentation,
            {QStringLiteral("cell"), QStringLiteral("tissue")}, {}, &error), "update metadata")) return 1;

    YoloModelRegistry reloaded(library);
    if (!require(reloaded.load(&error), "reload registry")) return 1;
    const YoloModelRecord* restored = reloaded.find(imported.id);
    if (!require(restored != nullptr, "model restored") ||
        !require(restored->classNames.size() == 2, "class names restored") ||
        !require(reloaded.activeModel() != nullptr, "active model restored")) return 1;

    const QString managed_path = restored->filePath;
    if (!require(reloaded.remove(restored->id, true, &error), "remove model") ||
        !require(!QFile::exists(managed_path), "managed model removed")) return 1;

    std::cout << "YOLO model registry tests passed\n";
    return 0;
}
