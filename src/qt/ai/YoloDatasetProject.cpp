#include "YoloDatasetProject.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QPolygonF>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

const QStringList kSplits = {
    QStringLiteral("train"),
    QStringLiteral("val"),
    QStringLiteral("test"),
};

QString yamlQuote(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

QString number(double value)
{
    return QLocale::c().toString(value, 'f', 6);
}

QJsonArray pointsToJson(const QVector<QPointF>& points)
{
    QJsonArray output;
    for (const QPointF& point : points) {
        output.append(QJsonArray{point.x(), point.y()});
    }
    return output;
}

QVector<QPointF> pointsFromJson(const QJsonArray& values)
{
    QVector<QPointF> output;
    output.reserve(values.size());
    for (const QJsonValue& value : values) {
        const QJsonArray point = value.toArray();
        if (point.size() >= 2) {
            output.push_back(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));
        }
    }
    return output;
}

bool writeBytes(const QString& path, const QByteArray& bytes, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace

bool YoloDatasetProject::create(
    const QString& rootDirectory,
    const QString& name,
    YoloTask task,
    const QStringList& classes,
    YoloDatasetProject* project,
    QString* error)
{
    if (!project) {
        setError(error, QStringLiteral("未提供数据集项目输出对象。"));
        return false;
    }
    const QString root = QFileInfo(rootDirectory).absoluteFilePath();
    if (root.trimmed().isEmpty() || name.trimmed().isEmpty()) {
        setError(error, QStringLiteral("数据集目录和名称不能为空。"));
        return false;
    }
    if (!validateClassList(classes, error)) return false;
    if (QFileInfo(QDir(root).filePath(QStringLiteral("dataset.json"))).exists()) {
        setError(error, QStringLiteral("目标目录已经包含 CameraView 数据集。"));
        return false;
    }
    const QDir target_directory(root);
    if (target_directory.exists() && !target_directory.entryList(
            QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        setError(error, QStringLiteral("目标目录不是空目录，请选择其他名称或打开已有数据集。"));
        return false;
    }

    YoloDatasetProject created;
    created.root_directory_ = QDir::cleanPath(root);
    created.name_ = name.trimmed();
    created.task_ = task;
    created.classes_ = classes;
    for (QString& class_name : created.classes_) class_name = class_name.trimmed();
    if (!created.writeProject(error)) return false;
    *project = std::move(created);
    return true;
}

bool YoloDatasetProject::load(const QString& rootDirectory, QString* error)
{
    const QString root = QFileInfo(rootDirectory).absoluteFilePath();
    QFile file(QDir(root).filePath(QStringLiteral("dataset.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("无法打开数据集清单：%1").arg(file.errorString()));
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("数据集清单格式错误：%1").arg(parse_error.errorString()));
        return false;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("version")).toInt() != FormatVersion) {
        setError(error, QStringLiteral("不支持的数据集清单版本。"));
        return false;
    }

    YoloDatasetProject loaded;
    loaded.root_directory_ = QDir::cleanPath(root);
    loaded.name_ = object.value(QStringLiteral("name")).toString().trimmed();
    const QString task_key = object.value(QStringLiteral("task")).toString().trimmed().toLower();
    if (task_key != QStringLiteral("detect") && task_key != QStringLiteral("segment") &&
        task_key != QStringLiteral("classify")) {
        setError(error, QStringLiteral("数据集任务类型无效。"));
        return false;
    }
    loaded.task_ = yoloTaskFromKey(task_key);
    for (const QJsonValue& value : object.value(QStringLiteral("classes")).toArray()) {
        loaded.classes_.push_back(value.toString());
    }
    if (loaded.name_.isEmpty() || !validateClassList(loaded.classes_, error)) return false;

    for (const QJsonValue& sample_value : object.value(QStringLiteral("samples")).toArray()) {
        const QJsonObject sample_object = sample_value.toObject();
        YoloDatasetSample sample;
        sample.id = sample_object.value(QStringLiteral("id")).toString();
        sample.sourceName = sample_object.value(QStringLiteral("source_name")).toString();
        sample.split = normalizedSplit(sample_object.value(QStringLiteral("split")).toString());
        sample.imageRelativePath = sample_object.value(QStringLiteral("image")).toString();
        sample.labelRelativePath = sample_object.value(QStringLiteral("label")).toString();
        sample.imageSize = QSize(
            sample_object.value(QStringLiteral("width")).toInt(),
            sample_object.value(QStringLiteral("height")).toInt());
        for (const QJsonValue& annotation_value : sample_object.value(QStringLiteral("annotations")).toArray()) {
            const QJsonObject annotation_object = annotation_value.toObject();
            sample.annotations.push_back({
                annotation_object.value(QStringLiteral("class_id")).toInt(-1),
                pointsFromJson(annotation_object.value(QStringLiteral("points")).toArray()),
            });
        }
        loaded.samples_.push_back(std::move(sample));
    }
    if (!loaded.validate(error)) return false;
    *this = std::move(loaded);
    return true;
}

QString YoloDatasetProject::manifestPath() const
{
    return isOpen() ? QDir(root_directory_).filePath(QStringLiteral("dataset.json")) : QString();
}

QString YoloDatasetProject::trainingDataPath() const
{
    if (!isOpen()) return {};
    return task_ == YoloTask::Classification
        ? root_directory_
        : QDir(root_directory_).filePath(QStringLiteral("dataset.yaml"));
}

bool YoloDatasetProject::addClass(const QString& name, QString* error)
{
    const QString normalized = name.trimmed();
    if (!validateClassName(normalized, error)) return false;
    for (const QString& existing : classes_) {
        if (existing.compare(normalized, Qt::CaseInsensitive) == 0) {
            setError(error, QStringLiteral("类别名称已存在。"));
            return false;
        }
    }
    classes_.push_back(normalized);
    if (!writeProject(error)) {
        classes_.removeLast();
        return false;
    }
    return true;
}

bool YoloDatasetProject::renameClass(int index, const QString& name, QString* error)
{
    if (index < 0 || index >= classes_.size()) {
        setError(error, QStringLiteral("请选择有效类别。"));
        return false;
    }
    const QString normalized = name.trimmed();
    if (!validateClassName(normalized, error)) return false;
    for (int other = 0; other < classes_.size(); ++other) {
        if (other != index && classes_.at(other).compare(normalized, Qt::CaseInsensitive) == 0) {
            setError(error, QStringLiteral("类别名称已存在。"));
            return false;
        }
    }
    if (task_ == YoloTask::Classification) {
        for (const YoloDatasetSample& sample : samples_) {
            if (!sample.annotations.isEmpty() && sample.annotations.first().classId == index) {
                setError(error, QStringLiteral("分类类别已有样本，删除样本后才能重命名。"));
                return false;
            }
        }
        for (const QString& split : kSplits) {
            const QDir old_directory(QDir(root_directory_).filePath(
                QStringLiteral("%1/%2").arg(split, classes_.at(index))));
            if (old_directory.exists() && !old_directory.entryList(
                    QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                setError(error, QStringLiteral("分类类别目录中仍有未登记文件，无法安全重命名。"));
                return false;
            }
        }
    }
    const QString previous = classes_.at(index);
    classes_[index] = normalized;
    if (!writeProject(error)) {
        classes_[index] = previous;
        return false;
    }
    if (task_ == YoloTask::Classification && previous != normalized) {
        for (const QString& split : kSplits) {
            QDir(QDir(root_directory_).filePath(split)).rmdir(previous);
        }
    }
    return true;
}

bool YoloDatasetProject::removeClass(int index, QString* error)
{
    if (classes_.size() <= 1) {
        setError(error, QStringLiteral("数据集至少需要保留一个类别。"));
        return false;
    }
    if (index < 0 || index >= classes_.size()) {
        setError(error, QStringLiteral("请选择有效类别。"));
        return false;
    }
    for (const YoloDatasetSample& sample : samples_) {
        for (const YoloDatasetAnnotation& annotation : sample.annotations) {
            if (annotation.classId == index) {
                setError(error, QStringLiteral("该类别仍被样本使用，请先删除相关标注或样本。"));
                return false;
            }
        }
    }
    const QString removed_class = classes_.at(index);
    if (task_ == YoloTask::Classification) {
        for (const QString& split : kSplits) {
            const QDir class_directory(QDir(root_directory_).filePath(
                QStringLiteral("%1/%2").arg(split, removed_class)));
            if (class_directory.exists() && !class_directory.entryList(
                    QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                setError(error, QStringLiteral("分类类别目录中仍有未登记文件，无法安全删除。"));
                return false;
            }
        }
    }

    const QStringList previous_classes = classes_;
    const QVector<YoloDatasetSample> previous_samples = samples_;
    classes_.removeAt(index);
    for (YoloDatasetSample& sample : samples_) {
        for (YoloDatasetAnnotation& annotation : sample.annotations) {
            if (annotation.classId > index) --annotation.classId;
        }
    }
    if (!writeProject(error)) {
        classes_ = previous_classes;
        samples_ = previous_samples;
        return false;
    }
    if (task_ != YoloTask::Classification) {
        for (const YoloDatasetSample& sample : samples_) {
            if (!writeLabelFile(sample, error)) return false;
        }
    } else {
        for (const QString& split : kSplits) {
            QDir(QDir(root_directory_).filePath(split)).rmdir(removed_class);
        }
    }
    return true;
}

bool YoloDatasetProject::saveSample(
    const QImage& image,
    const QString& sourceName,
    const QString& split,
    const QVector<YoloDatasetAnnotation>& annotations,
    const QString& existingId,
    QString* savedId,
    QString* error)
{
    if (!isOpen() || image.isNull()) {
        setError(error, QStringLiteral("请先打开数据集和有效图像。"));
        return false;
    }
    const QString normalized_split = normalizedSplit(split);
    if (!isValidSplit(normalized_split)) {
        setError(error, QStringLiteral("无效的数据集划分。"));
        return false;
    }
    QVector<YoloDatasetAnnotation> normalized_annotations;
    if (!normalizeAnnotations(image.size(), annotations, &normalized_annotations, error)) return false;

    const int existing_index = existingId.isEmpty() ? -1 : sampleIndex(existingId);
    if (!existingId.isEmpty() && existing_index < 0) {
        setError(error, QStringLiteral("要更新的样本不存在。"));
        return false;
    }
    YoloDatasetSample sample;
    sample.id = existing_index >= 0 ? existingId
        : QStringLiteral("sample_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(12));
    sample.sourceName = sourceName;
    sample.split = normalized_split;
    sample.imageSize = image.size();
    sample.annotations = normalized_annotations;

    if (task_ == YoloTask::Classification) {
        sample.imageRelativePath = QStringLiteral("%1/%2/%3.png")
            .arg(sample.split, classDirectory(sample.annotations.first().classId), sample.id);
    } else {
        sample.imageRelativePath = QStringLiteral("images/%1/%2.png").arg(sample.split, sample.id);
        sample.labelRelativePath = QStringLiteral("labels/%1/%2.txt").arg(sample.split, sample.id);
    }
    if (!isSafeRelativePath(sample.imageRelativePath) ||
        (!sample.labelRelativePath.isEmpty() && !isSafeRelativePath(sample.labelRelativePath))) {
        setError(error, QStringLiteral("生成的样本路径不安全。"));
        return false;
    }

    const QString image_path = QDir(root_directory_).filePath(sample.imageRelativePath);
    QDir().mkpath(QFileInfo(image_path).absolutePath());
    QSaveFile image_file(image_path);
    if (!image_file.open(QIODevice::WriteOnly) || !image.save(&image_file, "PNG") || !image_file.commit()) {
        setError(error, QStringLiteral("保存样本图像失败：%1").arg(image_file.errorString()));
        return false;
    }
    if (task_ != YoloTask::Classification && !writeLabelFile(sample, error)) {
        if (existing_index < 0) QFile::remove(image_path);
        return false;
    }

    const QVector<YoloDatasetSample> previous_samples = samples_;
    if (existing_index >= 0) samples_[existing_index] = sample;
    else samples_.push_back(sample);
    if (!writeManifest(error)) {
        samples_ = previous_samples;
        return false;
    }

    if (existing_index >= 0) {
        const YoloDatasetSample& old = previous_samples.at(existing_index);
        if (old.imageRelativePath != sample.imageRelativePath && isSafeRelativePath(old.imageRelativePath)) {
            QFile::remove(QDir(root_directory_).filePath(old.imageRelativePath));
        }
        if (!old.labelRelativePath.isEmpty() && old.labelRelativePath != sample.labelRelativePath &&
            isSafeRelativePath(old.labelRelativePath)) {
            QFile::remove(QDir(root_directory_).filePath(old.labelRelativePath));
        }
    }
    if (savedId) *savedId = sample.id;
    return true;
}

bool YoloDatasetProject::loadSample(
    const QString& id,
    QImage* image,
    QVector<YoloDatasetAnnotation>* annotations,
    QString* error) const
{
    const int index = sampleIndex(id);
    if (index < 0) {
        setError(error, QStringLiteral("样本不存在。"));
        return false;
    }
    const YoloDatasetSample& sample = samples_.at(index);
    QImage loaded(absoluteImagePath(sample));
    if (loaded.isNull()) {
        setError(error, QStringLiteral("无法读取样本图像。"));
        return false;
    }
    if (image) *image = loaded;
    if (annotations) *annotations = sample.annotations;
    return true;
}

bool YoloDatasetProject::removeSample(const QString& id, QString* error)
{
    const int index = sampleIndex(id);
    if (index < 0) {
        setError(error, QStringLiteral("请选择有效样本。"));
        return false;
    }
    const YoloDatasetSample sample = samples_.at(index);
    samples_.removeAt(index);
    if (!writeManifest(error)) {
        samples_.insert(index, sample);
        return false;
    }
    if (isSafeRelativePath(sample.imageRelativePath)) {
        QFile::remove(QDir(root_directory_).filePath(sample.imageRelativePath));
    }
    if (!sample.labelRelativePath.isEmpty() && isSafeRelativePath(sample.labelRelativePath)) {
        QFile::remove(QDir(root_directory_).filePath(sample.labelRelativePath));
    }
    return true;
}

int YoloDatasetProject::sampleIndex(const QString& id) const
{
    for (int index = 0; index < samples_.size(); ++index) {
        if (samples_.at(index).id == id) return index;
    }
    return -1;
}

QString YoloDatasetProject::absoluteImagePath(const YoloDatasetSample& sample) const
{
    return isSafeRelativePath(sample.imageRelativePath)
        ? QDir(root_directory_).filePath(sample.imageRelativePath)
        : QString();
}

bool YoloDatasetProject::validate(QString* error) const
{
    if (!isOpen() || name_.trimmed().isEmpty() || !validateClassList(classes_, error)) {
        if (error && error->isEmpty()) *error = QStringLiteral("数据集项目不完整。");
        return false;
    }
    QSet<QString> ids;
    QSet<QString> image_paths;
    QSet<QString> label_paths;
    for (const YoloDatasetSample& sample : samples_) {
        if (sample.id.isEmpty() || ids.contains(sample.id) || !isValidSplit(sample.split) ||
            !sample.imageSize.isValid() || !isSafeRelativePath(sample.imageRelativePath) ||
            (task_ != YoloTask::Classification && !isSafeRelativePath(sample.labelRelativePath))) {
            setError(error, QStringLiteral("数据集包含无效或重复的样本记录。"));
            return false;
        }
        ids.insert(sample.id);
        if (image_paths.contains(sample.imageRelativePath) ||
            (!sample.labelRelativePath.isEmpty() && label_paths.contains(sample.labelRelativePath))) {
            setError(error, QStringLiteral("多个样本引用了同一个图像或标签文件。"));
            return false;
        }
        image_paths.insert(sample.imageRelativePath);
        if (!sample.labelRelativePath.isEmpty()) label_paths.insert(sample.labelRelativePath);
        if (!QFileInfo(absoluteImagePath(sample)).isFile()) {
            setError(error, QStringLiteral("样本图像缺失：%1").arg(sample.imageRelativePath));
            return false;
        }
        QVector<YoloDatasetAnnotation> normalized;
        if (!normalizeAnnotations(sample.imageSize, sample.annotations, &normalized, error)) return false;
        if (task_ != YoloTask::Classification &&
            !QFileInfo(QDir(root_directory_).filePath(sample.labelRelativePath)).isFile()) {
            setError(error, QStringLiteral("样本标签缺失：%1").arg(sample.labelRelativePath));
            return false;
        }
    }
    return true;
}

bool YoloDatasetProject::isValidSplit(const QString& split)
{
    return kSplits.contains(split);
}

QString YoloDatasetProject::normalizedSplit(const QString& split)
{
    return split.trimmed().toLower();
}

bool YoloDatasetProject::writeProject(QString* error) const
{
    return ensureLayout(error) && writeDatasetYaml(error) && writeManifest(error);
}

bool YoloDatasetProject::ensureLayout(QString* error) const
{
    if (!QDir().mkpath(root_directory_)) {
        setError(error, QStringLiteral("无法创建数据集目录。"));
        return false;
    }
    if (task_ == YoloTask::Classification) {
        for (const QString& split : kSplits) {
            for (int class_id = 0; class_id < classes_.size(); ++class_id) {
                if (!QDir().mkpath(QDir(root_directory_).filePath(
                        QStringLiteral("%1/%2").arg(split, classDirectory(class_id))))) {
                    setError(error, QStringLiteral("无法创建分类数据目录。"));
                    return false;
                }
            }
        }
    } else {
        for (const QString& split : kSplits) {
            if (!QDir().mkpath(QDir(root_directory_).filePath(QStringLiteral("images/%1").arg(split))) ||
                !QDir().mkpath(QDir(root_directory_).filePath(QStringLiteral("labels/%1").arg(split)))) {
                setError(error, QStringLiteral("无法创建检测/分割数据目录。"));
                return false;
            }
        }
    }
    return true;
}

bool YoloDatasetProject::writeManifest(QString* error) const
{
    QJsonArray classes;
    for (const QString& class_name : classes_) classes.append(class_name);
    QJsonArray samples;
    for (const YoloDatasetSample& sample : samples_) {
        QJsonArray annotations;
        for (const YoloDatasetAnnotation& annotation : sample.annotations) {
            annotations.append(QJsonObject{
                {QStringLiteral("class_id"), annotation.classId},
                {QStringLiteral("points"), pointsToJson(annotation.points)},
            });
        }
        samples.append(QJsonObject{
            {QStringLiteral("id"), sample.id},
            {QStringLiteral("source_name"), sample.sourceName},
            {QStringLiteral("split"), sample.split},
            {QStringLiteral("image"), sample.imageRelativePath},
            {QStringLiteral("label"), sample.labelRelativePath},
            {QStringLiteral("width"), sample.imageSize.width()},
            {QStringLiteral("height"), sample.imageSize.height()},
            {QStringLiteral("annotations"), annotations},
        });
    }
    const QJsonObject object{
        {QStringLiteral("version"), FormatVersion},
        {QStringLiteral("name"), name_},
        {QStringLiteral("task"), yoloTaskKey(task_)},
        {QStringLiteral("classes"), classes},
        {QStringLiteral("samples"), samples},
        {QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    };
    return writeBytes(manifestPath(), QJsonDocument(object).toJson(QJsonDocument::Indented), error);
}

bool YoloDatasetProject::writeDatasetYaml(QString* error) const
{
    if (task_ == YoloTask::Classification) return true;
    QString text = QStringLiteral("path: %1\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n")
        .arg(yamlQuote(QDir(root_directory_).absolutePath().replace(QLatin1Char('\\'), QLatin1Char('/'))));
    for (int index = 0; index < classes_.size(); ++index) {
        text += QStringLiteral("  %1: %2\n").arg(index).arg(yamlQuote(classes_.at(index)));
    }
    return writeBytes(QDir(root_directory_).filePath(QStringLiteral("dataset.yaml")), text.toUtf8(), error);
}

bool YoloDatasetProject::writeLabelFile(const YoloDatasetSample& sample, QString* error) const
{
    if (task_ == YoloTask::Classification) return true;
    if (!sample.imageSize.isValid() || !isSafeRelativePath(sample.labelRelativePath)) {
        setError(error, QStringLiteral("无法写入无效样本标签。"));
        return false;
    }
    QStringList lines;
    for (const YoloDatasetAnnotation& annotation : sample.annotations) {
        if (task_ == YoloTask::Detection) {
            const QRectF bounds(annotation.points.at(0), annotation.points.at(1));
            const QRectF normalized = bounds.normalized();
            lines.push_back(QStringLiteral("%1 %2 %3 %4 %5")
                .arg(annotation.classId)
                .arg(number(normalized.center().x() / sample.imageSize.width()))
                .arg(number(normalized.center().y() / sample.imageSize.height()))
                .arg(number(normalized.width() / sample.imageSize.width()))
                .arg(number(normalized.height() / sample.imageSize.height())));
        } else {
            QStringList values;
            values.push_back(QString::number(annotation.classId));
            for (const QPointF& point : annotation.points) {
                values.push_back(number(point.x() / sample.imageSize.width()));
                values.push_back(number(point.y() / sample.imageSize.height()));
            }
            lines.push_back(values.join(QLatin1Char(' ')));
        }
    }
    const QByteArray bytes = (lines.join(QLatin1Char('\n')) + (lines.isEmpty() ? QString() : QStringLiteral("\n"))).toUtf8();
    return writeBytes(QDir(root_directory_).filePath(sample.labelRelativePath), bytes, error);
}

bool YoloDatasetProject::normalizeAnnotations(
    const QSize& imageSize,
    const QVector<YoloDatasetAnnotation>& input,
    QVector<YoloDatasetAnnotation>* output,
    QString* error) const
{
    if (!output || !imageSize.isValid()) {
        setError(error, QStringLiteral("图像尺寸无效。"));
        return false;
    }
    if (input.isEmpty()) {
        if (task_ == YoloTask::Classification) {
            setError(error, QStringLiteral("分类样本必须选择一个类别。"));
            return false;
        }
        output->clear();
        return true;
    }
    if (task_ == YoloTask::Classification && input.size() != 1) {
        setError(error, QStringLiteral("分类样本必须且只能选择一个类别。"));
        return false;
    }
    const QRectF image_bounds(QPointF(0.0, 0.0), QSizeF(imageSize));
    QVector<YoloDatasetAnnotation> normalized;
    for (const YoloDatasetAnnotation& annotation : input) {
        if (annotation.classId < 0 || annotation.classId >= classes_.size()) {
            setError(error, QStringLiteral("标注包含无效类别。"));
            return false;
        }
        YoloDatasetAnnotation value;
        value.classId = annotation.classId;
        if (task_ == YoloTask::Classification) {
            normalized.push_back(value);
            continue;
        }
        if (task_ == YoloTask::Detection) {
            if (annotation.points.size() != 2) {
                setError(error, QStringLiteral("检测标注必须包含矩形的两个对角点。"));
                return false;
            }
            const QRectF bounds = QRectF(annotation.points.at(0), annotation.points.at(1))
                .normalized().intersected(image_bounds);
            if (bounds.width() < 1.0 || bounds.height() < 1.0) {
                setError(error, QStringLiteral("检测框太小或位于图像外。"));
                return false;
            }
            value.points = {bounds.topLeft(), bounds.bottomRight()};
        } else {
            if (annotation.points.size() < 3) {
                setError(error, QStringLiteral("分割标注至少需要三个顶点。"));
                return false;
            }
            for (const QPointF& point : annotation.points) {
                value.points.push_back(QPointF(
                    std::clamp(point.x(), 0.0, static_cast<double>(imageSize.width())),
                    std::clamp(point.y(), 0.0, static_cast<double>(imageSize.height()))));
            }
            const QRectF bounds = QPolygonF(value.points).boundingRect();
            if (bounds.width() < 1.0 || bounds.height() < 1.0) {
                setError(error, QStringLiteral("分割多边形面积太小。"));
                return false;
            }
        }
        normalized.push_back(std::move(value));
    }
    *output = std::move(normalized);
    return true;
}

QString YoloDatasetProject::classDirectory(int classId) const
{
    return classId >= 0 && classId < classes_.size() ? classes_.at(classId) : QStringLiteral("unknown");
}

bool YoloDatasetProject::validateClassList(const QStringList& classes, QString* error)
{
    if (classes.isEmpty()) {
        setError(error, QStringLiteral("数据集至少需要一个类别。"));
        return false;
    }
    QStringList seen;
    for (const QString& class_name : classes) {
        const QString normalized = class_name.trimmed();
        if (!validateClassName(normalized, error)) return false;
        for (const QString& existing : seen) {
            if (existing.compare(normalized, Qt::CaseInsensitive) == 0) {
                setError(error, QStringLiteral("数据集类别名称不能重复。"));
                return false;
            }
        }
        seen.push_back(normalized);
    }
    return true;
}

bool YoloDatasetProject::validateClassName(const QString& name, QString* error)
{
    if (name.trimmed().isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") ||
        name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        setError(error, QStringLiteral("类别名称不能为空或以点/空格结尾。"));
        return false;
    }
    static const QString forbidden = QStringLiteral("<>:\"/\\|?*");
    for (const QChar character : name) {
        if (character.unicode() < 32 || forbidden.contains(character)) {
            setError(error, QStringLiteral("类别名称包含文件系统不支持的字符。"));
            return false;
        }
    }
    return true;
}

bool YoloDatasetProject::isSafeRelativePath(const QString& path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path)) return false;
    const QString clean = QDir::cleanPath(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
    return clean != QStringLiteral(".") && clean != QStringLiteral("..") &&
        !clean.startsWith(QStringLiteral("../"));
}

void YoloDatasetProject::setError(QString* error, const QString& message)
{
    if (error) *error = message;
}
