#include "YoloModelRegistry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {

QJsonObject toJson(const YoloModelRecord& model)
{
    QJsonArray classes;
    for (const QString& name : model.classNames) {
        classes.append(name);
    }
    return {
        {QStringLiteral("id"), model.id},
        {QStringLiteral("name"), model.name},
        {QStringLiteral("file_path"), model.filePath},
        {QStringLiteral("task"), yoloTaskKey(model.task)},
        {QStringLiteral("format"), model.format},
        {QStringLiteral("source"), model.source},
        {QStringLiteral("created_at"), model.createdAt},
        {QStringLiteral("last_used_at"), model.lastUsedAt},
        {QStringLiteral("classes"), classes},
        {QStringLiteral("metrics"), model.metrics},
        {QStringLiteral("active"), model.active},
    };
}

YoloModelRecord fromJson(const QJsonObject& object)
{
    YoloModelRecord model;
    model.id = object.value(QStringLiteral("id")).toString();
    model.name = object.value(QStringLiteral("name")).toString();
    model.filePath = object.value(QStringLiteral("file_path")).toString();
    model.task = yoloTaskFromKey(object.value(QStringLiteral("task")).toString());
    model.format = object.value(QStringLiteral("format")).toString();
    model.source = object.value(QStringLiteral("source")).toString();
    model.createdAt = object.value(QStringLiteral("created_at")).toString();
    model.lastUsedAt = object.value(QStringLiteral("last_used_at")).toString();
    for (const QJsonValue& value : object.value(QStringLiteral("classes")).toArray()) {
        model.classNames.push_back(value.toString());
    }
    model.metrics = object.value(QStringLiteral("metrics")).toObject();
    model.active = object.value(QStringLiteral("active")).toBool();
    return model;
}

QString safeBaseName(QString value)
{
    for (qsizetype index = 0; index < value.size(); ++index) {
        QChar character = value.at(index);
        if (!character.isLetterOrNumber() && character != QLatin1Char('-') && character != QLatin1Char('_')) {
            value[index] = QLatin1Char('_');
        }
    }
    return value.left(80);
}

} // namespace

QString yoloTaskKey(YoloTask task)
{
    switch (task) {
    case YoloTask::Classification:
        return QStringLiteral("classify");
    case YoloTask::Segmentation:
        return QStringLiteral("segment");
    case YoloTask::Detection:
    default:
        return QStringLiteral("detect");
    }
}

QString yoloTaskDisplayName(YoloTask task)
{
    switch (task) {
    case YoloTask::Classification:
        return QStringLiteral("图像分类");
    case YoloTask::Segmentation:
        return QStringLiteral("实例分割");
    case YoloTask::Detection:
    default:
        return QStringLiteral("目标检测");
    }
}

YoloTask yoloTaskFromKey(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("classify") || normalized == QStringLiteral("classification")) {
        return YoloTask::Classification;
    }
    if (normalized == QStringLiteral("segment") || normalized == QStringLiteral("segmentation")) {
        return YoloTask::Segmentation;
    }
    return YoloTask::Detection;
}

YoloModelRegistry::YoloModelRegistry(QString rootDirectory)
{
    if (rootDirectory.isEmpty()) {
        rootDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/models/yolo");
    }
    root_directory_ = QDir::cleanPath(QFileInfo(rootDirectory).absoluteFilePath());
}

QString YoloModelRegistry::modelsDirectory() const
{
    return QDir(root_directory_).filePath(QStringLiteral("weights"));
}

QString YoloModelRegistry::registryPath() const
{
    return QDir(root_directory_).filePath(QStringLiteral("registry.json"));
}

bool YoloModelRegistry::load(QString* error)
{
    models_.clear();
    QFile file(registryPath());
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parse_error.errorString();
        return false;
    }
    for (const QJsonValue& value : document.object().value(QStringLiteral("models")).toArray()) {
        YoloModelRecord model = fromJson(value.toObject());
        if (!model.id.isEmpty() && !model.filePath.isEmpty()) {
            models_.push_back(std::move(model));
        }
    }
    return true;
}

bool YoloModelRegistry::save(QString* error) const
{
    if (!QDir().mkpath(root_directory_)) {
        if (error) *error = QStringLiteral("无法创建模型库目录：%1").arg(root_directory_);
        return false;
    }
    QJsonArray models;
    for (const YoloModelRecord& model : models_) {
        models.append(toJson(model));
    }
    QSaveFile file(registryPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("models"), models},
    }).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

const YoloModelRecord* YoloModelRegistry::find(const QString& id) const
{
    const int index = indexOf(id);
    return index >= 0 ? &models_[index] : nullptr;
}

const YoloModelRecord* YoloModelRegistry::activeModel() const
{
    for (const YoloModelRecord& model : models_) {
        if (model.active) {
            return &model;
        }
    }
    return nullptr;
}

