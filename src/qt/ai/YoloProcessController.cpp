#include "YoloProcessController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

YoloProcessController::YoloProcessController(QObject* parent) : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput, this, &YoloProcessController::readStandardOutput);
    connect(&process_, &QProcess::readyReadStandardError, this, &YoloProcessController::readStandardError);
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this, &YoloProcessController::processFinished);

    backend_script_ = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("yolo/yolo_backend.py"));
}

bool YoloProcessController::probeRuntime()
{
    return start(Operation::Probe, {QStringLiteral("probe")});
}

bool YoloProcessController::inspectModel(const QString& modelId, const QString& modelPath, YoloTask task)
{
    return start(Operation::Inspect, {
        QStringLiteral("inspect"),
        QStringLiteral("--model"), modelPath,
        QStringLiteral("--task"), yoloTaskKey(task),
    }, modelId);
}

bool YoloProcessController::runInference(
    const QString& modelId,
    const QString& modelPath,
    YoloTask task,
    const QImage& image,
    const YoloInferenceOptions& options)
{
    if (image.isNull() || isBusy()) return false;
    const QString temporary_directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    temporary_image_path_ = QDir(temporary_directory).filePath(
        QStringLiteral("CameraView-YOLO-%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!image.save(temporary_image_path_, "PNG")) {
        temporary_image_path_.clear();
        emit operationError(QStringLiteral("inference"), QStringLiteral("无法创建推理临时图像。"), {});
        return false;
    }
    const bool started = start(Operation::Inference, {
        QStringLiteral("infer"),
        QStringLiteral("--model"), modelPath,
        QStringLiteral("--task"), yoloTaskKey(task),
        QStringLiteral("--source"), temporary_image_path_,
        QStringLiteral("--conf"), number(options.confidence),
        QStringLiteral("--iou"), number(options.iou),
        QStringLiteral("--imgsz"), QString::number(options.imageSize),
        QStringLiteral("--max-det"), QString::number(options.maxDetections),
        QStringLiteral("--device"), options.device,
    }, modelId);
    if (!started) {
        QFile::remove(temporary_image_path_);
        temporary_image_path_.clear();
    }
    return started;
}

bool YoloProcessController::startTraining(const YoloTrainingRequest& request)
{
    return start(Operation::Training, {
        QStringLiteral("train"),
        QStringLiteral("--model"), request.modelPath,
        QStringLiteral("--data"), request.datasetPath,
        QStringLiteral("--task"), yoloTaskKey(request.task),
        QStringLiteral("--epochs"), QString::number(request.epochs),
        QStringLiteral("--imgsz"), QString::number(request.imageSize),
        QStringLiteral("--batch"), QString::number(request.batchSize),
        QStringLiteral("--workers"), QString::number(request.workers),
        QStringLiteral("--patience"), QString::number(request.patience),
        QStringLiteral("--device"), request.device,
        QStringLiteral("--project"), request.projectDirectory,
        QStringLiteral("--name"), request.runName,
        QStringLiteral("--exist-ok"),
    });
}

bool YoloProcessController::exportModel(
    const QString& modelId,
    const QString& modelPath,
    YoloTask task,
    const QString& format,
    int imageSize,
    const QString& device)
{
    return start(Operation::Export, {
        QStringLiteral("export"),
        QStringLiteral("--model"), modelPath,
        QStringLiteral("--task"), yoloTaskKey(task),
        QStringLiteral("--format"), format,
        QStringLiteral("--imgsz"), QString::number(imageSize),
        QStringLiteral("--device"), device,
    }, modelId);
}

void YoloProcessController::cancel()
{
    if (!isBusy()) return;
    cancel_requested_ = true;
    emit logLine(QStringLiteral("正在停止 YOLO 任务…"));
    process_.terminate();
    if (!process_.waitForFinished(3000)) process_.kill();
}

bool YoloProcessController::start(Operation operation, QStringList arguments, QString contextId)
{
    if (isBusy()) {
        emit operationError(operationName(), QStringLiteral("另一个 YOLO 任务正在运行。"), {});
        return false;
    }
    if (python_executable_.isEmpty() || !QFileInfo::exists(python_executable_)) {
        emit operationError(QStringLiteral("runtime"), QStringLiteral("尚未配置有效的 Python 解释器。"), python_executable_);
        return false;
    }
    if (!QFileInfo::exists(backend_script_)) {
        emit operationError(QStringLiteral("runtime"), QStringLiteral("找不到 YOLO 后端脚本。"), backend_script_);
        return false;
    }
    operation_ = operation;
    context_id_ = std::move(contextId);
    stdout_buffer_.clear();
    stderr_buffer_.clear();
    terminal_event_received_ = false;
    cancel_requested_ = false;
    arguments.prepend(backend_script_);
    process_.setProgram(python_executable_);
    process_.setArguments(arguments);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.start();
    if (!process_.waitForStarted(3000)) {
        const QString detail = process_.errorString();
        operation_ = Operation::None;
        emit operationError(QStringLiteral("runtime"), QStringLiteral("无法启动 YOLO 后端。"), detail);
        return false;
    }
    emit busyChanged(true, operationName());
    return true;
}

void YoloProcessController::readStandardOutput()
{
    stdout_buffer_.append(process_.readAllStandardOutput());
    consumeBuffer(stdout_buffer_, true);
}

void YoloProcessController::readStandardError()
{
    stderr_buffer_.append(process_.readAllStandardError());
    consumeBuffer(stderr_buffer_, false);
}

void YoloProcessController::consumeBuffer(QByteArray& buffer, bool parseJson)
{
    int newline = -1;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (!line.isEmpty() && line.endsWith('\r')) line.chop(1);
        consumeLine(QString::fromUtf8(line), parseJson);
    }
}

