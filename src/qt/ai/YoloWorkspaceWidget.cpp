#include "YoloWorkspaceWidget.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>

#include <algorithm>

namespace {

QWidget* pathRow(QLineEdit* edit, QPushButton* button)
{
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    layout->addWidget(button);
    return widget;
}

QString existingExecutable(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isFile()) return QFileInfo(candidate).absoluteFilePath();
    }
    return {};
}

} // namespace

YoloWorkspaceWidget::YoloWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
    QString error;
    if (!registry_.load(&error)) {
        emit statusMessage(tr("模型库读取失败：%1").arg(error));
    }
    buildUi();
    connectController();
    discoverPython();
    refreshModels();
}

void YoloWorkspaceWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;
    root->addWidget(tabs);

    auto* inference_page = new QWidget;
    auto* inference_layout = new QVBoxLayout(inference_page);

    auto* runtime_group = new QGroupBox(tr("运行环境"));
    auto* runtime_layout = new QVBoxLayout(runtime_group);
    python_edit_ = new QLineEdit;
    python_edit_->setPlaceholderText(tr("YOLO 虚拟环境中的 python.exe"));
    auto* choose_python = new QPushButton(tr("选择"));
    runtime_layout->addWidget(pathRow(python_edit_, choose_python));
    auto* probe = new QPushButton(tr("检测环境"));
    runtime_status_ = new QLabel(tr("尚未检测"));
    runtime_status_->setWordWrap(true);
    runtime_layout->addWidget(probe);
    runtime_layout->addWidget(runtime_status_);
    inference_layout->addWidget(runtime_group);

    auto* model_group = new QGroupBox(tr("模型管理"));
    auto* model_layout = new QVBoxLayout(model_group);
    auto* model_form = new QFormLayout;
    model_combo_ = new QComboBox;
    task_combo_ = new QComboBox;
    task_combo_->addItem(tr("目标检测"), yoloTaskKey(YoloTask::Detection));
    task_combo_->addItem(tr("图像分类"), yoloTaskKey(YoloTask::Classification));
    task_combo_->addItem(tr("实例分割"), yoloTaskKey(YoloTask::Segmentation));
    model_form->addRow(tr("模型"), model_combo_);
    model_form->addRow(tr("导入类型"), task_combo_);
    model_layout->addLayout(model_form);
    auto* model_buttons = new QHBoxLayout;
    auto* import_button = new QPushButton(tr("导入"));
    auto* activate_button = new QPushButton(tr("设为当前"));
    auto* inspect_button = new QPushButton(tr("读取信息"));
    auto* export_button = new QPushButton(tr("导出 ONNX"));
    auto* remove_button = new QPushButton(tr("删除"));
    model_buttons->addWidget(import_button);
    model_buttons->addWidget(activate_button);
    model_buttons->addWidget(inspect_button);
    model_buttons->addWidget(export_button);
    model_buttons->addWidget(remove_button);
    model_layout->addLayout(model_buttons);
    model_info_ = new QLabel;
    model_info_->setWordWrap(true);
    model_layout->addWidget(model_info_);
    auto* open_library = new QPushButton(tr("打开模型库目录"));
    model_layout->addWidget(open_library);
    inference_layout->addWidget(model_group);

    auto* options_group = new QGroupBox(tr("推理"));
    auto* options_layout = new QFormLayout(options_group);
    confidence_spin_ = new QDoubleSpinBox;
    confidence_spin_->setRange(0.01, 1.0);
    confidence_spin_->setSingleStep(0.05);
    confidence_spin_->setValue(0.25);
    iou_spin_ = new QDoubleSpinBox;
    iou_spin_->setRange(0.01, 1.0);
    iou_spin_->setSingleStep(0.05);
    iou_spin_->setValue(0.45);
    image_size_spin_ = new QSpinBox;
    image_size_spin_->setRange(160, 2048);
    image_size_spin_->setSingleStep(32);
    image_size_spin_->setValue(640);
    max_detections_spin_ = new QSpinBox;
    max_detections_spin_->setRange(1, 3000);
    max_detections_spin_->setValue(300);
    device_edit_ = new QLineEdit(QStringLiteral("cpu"));
    options_layout->addRow(tr("置信度"), confidence_spin_);
    options_layout->addRow(tr("IOU"), iou_spin_);
    options_layout->addRow(tr("输入尺寸"), image_size_spin_);
    options_layout->addRow(tr("最大结果数"), max_detections_spin_);
    options_layout->addRow(tr("设备"), device_edit_);
    infer_button_ = new QPushButton(tr("识别当前图像"));
    options_layout->addRow(infer_button_);
    result_list_ = new QListWidget;
    result_list_->setMinimumHeight(120);
    options_layout->addRow(tr("结果"), result_list_);
    inference_layout->addWidget(options_group);
    inference_layout->addStretch();
    tabs->addTab(inference_page, tr("推理与模型"));

    auto* training_page = new QWidget;
    auto* training_layout = new QVBoxLayout(training_page);
    auto* train_form = new QFormLayout;
    training_model_edit_ = new QLineEdit;
    auto* choose_training_model = new QPushButton(tr("选择"));
    dataset_edit_ = new QLineEdit;
    auto* choose_dataset = new QPushButton(tr("选择"));
    output_edit_ = new QLineEdit(QDir(registry_.rootDirectory()).filePath(QStringLiteral("runs")));
    auto* choose_output = new QPushButton(tr("选择"));
    run_name_edit_ = new QLineEdit(QStringLiteral("train"));
    epochs_spin_ = new QSpinBox;
    epochs_spin_->setRange(1, 10000);
    epochs_spin_->setValue(50);
    batch_spin_ = new QSpinBox;
    batch_spin_->setRange(-1, 1024);
    batch_spin_->setValue(8);
    workers_spin_ = new QSpinBox;
    workers_spin_->setRange(0, 64);
    workers_spin_->setValue(0);
    patience_spin_ = new QSpinBox;
    patience_spin_->setRange(0, 1000);
    patience_spin_->setValue(20);
    train_form->addRow(tr("基础模型"), pathRow(training_model_edit_, choose_training_model));
    train_form->addRow(tr("数据集"), pathRow(dataset_edit_, choose_dataset));
    train_form->addRow(tr("输出目录"), pathRow(output_edit_, choose_output));
    train_form->addRow(tr("任务名称"), run_name_edit_);
    train_form->addRow(tr("训练轮数"), epochs_spin_);
    train_form->addRow(tr("批大小（-1 自动）"), batch_spin_);
    train_form->addRow(tr("数据线程"), workers_spin_);
    train_form->addRow(tr("早停轮数"), patience_spin_);
    training_layout->addLayout(train_form);
    auto* train_buttons = new QHBoxLayout;
    train_button_ = new QPushButton(tr("开始训练"));
    cancel_button_ = new QPushButton(tr("停止"));
    cancel_button_->setEnabled(false);
    train_buttons->addWidget(train_button_);
    train_buttons->addWidget(cancel_button_);
    training_layout->addLayout(train_buttons);
    progress_ = new QProgressBar;
    progress_->setRange(0, 100);
    training_layout->addWidget(progress_);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    training_layout->addWidget(log_, 1);
    tabs->addTab(training_page, tr("训练"));

    connect(choose_python, &QPushButton::clicked, this, &YoloWorkspaceWidget::choosePython);
    connect(probe, &QPushButton::clicked, this, [this] {
        controller_.setPythonExecutable(python_edit_->text().trimmed());
        QSettings().setValue(QStringLiteral("yolo/python"), python_edit_->text().trimmed());
        controller_.probeRuntime();
    });
    connect(import_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::importModel);
    connect(remove_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::removeModel);
    connect(inspect_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::inspectModel);
    connect(activate_button, &QPushButton::clicked, this, [this] {
        const YoloModelRecord* model = selectedModel();
        QString error;
        if (model && registry_.setActive(model->id, &error)) refreshModels(model->id);
        else if (!error.isEmpty()) QMessageBox::warning(this, tr("模型管理"), error);
    });
    connect(export_button, &QPushButton::clicked, this, [this] {
        const YoloModelRecord* model = selectedModel();
        if (model) controller_.exportModel(model->id, model->filePath, model->task,
            QStringLiteral("onnx"), image_size_spin_->value(), device_edit_->text().trimmed());
    });
    connect(open_library, &QPushButton::clicked, this, [this] {
        QDir().mkpath(registry_.rootDirectory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(registry_.rootDirectory()));
    });
    connect(model_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        const YoloModelRecord* model = selectedModel();
        if (!model) {
            model_info_->setText(tr("尚未导入模型"));
            return;
        }
        model_info_->setText(tr("%1 · %2 · %3 类\n%4")
            .arg(yoloTaskDisplayName(model->task), model->format.toUpper())
            .arg(model->classNames.size()).arg(model->filePath));
        training_model_edit_->setText(model->filePath);
        task_combo_->setCurrentIndex(static_cast<int>(model->task));
    });
    connect(infer_button_, &QPushButton::clicked, this, &YoloWorkspaceWidget::runInference);
    connect(choose_dataset, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseDataset);
    connect(choose_training_model, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseTrainingModel);
    connect(choose_output, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseTrainingOutput);
    connect(train_button_, &QPushButton::clicked, this, &YoloWorkspaceWidget::startTraining);
    connect(cancel_button_, &QPushButton::clicked, &controller_, &YoloProcessController::cancel);
}