bool YoloModelRegistry::importModel(
    const QString& sourcePath,
    YoloTask task,
    bool copyIntoLibrary,
    YoloModelRecord* imported,
    QString* error)
{
    return addModel(sourcePath, task, QFileInfo(sourcePath).completeBaseName(),
        QStringLiteral("imported"), {}, copyIntoLibrary, imported, error);
}

bool YoloModelRegistry::registerTrainingArtifact(
    const QString& artifactPath,
    YoloTask task,
    const QString& displayName,
    const QJsonObject& metrics,
    YoloModelRecord* imported,
    QString* error)
{
    return addModel(artifactPath, task, displayName, QStringLiteral("trained"), metrics,
        true, imported, error);
}

bool YoloModelRegistry::addModel(
    const QString& sourcePath,
    YoloTask task,
    const QString& displayName,
    const QString& source,
    const QJsonObject& metrics,
    bool copyIntoLibrary,
    YoloModelRecord* imported,
    QString* error)
{
    const QFileInfo input(sourcePath);
    const QString extension = input.suffix().toLower();
    if (!input.exists() || !input.isFile()) {
        if (error) *error = QStringLiteral("模型文件不存在：%1").arg(sourcePath);
        return false;
    }
    if (extension != QStringLiteral("pt") && extension != QStringLiteral("onnx")) {
        if (error) *error = QStringLiteral("仅支持 .pt 和 .onnx YOLO 模型。");
        return false;
    }

    YoloModelRecord model;
    model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    model.name = displayName.trimmed().isEmpty() ? input.completeBaseName() : displayName.trimmed();
    model.task = task;
    model.format = extension;
    model.source = source;
    model.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    model.metrics = metrics;
    model.active = models_.isEmpty();

    if (copyIntoLibrary) {
        if (!QDir().mkpath(modelsDirectory())) {
            if (error) *error = QStringLiteral("无法创建模型权重目录。");
            return false;
        }
        const QString target_name = QStringLiteral("%1_%2.%3")
            .arg(model.id.left(8), safeBaseName(model.name), extension);
        const QString target_path = QDir(modelsDirectory()).filePath(target_name);
        if (!QFile::copy(input.absoluteFilePath(), target_path)) {
            if (error) *error = QStringLiteral("复制模型到模型库失败：%1").arg(target_path);
            return false;
        }
        model.filePath = QFileInfo(target_path).absoluteFilePath();
    } else {
        model.filePath = input.absoluteFilePath();
    }

    models_.push_back(model);
    if (!save(error)) {
        models_.removeLast();
        if (copyIntoLibrary) QFile::remove(model.filePath);
        return false;
    }
    if (imported) *imported = model;
    return true;
}

bool YoloModelRegistry::updateMetadata(
    const QString& id,
    YoloTask task,
    const QStringList& classNames,
    const QJsonObject& metrics,
    QString* error)
{
    const int index = indexOf(id);
    if (index < 0) {
        if (error) *error = QStringLiteral("模型不存在。");
        return false;
    }
    models_[index].task = task;
    models_[index].classNames = classNames;
    if (!metrics.isEmpty()) models_[index].metrics = metrics;
    return save(error);
}

bool YoloModelRegistry::setActive(const QString& id, QString* error)
{
    const int selected = indexOf(id);
    if (selected < 0) {
        if (error) *error = QStringLiteral("模型不存在。");
        return false;
    }
    for (int index = 0; index < models_.size(); ++index) {
        models_[index].active = index == selected;
        if (index == selected) {
            models_[index].lastUsedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        }
    }
    return save(error);
}

bool YoloModelRegistry::remove(const QString& id, bool deleteManagedFile, QString* error)
{
    const int index = indexOf(id);
    if (index < 0) {
        if (error) *error = QStringLiteral("模型不存在。");
        return false;
    }
    const YoloModelRecord removed = models_[index];
    models_.removeAt(index);
    if (removed.active && !models_.isEmpty()) models_.front().active = true;
    if (!save(error)) {
        models_.insert(index, removed);
        return false;
    }
    if (deleteManagedFile && isManagedPath(removed.filePath)) {
        QFile::remove(removed.filePath);
    }
    return true;
}

YoloTask YoloModelRegistry::inferTaskFromFileName(const QString& path)
{
    const QString name = QFileInfo(path).completeBaseName().toLower();
    if (name.contains(QStringLiteral("-cls")) || name.contains(QStringLiteral("_cls"))) {
        return YoloTask::Classification;
    }
    if (name.contains(QStringLiteral("-seg")) || name.contains(QStringLiteral("_seg"))) {
        return YoloTask::Segmentation;
    }
    return YoloTask::Detection;
}

int YoloModelRegistry::indexOf(const QString& id) const
{
    for (int index = 0; index < models_.size(); ++index) {
        if (models_[index].id == id) return index;
    }
    return -1;
}

bool YoloModelRegistry::isManagedPath(const QString& path) const
{
    const QString managed = QDir::cleanPath(QFileInfo(modelsDirectory()).absoluteFilePath()) + QLatin1Char('/');
    const QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return candidate.startsWith(managed, Qt::CaseInsensitive);
}
