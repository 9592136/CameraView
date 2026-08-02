#pragma once

#include "YoloModelRegistry.h"

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

struct YoloInferenceOptions {
    double confidence = 0.25;
    double iou = 0.45;
    int imageSize = 640;
    int maxDetections = 300;
    QString device = QStringLiteral("cpu");
};

struct YoloTrainingRequest {
    QString modelPath;
    QString datasetPath;
    YoloTask task = YoloTask::Detection;
    int epochs = 50;
    int imageSize = 640;
    int batchSize = 8;
    int workers = 4;
    int patience = 20;
    QString device = QStringLiteral("cpu");
    QString projectDirectory;
    QString runName = QStringLiteral("train");
};

class YoloProcessController final : public QObject {
    Q_OBJECT

public:
    explicit YoloProcessController(QObject* parent = nullptr);

    void setPythonExecutable(QString path) { python_executable_ = std::move(path); }
    QString pythonExecutable() const { return python_executable_; }
    void setBackendScript(QString path) { backend_script_ = std::move(path); }
    QString backendScript() const { return backend_script_; }
    bool isBusy() const { return process_.state() != QProcess::NotRunning; }

    bool probeRuntime();
    bool inspectModel(const QString& modelId, const QString& modelPath, YoloTask task);
    bool runInference(
        const QString& modelId,
        const QString& modelPath,
        YoloTask task,
        const QImage& image,
        const YoloInferenceOptions& options);
    bool startTraining(const YoloTrainingRequest& request);
    bool exportModel(
        const QString& modelId,
        const QString& modelPath,
        YoloTask task,
        const QString& format,
        int imageSize,
        const QString& device);
    void cancel();

signals:
    void busyChanged(bool busy, QString operation);
    void runtimeProbeFinished(QJsonObject result);
    void modelInspected(QString modelId, QJsonObject result);
    void inferenceFinished(QString modelId, QJsonObject result);
    void trainingProgress(QJsonObject progress);
    void trainingFinished(QJsonObject result);
    void exportFinished(QString modelId, QJsonObject result);
    void logLine(QString line);
    void operationError(QString operation, QString message, QString detail);

private slots:
    void readStandardOutput();
    void readStandardError();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class Operation {
        None,
        Probe,
        Inspect,
        Inference,
        Training,
        Export
    };

    bool start(Operation operation, QStringList arguments, QString contextId = {});
    void consumeBuffer(QByteArray& buffer, bool parseJson);
    void consumeLine(const QString& line, bool parseJson);
    QString operationName() const;
    static QString number(double value);

    QProcess process_;
    QString python_executable_;
    QString backend_script_;
    Operation operation_ = Operation::None;
    QString context_id_;
    QString temporary_image_path_;
    QByteArray stdout_buffer_;
    QByteArray stderr_buffer_;
    bool terminal_event_received_ = false;
    bool cancel_requested_ = false;
};