void YoloWorkspaceWidget::connectController()
{
    connect(&controller_, &YoloProcessController::busyChanged, this, &YoloWorkspaceWidget::setControlsBusy);
    connect(&controller_, &YoloProcessController::runtimeProbeFinished, this, [this](const QJsonObject& value) {
        const bool ready = value.value(QStringLiteral("ultralytics")).toBool();
        runtime_status_->setText(ready
            ? tr("可用：Python %1，Ultralytics %2，CUDA %3").arg(
                value.value(QStringLiteral("python")).toString(),
                value.value(QStringLiteral("ultralytics_version")).toString(),
                value.value(QStringLiteral("cuda")).toBool() ? tr("可用") : tr("不可用"))
            : tr("Python 可用，但未安装 Ultralytics。请运行 tools/setup_yolo.ps1。"));
    });
    connect(&controller_, &YoloProcessController::modelInspected, this,
        [this](const QString& id, const QJsonObject& value) {
            QStringList classes;
            const QJsonObject names = value.value(QStringLiteral("names")).toObject();
            QList<int> keys;
            for (auto it = names.begin(); it != names.end(); ++it) keys.push_back(it.key().toInt());
            std::sort(keys.begin(), keys.end());
            for (int key : keys) classes.push_back(names.value(QString::number(key)).toString());
            QString error;
            registry_.updateMetadata(id, yoloTaskFromKey(value.value(QStringLiteral("task")).toString()), classes, {}, &error);
            refreshModels(id);
            emit statusMessage(tr("模型信息已更新"));
        });
    connect(&controller_, &YoloProcessController::inferenceFinished, this,
        [this](const QString&, const QJsonObject& value) { renderInference(value); });
    connect(&controller_, &YoloProcessController::trainingProgress, this, [this](const QJsonObject& value) {
        progress_->setValue(qRound(value.value(QStringLiteral("progress")).toDouble() * 100.0));
        QString line = tr("第 %1/%2 轮").arg(value.value(QStringLiteral("epoch")).toInt())
            .arg(value.value(QStringLiteral("epochs")).toInt());
        if (value.value(QStringLiteral("loss")).isDouble()) {
            line += tr("，损失 %1").arg(value.value(QStringLiteral("loss")).toDouble(), 0, 'g', 6);
        }
        log_->appendPlainText(line);
    });
    connect(&controller_, &YoloProcessController::trainingFinished, this, [this](const QJsonObject& value) {
        progress_->setValue(100);
        const QString best = value.value(QStringLiteral("best")).toString();
        log_->appendPlainText(tr("训练完成：%1").arg(best));
        if (!best.isEmpty()) {
            YoloModelRecord imported;
            QString error;
            const QString name = run_name_edit_->text().trimmed() + QStringLiteral("-best");
            if (registry_.registerTrainingArtifact(best,
                    yoloTaskFromKey(value.value(QStringLiteral("task")).toString()), name,
                    value.value(QStringLiteral("metrics")).toObject(), &imported, &error)) {
                refreshModels(imported.id);
                emit statusMessage(tr("训练完成，最佳模型已加入模型库"));
            } else {
                log_->appendPlainText(error);
            }
        }
    });
    connect(&controller_, &YoloProcessController::exportFinished, this,
        [this](const QString& sourceId, const QJsonObject& value) {
            const QString output = value.value(QStringLiteral("output")).toString();
            const YoloModelRecord* source = registry_.find(sourceId);
            if (source && QFileInfo(output).isFile()) {
                YoloModelRecord imported;
                QString error;
                if (registry_.importModel(output, source->task, true, &imported, &error)) {
                    refreshModels(imported.id);
                    emit statusMessage(tr("模型已导出并加入模型库：%1").arg(output));
                    return;
                }
                log_->appendPlainText(error);
            }
            emit statusMessage(tr("模型已导出：%1").arg(output));
        });
    connect(&controller_, &YoloProcessController::logLine, log_, &QPlainTextEdit::appendPlainText);
    connect(&controller_, &YoloProcessController::operationError, this,
        [this](const QString& operation, const QString& message, const QString& detail) {
            const QString text = detail.isEmpty() ? message : message + QStringLiteral("\n") + detail;
            log_->appendPlainText(QStringLiteral("[%1] %2").arg(operation, text));
            emit statusMessage(text);
        });
}

