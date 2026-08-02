#pragma once

#include "YoloModelRegistry.h"

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

struct YoloDatasetAnnotation {
    int classId = -1;
    QVector<QPointF> points;
};

struct YoloDatasetSample {
    QString id;
    QString sourceName;
    QString split;
    QString imageRelativePath;
    QString labelRelativePath;
    QSize imageSize;
    QVector<YoloDatasetAnnotation> annotations;
};

class YoloDatasetProject {
public:
    static constexpr int FormatVersion = 1;

    static bool create(
        const QString& rootDirectory,
        const QString& name,
        YoloTask task,
        const QStringList& classes,
        YoloDatasetProject* project,
        QString* error = nullptr);

    bool load(const QString& rootDirectory, QString* error = nullptr);
    bool isOpen() const { return !root_directory_.isEmpty(); }
    QString rootDirectory() const { return root_directory_; }
    QString manifestPath() const;
    QString trainingDataPath() const;
    QString name() const { return name_; }
    YoloTask task() const { return task_; }
    const QStringList& classes() const { return classes_; }
    const QVector<YoloDatasetSample>& samples() const { return samples_; }

    bool addClass(const QString& name, QString* error = nullptr);
    bool renameClass(int index, const QString& name, QString* error = nullptr);
    bool removeClass(int index, QString* error = nullptr);

    bool saveSample(
        const QImage& image,
        const QString& sourceName,
        const QString& split,
        const QVector<YoloDatasetAnnotation>& annotations,
        const QString& existingId,
        QString* savedId,
        QString* error = nullptr);
    bool loadSample(
        const QString& id,
        QImage* image,
        QVector<YoloDatasetAnnotation>* annotations,
        QString* error = nullptr) const;
    bool removeSample(const QString& id, QString* error = nullptr);
    int sampleIndex(const QString& id) const;
    QString absoluteImagePath(const YoloDatasetSample& sample) const;
    bool validate(QString* error = nullptr) const;

    static bool isValidSplit(const QString& split);
    static QString normalizedSplit(const QString& split);

private:
    bool writeProject(QString* error = nullptr) const;
    bool ensureLayout(QString* error = nullptr) const;
    bool writeManifest(QString* error = nullptr) const;
    bool writeDatasetYaml(QString* error = nullptr) const;
    bool writeLabelFile(const YoloDatasetSample& sample, QString* error = nullptr) const;
    bool normalizeAnnotations(
        const QSize& imageSize,
        const QVector<YoloDatasetAnnotation>& input,
        QVector<YoloDatasetAnnotation>* output,
        QString* error = nullptr) const;
    QString classDirectory(int classId) const;
    static bool validateClassList(const QStringList& classes, QString* error = nullptr);
    static bool validateClassName(const QString& name, QString* error = nullptr);
    static bool isSafeRelativePath(const QString& path);
    static void setError(QString* error, const QString& message);

    QString root_directory_;
    QString name_;
    YoloTask task_ = YoloTask::Detection;
    QStringList classes_;
    QVector<YoloDatasetSample> samples_;
};
