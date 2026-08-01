#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class YoloTask {
    Detection,
    Classification,
    Segmentation
};

QString yoloTaskKey(YoloTask task);
QString yoloTaskDisplayName(YoloTask task);
YoloTask yoloTaskFromKey(const QString& value);

struct YoloModelRecord {
    QString id;
    QString name;
    QString filePath;
    YoloTask task = YoloTask::Detection;
    QString format;
    QString source;
    QString createdAt;
    QString lastUsedAt;
    QStringList classNames;
    QJsonObject metrics;
    bool active = false;
};

class YoloModelRegistry {
public:
    explicit YoloModelRegistry(QString rootDirectory = {});

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr) const;

    const QVector<YoloModelRecord>& models() const { return models_; }
    const YoloModelRecord* find(const QString& id) const;
    const YoloModelRecord* activeModel() const;
    QString rootDirectory() const { return root_directory_; }
    QString modelsDirectory() const;
    QString registryPath() const;

    bool importModel(
        const QString& sourcePath,
        YoloTask task,
        bool copyIntoLibrary,
        YoloModelRecord* imported = nullptr,
        QString* error = nullptr);
    bool registerTrainingArtifact(
        const QString& artifactPath,
        YoloTask task,
        const QString& displayName,
        const QJsonObject& metrics,
        YoloModelRecord* imported = nullptr,
        QString* error = nullptr);
    bool updateMetadata(
        const QString& id,
        YoloTask task,
        const QStringList& classNames,
        const QJsonObject& metrics,
        QString* error = nullptr);
    bool setActive(const QString& id, QString* error = nullptr);
    bool remove(const QString& id, bool deleteManagedFile, QString* error = nullptr);

    static YoloTask inferTaskFromFileName(const QString& path);

private:
    bool addModel(
        const QString& sourcePath,
        YoloTask task,
        const QString& displayName,
        const QString& source,
        const QJsonObject& metrics,
        bool copyIntoLibrary,
        YoloModelRecord* imported,
        QString* error);
    int indexOf(const QString& id) const;
    bool isManagedPath(const QString& path) const;

    QString root_directory_;
    QVector<YoloModelRecord> models_;
};