void YoloWorkspaceWidget::discoverPython()
{
    const QString app = QCoreApplication::applicationDirPath();
    const QString configured = QSettings().value(QStringLiteral("yolo/python")).toString();
    const QString env = qEnvironmentVariable("CAMERAVIEW_YOLO_PYTHON");
    const QString found = existingExecutable({configured, env,
        QDir(app).filePath(QStringLiteral("../.venv-yolo/Scripts/python.exe")),
        QDir(app).filePath(QStringLiteral(".venv-yolo/Scripts/python.exe")),
        QStandardPaths::findExecutable(QStringLiteral("python.exe")),
        QStandardPaths::findExecutable(QStringLiteral("python"))});
    python_edit_->setText(found);
    controller_.setPythonExecutable(found);
}

void YoloWorkspaceWidget::setCurrentImage(const QImage& image, const QString& sourceName)
{
    if (sourceName != current_source_ || image.size() != current_image_.size() || image.isNull()) {
        if (result_list_) result_list_->clear();
        emit overlaysChanged({});
    }
    current_image_ = image;
    current_source_ = sourceName;
    infer_button_->setEnabled(!image.isNull() && !controller_.isBusy());
}

void YoloWorkspaceWidget::refreshModels(const QString& selectId)
{
    const QString previous = selectId.isEmpty() ? model_combo_->currentData().toString() : selectId;
    model_combo_->blockSignals(true);
    model_combo_->clear();
    int selected = -1;
    int active = -1;
    for (const YoloModelRecord& model : registry_.models()) {
        const QString label = QStringLiteral("%1%2 [%3]").arg(model.active ? QStringLiteral("★ ") : QString(),
            model.name, yoloTaskDisplayName(model.task));
        model_combo_->addItem(label, model.id);
        if (model.id == previous) selected = model_combo_->count() - 1;
        if (model.active) active = model_combo_->count() - 1;
    }
    model_combo_->setCurrentIndex(selected >= 0 ? selected : active >= 0 ? active : 0);
    model_combo_->blockSignals(false);
    model_combo_->currentIndexChanged(model_combo_->currentIndex());
}

