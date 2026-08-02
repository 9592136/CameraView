#include "qt/ai/YoloDatasetProject.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

namespace {

int fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return 1;
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QImage testImage()
{
    QImage image(100, 50, QImage::Format_RGB32);
    image.fill(QColor(80, 40, 120));
    return image;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return fail(QStringLiteral("Could not create a temporary directory."));

    QString error;
    YoloDatasetProject detection;
    const QString detection_root = temporary.filePath(QStringLiteral("detection"));
    if (!YoloDatasetProject::create(detection_root, QStringLiteral("Cells"), YoloTask::Detection,
            {QStringLiteral("cell"), QStringLiteral("debris")}, &detection, &error)) {
        return fail(QStringLiteral("Detection project creation failed: %1").arg(error));
    }
    QString sample_id;
    if (!detection.saveSample(testImage(), QStringLiteral("source.png"), QStringLiteral("train"),
            {{0, {QPointF(10.0, 10.0), QPointF(30.0, 20.0)}}}, {}, &sample_id, &error)) {
        return fail(QStringLiteral("Detection sample save failed: %1").arg(error));
    }
    const YoloDatasetSample detection_sample = detection.samples().first();
    const QString train_image = detection.absoluteImagePath(detection_sample);
    const QString train_label = QDir(detection.rootDirectory()).filePath(detection_sample.labelRelativePath);
    if (!QFileInfo::exists(train_image) ||
        readText(train_label) != QStringLiteral("0 0.200000 0.300000 0.200000 0.200000\n") ||
        !readText(detection.trainingDataPath()).contains(QStringLiteral("0: 'cell'"))) {
        return fail(QStringLiteral("Detection YOLO files are incorrect."));
    }
    if (detection.saveSample(testImage(), QStringLiteral("source.png"), QStringLiteral("val"),
            {{1, {QPointF(20.0, 5.0), QPointF(60.0, 25.0)}}}, sample_id, nullptr, &error) == false) {
        return fail(QStringLiteral("Detection sample update failed: %1").arg(error));
    }
    if (QFileInfo::exists(train_image) || detection.samples().first().split != QStringLiteral("val") ||
        !QFileInfo::exists(detection.absoluteImagePath(detection.samples().first()))) {
        return fail(QStringLiteral("Detection sample split update left incorrect files."));
    }
    if (detection.removeClass(1, &error)) {
        return fail(QStringLiteral("A class still used by annotations was removed."));
    }
    if (!detection.removeClass(0, &error) || detection.classes() != QStringList{QStringLiteral("debris")} ||
        detection.samples().first().annotations.first().classId != 0) {
        return fail(QStringLiteral("Unused class removal did not remap later class IDs."));
    }

    YoloDatasetProject reloaded;
    if (!reloaded.load(detection_root, &error) || !reloaded.validate(&error) ||
        reloaded.samples().size() != 1 || reloaded.samples().first().annotations.first().classId != 0) {
        return fail(QStringLiteral("Detection project reload failed: %1").arg(error));
    }
    QImage loaded_image;
    QVector<YoloDatasetAnnotation> loaded_annotations;
    if (!reloaded.loadSample(sample_id, &loaded_image, &loaded_annotations, &error) ||
        loaded_image.size() != QSize(100, 50) || loaded_annotations.size() != 1) {
        return fail(QStringLiteral("Detection sample reload failed: %1").arg(error));
    }
    const QString reloaded_image_path = reloaded.absoluteImagePath(reloaded.samples().first());
    if (!reloaded.removeSample(sample_id, &error) || QFileInfo::exists(reloaded_image_path) ||
        !reloaded.samples().isEmpty()) {
        return fail(QStringLiteral("Detection sample removal failed: %1").arg(error));
    }

    YoloDatasetProject segmentation;
    const QString segmentation_root = temporary.filePath(QStringLiteral("segmentation"));
    if (!YoloDatasetProject::create(segmentation_root, QStringLiteral("Masks"), YoloTask::Segmentation,
            {QStringLiteral("nucleus")}, &segmentation, &error) ||
        !segmentation.saveSample(testImage(), QStringLiteral("mask.png"), QStringLiteral("test"),
            {{0, {QPointF(10.0, 10.0), QPointF(30.0, 10.0), QPointF(20.0, 25.0)}}},
            {}, nullptr, &error)) {
        return fail(QStringLiteral("Segmentation project save failed: %1").arg(error));
    }
    const QString segmentation_label = QDir(segmentation.rootDirectory()).filePath(
        segmentation.samples().first().labelRelativePath);
    if (readText(segmentation_label) !=
        QStringLiteral("0 0.100000 0.200000 0.300000 0.200000 0.200000 0.500000\n")) {
        return fail(QStringLiteral("Segmentation YOLO polygon is incorrect."));
    }
    QString negative_id;
    if (!segmentation.saveSample(testImage(), QStringLiteral("background.png"), QStringLiteral("val"),
            {}, {}, &negative_id, &error)) {
        return fail(QStringLiteral("A valid negative segmentation sample was rejected: %1").arg(error));
    }
    const YoloDatasetSample& negative_sample = segmentation.samples().last();
    if (!readText(QDir(segmentation.rootDirectory()).filePath(negative_sample.labelRelativePath)).isEmpty()) {
        return fail(QStringLiteral("Negative sample label file is not empty."));
    }

    YoloDatasetProject classification;
    const QString classification_root = temporary.filePath(QStringLiteral("classification"));
    if (!YoloDatasetProject::create(classification_root, QStringLiteral("Classes"), YoloTask::Classification,
            {QStringLiteral("tumor cell"), QStringLiteral("normal")}, &classification, &error) ||
        !classification.saveSample(testImage(), QStringLiteral("class.png"), QStringLiteral("train"),
            {{0, {}}}, {}, nullptr, &error)) {
        return fail(QStringLiteral("Classification project save failed: %1").arg(error));
    }
    if (classification.trainingDataPath() != classification.rootDirectory() ||
        !classification.samples().first().imageRelativePath.startsWith(QStringLiteral("train/tumor cell/")) ||
        !QFileInfo::exists(classification.absoluteImagePath(classification.samples().first())) ||
        classification.renameClass(0, QStringLiteral("renamed"), &error)) {
        return fail(QStringLiteral("Classification dataset structure or class safety is incorrect."));
    }
    const QString old_normal_directory = QDir(classification.rootDirectory()).filePath(QStringLiteral("train/normal"));
    if (!classification.renameClass(1, QStringLiteral("healthy"), &error) ||
        QFileInfo::exists(old_normal_directory) ||
        !QFileInfo(QDir(classification.rootDirectory()).filePath(QStringLiteral("train/healthy"))).isDir() ||
        !classification.removeClass(1, &error) || classification.classes().size() != 1 ||
        QFileInfo::exists(QDir(classification.rootDirectory()).filePath(QStringLiteral("train/healthy")))) {
        return fail(QStringLiteral("Unused classification class rename/removal did not maintain folders."));
    }
    if (classification.saveSample(testImage(), QStringLiteral("invalid.png"), QStringLiteral("train"),
            {}, {}, nullptr, &error)) {
        return fail(QStringLiteral("A sample without annotations was accepted."));
    }

    return 0;
}