void YoloProcessController::consumeLine(const QString& line, bool parseJson)
{
    QString clean_line = line;
    static const QRegularExpression ansi_escape(
        QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));
    clean_line.remove(ansi_escape);
    if (clean_line.trimmed().isEmpty()) return;
    if (!parseJson) {
        emit logLine(clean_line);
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(clean_line.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        emit logLine(clean_line);
        return;
    }
    const QJsonObject object = document.object();
    const QString event = object.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("probe")) {
        terminal_event_received_ = true;
        emit runtimeProbeFinished(object);
    } else if (event == QStringLiteral("model")) {
        terminal_event_received_ = true;
        emit modelInspected(context_id_, object);
    } else if (event == QStringLiteral("result")) {
        terminal_event_received_ = true;
        emit inferenceFinished(context_id_, object);
    } else if (event == QStringLiteral("train_progress")) {
        emit trainingProgress(object);
    } else if (event == QStringLiteral("train_complete")) {
        terminal_event_received_ = true;
        emit trainingFinished(object);
    } else if (event == QStringLiteral("export_complete")) {
        terminal_event_received_ = true;
        emit exportFinished(context_id_, object);
    } else if (event == QStringLiteral("error")) {
        terminal_event_received_ = true;
        emit operationError(operationName(),
            object.value(QStringLiteral("message")).toString(),
            object.value(QStringLiteral("detail")).toString());
    } else {
        emit logLine(clean_line);
    }
}

void YoloProcessController::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!stdout_buffer_.isEmpty()) {
        consumeLine(QString::fromUtf8(stdout_buffer_), true);
        stdout_buffer_.clear();
    }
    if (!stderr_buffer_.isEmpty()) {
        consumeLine(QString::fromUtf8(stderr_buffer_), false);
        stderr_buffer_.clear();
    }
    if (cancel_requested_) {
        emit logLine(QStringLiteral("YOLO 任务已停止。"));
    } else if (!terminal_event_received_ && (exitStatus != QProcess::NormalExit || exitCode != 0)) {
        emit operationError(operationName(), QStringLiteral("YOLO 后端进程异常结束。"),
            QStringLiteral("exit code: %1").arg(exitCode));
    }
    if (!temporary_image_path_.isEmpty()) {
        QFile::remove(temporary_image_path_);
        temporary_image_path_.clear();
    }
    const QString completed_operation = operationName();
    operation_ = Operation::None;
    context_id_.clear();
    cancel_requested_ = false;
    emit busyChanged(false, completed_operation);
}

QString YoloProcessController::operationName() const
{
    switch (operation_) {
    case Operation::Probe: return QStringLiteral("runtime probe");
    case Operation::Inspect: return QStringLiteral("model inspection");
    case Operation::Inference: return QStringLiteral("inference");
    case Operation::Training: return QStringLiteral("training");
    case Operation::Export: return QStringLiteral("export");
    case Operation::None:
    default: return QStringLiteral("YOLO");
    }
}

QString YoloProcessController::number(double value)
{
    return QString::number(value, 'f', 6);
}