const YoloModelRecord* YoloWorkspaceWidget::selectedModel() const
{
    return registry_.find(model_combo_->currentData().toString());
}

YoloTask YoloWorkspaceWidget::selectedTask() const
{
    return yoloTaskFromKey(task_combo_->currentData().toString());
}

void YoloWorkspaceWidget::choosePython()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("选择 Python"), {}, tr("Python (python.exe);;所有文件 (*)"));
    if (!path.isEmpty()) {
        python_edit_->setText(path);
        controller_.setPythonExecutable(path);
        QSettings().setValue(QStringLiteral("yolo/python"), path);
    }
}

void YoloWorkspaceWidget::importModel()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("导入 YOLO 模型"), {}, tr("YOLO 模型 (*.pt *.onnx)"));
    if (path.isEmpty()) return;
    YoloTask task = selectedTask();
    const YoloTask inferred = YoloModelRegistry::inferTaskFromFileName(path);
    if (inferred != YoloTask::Detection || task == YoloTask::Detection) task = inferred;
    YoloModelRecord imported;
    QString error;
    if (!registry_.importModel(path, task, true, &imported, &error)) {
        QMessageBox::warning(this, tr("导入失败"), error);
        return;
    }
    refreshModels(imported.id);
    emit statusMessage(tr("模型已加入模型库"));
    controller_.inspectModel(imported.id, imported.filePath, imported.task);
}

void YoloWorkspaceWidget::removeModel()
{
    const YoloModelRecord* model = selectedModel();
    if (!model) return;
    if (QMessageBox::question(this, tr("删除模型"), tr("从模型库删除“%1”及其托管文件？").arg(model->name)) != QMessageBox::Yes) return;
    QString error;
    if (!registry_.remove(model->id, true, &error)) QMessageBox::warning(this, tr("删除失败"), error);
    refreshModels();
}

void YoloWorkspaceWidget::inspectModel()
{
    const YoloModelRecord* model = selectedModel();
    if (model) controller_.inspectModel(model->id, model->filePath, model->task);
}

void YoloWorkspaceWidget::runInference()
{
    const YoloModelRecord* model = selectedModel();
    if (!model || current_image_.isNull()) {
        emit statusMessage(tr("请先导入模型并打开图像"));
        return;
    }
    registry_.setActive(model->id);
    result_list_->clear();
    YoloInferenceOptions options;
    options.confidence = confidence_spin_->value();
    options.iou = iou_spin_->value();
    options.imageSize = image_size_spin_->value();
    options.maxDetections = max_detections_spin_->value();
    options.device = device_edit_->text().trimmed();
    controller_.runInference(model->id, model->filePath, model->task, current_image_, options);
}

void YoloWorkspaceWidget::chooseDataset()
{
    QString path;
    if (selectedTask() == YoloTask::Classification) {
        path = QFileDialog::getExistingDirectory(this, tr("选择分类数据集目录"));
    } else {
        path = QFileDialog::getOpenFileName(this, tr("选择数据集配置"), {}, tr("YOLO 数据集 (*.yaml *.yml)"));
    }
    if (!path.isEmpty()) dataset_edit_->setText(path);
}

void YoloWorkspaceWidget::chooseTrainingModel()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("选择基础模型"), {}, tr("YOLO PyTorch 模型 (*.pt)"));
    if (!path.isEmpty()) training_model_edit_->setText(path);
}

void YoloWorkspaceWidget::chooseTrainingOutput()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("选择训练输出目录"), output_edit_->text());
    if (!path.isEmpty()) output_edit_->setText(path);
}

void YoloWorkspaceWidget::startTraining()
{
    if (training_model_edit_->text().trimmed().isEmpty() || dataset_edit_->text().trimmed().isEmpty()) {
        emit statusMessage(tr("请设置基础模型与数据集"));
        return;
    }
    if (QFileInfo(training_model_edit_->text().trimmed()).suffix().compare(QStringLiteral("pt"), Qt::CaseInsensitive) != 0) {
        emit statusMessage(tr("训练仅支持 .pt 基础模型；ONNX 模型可用于推理。"));
        return;
    }
    QDir().mkpath(output_edit_->text().trimmed());
    YoloTrainingRequest request;
    request.modelPath = training_model_edit_->text().trimmed();
    request.datasetPath = dataset_edit_->text().trimmed();
    request.task = selectedTask();
    request.epochs = epochs_spin_->value();
    request.imageSize = image_size_spin_->value();
    request.batchSize = batch_spin_->value();
    request.workers = workers_spin_->value();
    request.patience = patience_spin_->value();
    request.device = device_edit_->text().trimmed();
    request.projectDirectory = output_edit_->text().trimmed();
    request.runName = run_name_edit_->text().trimmed().isEmpty() ? QStringLiteral("train") : run_name_edit_->text().trimmed();
    progress_->setValue(0);
    log_->appendPlainText(tr("开始训练：%1").arg(request.runName));
    controller_.startTraining(request);
}

void YoloWorkspaceWidget::renderInference(const QJsonObject& result)
{
    QVector<CanvasOverlay> overlays;
    const QJsonArray predictions = result.value(QStringLiteral("predictions")).toArray();
    for (const QJsonValue& value : predictions) {
        const QJsonObject prediction = value.toObject();
        const int class_id = prediction.value(QStringLiteral("class_id")).toInt();
        const QString label = prediction.value(QStringLiteral("label")).toString();
        const double confidence = prediction.value(QStringLiteral("confidence")).toDouble();
        const QString text = QStringLiteral("%1  %2%").arg(label).arg(confidence * 100.0, 0, 'f', 1);
        result_list_->addItem(text);
        QVector<QPointF> points;
        const QJsonArray polygon = prediction.value(QStringLiteral("polygon")).toArray();
        for (const QJsonValue& point_value : polygon) {
            const QJsonArray point = point_value.toArray();
            if (point.size() >= 2) points.push_back({point[0].toDouble() * current_image_.width(), point[1].toDouble() * current_image_.height()});
        }
        CanvasTool kind = CanvasTool::Polygon;
        if (points.size() < 3) {
            points.clear();
            const QJsonArray box = prediction.value(QStringLiteral("box")).toArray();
            if (box.size() >= 4) {
                points = {{box[0].toDouble() * current_image_.width(), box[1].toDouble() * current_image_.height()},
                    {box[2].toDouble() * current_image_.width(), box[3].toDouble() * current_image_.height()}};
                kind = CanvasTool::Rectangle;
            }
        }
        if (!points.isEmpty()) overlays.push_back({kind, points, text, classColor(class_id)});
    }
    if (predictions.isEmpty()) result_list_->addItem(tr("未发现符合阈值的结果"));
    result_list_->addItem(tr("耗时 %1 ms").arg(result.value(QStringLiteral("elapsed_ms")).toDouble(), 0, 'f', 1));
    emit overlaysChanged(overlays);
    emit statusMessage(tr("YOLO 推理完成，共 %1 个结果").arg(predictions.size()));
}

void YoloWorkspaceWidget::setControlsBusy(bool busy, const QString& operation)
{
    infer_button_->setEnabled(!busy && !current_image_.isNull());
    train_button_->setEnabled(!busy);
    cancel_button_->setEnabled(busy);
    if (busy) emit statusMessage(tr("YOLO 正在执行：%1").arg(operation));
}

QColor YoloWorkspaceWidget::classColor(int classId)
{
    static const QColor colors[] = {QColor(59, 130, 246), QColor(34, 197, 94), QColor(249, 115, 22),
        QColor(168, 85, 247), QColor(236, 72, 153), QColor(20, 184, 166), QColor(234, 179, 8)};
    return colors[std::abs(classId) % (sizeof(colors) / sizeof(colors[0]))];
}
