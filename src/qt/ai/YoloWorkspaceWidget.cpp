#include "YoloWorkspaceWidget.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
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

QWidget* buttonRow(std::initializer_list<QPushButton*> buttons)
{
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QPushButton* button : buttons) layout->addWidget(button);
    return widget;
}

QString safeDatasetFolder(QString value)
{
    value = value.trimmed();
    const QString forbidden = QStringLiteral("<>:\"/\\|?*");
    for (qsizetype index = 0; index < value.size(); ++index) {
        if (value.at(index).unicode() < 32 || forbidden.contains(value.at(index))) {
            value[index] = QLatin1Char('_');
        }
    }
    while (value.endsWith(QLatin1Char('.')) || value.endsWith(QLatin1Char(' '))) value.chop(1);
    return value;
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
    workspace_tabs_ = new QTabWidget;
    workspace_tabs_->setObjectName(QStringLiteral("YoloWorkspaceTabs"));
    auto* tabs = workspace_tabs_;
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
    task_combo_->setObjectName(QStringLiteral("TrainingTaskCombo"));
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
    result_list_->setObjectName(QStringLiteral("YoloResultList"));
    result_list_->setMinimumHeight(120);
    result_list_->setToolTip(tr("双击检测或分割结果可定位到图像目标"));
    options_layout->addRow(tr("结果"), result_list_);
    inference_layout->addWidget(options_group);
    inference_layout->addStretch();
    auto* inference_scroll = new QScrollArea;
    inference_scroll->setWidgetResizable(true);
    inference_scroll->setFrameShape(QFrame::NoFrame);
    inference_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inference_scroll->setWidget(inference_page);
    inference_tab_index_ = tabs->addTab(inference_scroll, tr("推理与模型"));

    dataset_tab_index_ = tabs->addTab(buildDatasetPage(), tr("数据集"));

    auto* training_page = new QWidget;
    auto* training_layout = new QVBoxLayout(training_page);
    auto* train_form = new QFormLayout;
    training_model_edit_ = new QLineEdit;
    auto* choose_training_model = new QPushButton(tr("选择"));
    dataset_edit_ = new QLineEdit;
    dataset_edit_->setObjectName(QStringLiteral("TrainingDatasetEdit"));
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
    auto* training_scroll = new QScrollArea;
    training_scroll->setWidgetResizable(true);
    training_scroll->setFrameShape(QFrame::NoFrame);
    training_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    training_scroll->setWidget(training_page);
    training_tab_index_ = tabs->addTab(training_scroll, tr("训练"));

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
    connect(result_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem* item) {
            const QRectF bounds = item ? item->data(Qt::UserRole).toRectF() : QRectF();
            if (bounds.isEmpty()) return;
            emit focusRequested(bounds);
            emit statusMessage(tr("已定位：%1").arg(item->text()));
        });
    connect(choose_dataset, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseDataset);
    connect(choose_training_model, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseTrainingModel);
    connect(choose_output, &QPushButton::clicked, this, &YoloWorkspaceWidget::chooseTrainingOutput);
    connect(train_button_, &QPushButton::clicked, this, &YoloWorkspaceWidget::startTraining);
    connect(cancel_button_, &QPushButton::clicked, &controller_, &YoloProcessController::cancel);
    connect(workspace_tabs_, &QTabWidget::currentChanged, this,
        [this](int) { refreshVisibleOverlays(); });
}

QWidget* YoloWorkspaceWidget::buildDatasetPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* project_group = new QGroupBox(tr("数据集项目"));
    auto* project_layout = new QVBoxLayout(project_group);
    dataset_project_edit_ = new QLineEdit;
    dataset_project_edit_->setReadOnly(true);
    dataset_project_edit_->setPlaceholderText(tr("尚未创建或打开数据集"));
    auto* create_button = new QPushButton(tr("新建"));
    auto* open_button = new QPushButton(tr("打开"));
    auto* open_folder_button = new QPushButton(tr("打开目录"));
    project_layout->addWidget(dataset_project_edit_);
    project_layout->addWidget(buttonRow({create_button, open_button, open_folder_button}));
    auto* task_form = new QFormLayout;
    dataset_task_combo_ = new QComboBox;
    dataset_task_combo_->addItem(tr("目标检测"), yoloTaskKey(YoloTask::Detection));
    dataset_task_combo_->addItem(tr("实例分割"), yoloTaskKey(YoloTask::Segmentation));
    dataset_task_combo_->addItem(tr("图像分类"), yoloTaskKey(YoloTask::Classification));
    task_form->addRow(tr("任务类型"), dataset_task_combo_);
    project_layout->addLayout(task_form);
    dataset_summary_ = new QLabel(tr("创建或打开数据集后即可开始标注。"));
    dataset_summary_->setWordWrap(true);
    project_layout->addWidget(dataset_summary_);
    layout->addWidget(project_group);

    dataset_editor_ = new QWidget;
    auto* editor_layout = new QVBoxLayout(dataset_editor_);
    editor_layout->setContentsMargins(0, 0, 0, 0);

    auto* class_group = new QGroupBox(tr("类别与划分"));
    auto* class_layout = new QFormLayout(class_group);
    dataset_class_combo_ = new QComboBox;
    dataset_class_combo_->setObjectName(QStringLiteral("DatasetClassCombo"));
    auto* add_class_button = new QPushButton(tr("新增类别"));
    auto* rename_class_button = new QPushButton(tr("重命名"));
    auto* remove_class_button = new QPushButton(tr("删除类别"));
    class_layout->addRow(tr("当前类别"), dataset_class_combo_);
    class_layout->addRow(buttonRow({add_class_button, rename_class_button, remove_class_button}));
    dataset_split_combo_ = new QComboBox;
    dataset_split_combo_->setObjectName(QStringLiteral("DatasetSplitCombo"));
    dataset_split_combo_->addItem(tr("训练集"), QStringLiteral("train"));
    dataset_split_combo_->addItem(tr("验证集"), QStringLiteral("val"));
    dataset_split_combo_->addItem(tr("测试集"), QStringLiteral("test"));
    class_layout->addRow(tr("样本划分"), dataset_split_combo_);
    editor_layout->addWidget(class_group);

    auto* annotation_group = new QGroupBox(tr("当前图像标注"));
    auto* annotation_layout = new QVBoxLayout(annotation_group);
    dataset_annotate_button_ = new QPushButton(tr("绘制检测框"));
    dataset_annotate_button_->setObjectName(QStringLiteral("DatasetAnnotateButton"));
    auto* cancel_annotation_button = new QPushButton(tr("取消绘制"));
    annotation_layout->addWidget(buttonRow({dataset_annotate_button_, cancel_annotation_button}));
    dataset_annotation_list_ = new QListWidget;
    dataset_annotation_list_->setObjectName(QStringLiteral("DatasetAnnotationList"));
    dataset_annotation_list_->setMinimumHeight(100);
    annotation_layout->addWidget(dataset_annotation_list_);
    auto* remove_annotation_button = new QPushButton(tr("删除选中标注"));
    auto* clear_annotations_button = new QPushButton(tr("清空当前标注"));
    annotation_layout->addWidget(buttonRow({remove_annotation_button, clear_annotations_button}));
    auto* save_sample_button = new QPushButton(tr("保存当前图像到数据集"));
    save_sample_button->setObjectName(QStringLiteral("DatasetSaveSampleButton"));
    annotation_layout->addWidget(save_sample_button);
    editor_layout->addWidget(annotation_group);

    auto* sample_group = new QGroupBox(tr("数据集样本"));
    auto* sample_layout = new QVBoxLayout(sample_group);
    dataset_sample_list_ = new QListWidget;
    dataset_sample_list_->setObjectName(QStringLiteral("DatasetSampleList"));
    dataset_sample_list_->setMinimumHeight(140);
    dataset_sample_list_->setToolTip(tr("双击样本可重新打开并编辑标注"));
    sample_layout->addWidget(dataset_sample_list_);
    auto* open_sample_button = new QPushButton(tr("打开选中样本"));
    auto* remove_sample_button = new QPushButton(tr("删除选中样本"));
    sample_layout->addWidget(buttonRow({open_sample_button, remove_sample_button}));
    auto* use_training_button = new QPushButton(tr("用于训练"));
    use_training_button->setObjectName(QStringLiteral("DatasetUseTrainingButton"));
    sample_layout->addWidget(use_training_button);
    editor_layout->addWidget(sample_group);
    editor_layout->addStretch();
    layout->addWidget(dataset_editor_);
    layout->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);

    connect(create_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::createDataset);
    connect(open_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::openDataset);
    connect(open_folder_button, &QPushButton::clicked, this, [this] {
        if (dataset_project_.isOpen()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(dataset_project_.rootDirectory()));
        }
    });
    connect(add_class_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::addDatasetClass);
    connect(rename_class_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::renameDatasetClass);
    connect(remove_class_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::removeDatasetClass);
    connect(dataset_annotate_button_, &QPushButton::clicked, this, &YoloWorkspaceWidget::beginDatasetAnnotation);
    connect(cancel_annotation_button, &QPushButton::clicked, this, [this] {
        emit annotationToolRequested(CanvasTool::None, tr("已取消数据标注绘制"));
    });
    connect(remove_annotation_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::removeCurrentAnnotation);
    connect(clear_annotations_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::clearCurrentAnnotations);
    connect(save_sample_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::saveCurrentDatasetSample);
    connect(open_sample_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::openSelectedDatasetSample);
    connect(remove_sample_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::removeSelectedDatasetSample);
    connect(use_training_button, &QPushButton::clicked, this, &YoloWorkspaceWidget::useDatasetForTraining);
    connect(dataset_sample_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem*) { openSelectedDatasetSample(); });
    connect(dataset_annotation_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem* item) {
            if (!item) return;
            bool valid_index = false;
            const int index = item->data(Qt::UserRole).toInt(&valid_index);
            if (!valid_index || index < 0 || index >= current_annotations_.size()) return;
            const QVector<QPointF>& points = current_annotations_.at(index).points;
            if (points.isEmpty()) return;
            double left = points.first().x();
            double right = left;
            double top = points.first().y();
            double bottom = top;
            for (const QPointF& point : points) {
                left = std::min(left, point.x());
                right = std::max(right, point.x());
                top = std::min(top, point.y());
                bottom = std::max(bottom, point.y());
            }
            emit focusRequested(QRectF(QPointF(left, top), QPointF(right, bottom)));
        });

    refreshDatasetUi();
    return scroll;
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

void YoloWorkspaceWidget::setCurrentImage(
    const QImage& image,
    const QString& sourceName,
    const QString& sourceIdentity)
{
    const QString identity = sourceIdentity.isEmpty() ? sourceName : sourceIdentity;
    const bool image_changed = identity != current_source_identity_ ||
        image.size() != current_image_.size() || image.isNull();
    if (image_changed) {
        if (result_list_) result_list_->clear();
        inference_overlays_.clear();
        if (pending_dataset_sample_id_.isEmpty()) {
            current_dataset_sample_id_.clear();
            current_annotations_.clear();
        }
    }
    current_image_ = image;
    current_source_ = sourceName;
    current_source_identity_ = identity;
    infer_button_->setEnabled(!image.isNull() && !controller_.isBusy());
    if (!pending_dataset_sample_id_.isEmpty() && !image.isNull()) {
        current_dataset_sample_id_ = pending_dataset_sample_id_;
        current_annotations_ = pending_annotations_;
        const int split_index = dataset_split_combo_
            ? dataset_split_combo_->findData(pending_split_) : -1;
        if (split_index >= 0) dataset_split_combo_->setCurrentIndex(split_index);
        pending_dataset_sample_id_.clear();
        pending_annotations_.clear();
        pending_split_.clear();
        emit statusMessage(tr("数据集样本已打开，可继续编辑标注"));
    }
    if (image_changed) refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

bool YoloWorkspaceWidget::loadDatasetProject(const QString& rootDirectory, QString* error)
{
    YoloDatasetProject loaded;
    if (!loaded.load(rootDirectory, error)) return false;
    dataset_project_ = std::move(loaded);
    current_dataset_sample_id_.clear();
    current_annotations_.clear();
    cancelPendingDatasetImageOpen();
    refreshDatasetUi();
    updateDatasetOverlays();
    refreshVisibleOverlays();
    return true;
}

void YoloWorkspaceWidget::acceptCanvasAnnotation(CanvasTool tool, QVector<QPointF> points)
{
    if (!dataset_project_.isOpen() || current_image_.isNull()) return;
    const int class_id = dataset_class_combo_->currentIndex();
    if (class_id < 0 || class_id >= dataset_project_.classes().size()) return;
    const bool detection = dataset_project_.task() == YoloTask::Detection;
    const bool segmentation = dataset_project_.task() == YoloTask::Segmentation;
    if ((detection && (tool != CanvasTool::Rectangle || points.size() != 2)) ||
        (segmentation && (tool != CanvasTool::Polygon || points.size() < 3))) {
        emit statusMessage(tr("画布返回的标注类型与数据集任务不匹配"));
        return;
    }
    if (!detection && !segmentation) return;
    current_annotations_.push_back({class_id, std::move(points)});
    refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
    emit statusMessage(tr("已添加标注，保存当前图像后写入数据集"));
}

void YoloWorkspaceWidget::cancelPendingDatasetImageOpen()
{
    pending_dataset_sample_id_.clear();
    pending_annotations_.clear();
    pending_split_.clear();
}

void YoloWorkspaceWidget::createDataset()
{
    const QString parent = QFileDialog::getExistingDirectory(
        this, tr("选择新数据集的保存位置"),
        QSettings().value(QStringLiteral("yolo/dataset_parent")).toString());
    if (parent.isEmpty()) return;
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("新建数据集"), tr("数据集名称"), QLineEdit::Normal,
        QStringLiteral("cell_dataset"), &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    const QStringList task_labels = {tr("目标检测"), tr("实例分割"), tr("图像分类")};
    const QString task_label = QInputDialog::getItem(
        this, tr("新建数据集"), tr("任务类型"), task_labels, 0, false, &accepted);
    if (!accepted) return;
    const int task_index = task_labels.indexOf(task_label);
    const YoloTask task = task_index == 1 ? YoloTask::Segmentation
        : task_index == 2 ? YoloTask::Classification : YoloTask::Detection;
    const QString first_class = QInputDialog::getText(
        this, tr("新建数据集"), tr("第一个类别名称"), QLineEdit::Normal,
        QStringLiteral("cell"), &accepted).trimmed();
    if (!accepted || first_class.isEmpty()) return;
    const QString folder = safeDatasetFolder(name);
    if (folder.isEmpty()) {
        QMessageBox::warning(this, tr("新建失败"), tr("数据集名称不能用于创建目录。"));
        return;
    }

    YoloDatasetProject created;
    QString error;
    const QString root = QDir(parent).filePath(folder);
    if (!YoloDatasetProject::create(root, name, task, {first_class}, &created, &error)) {
        QMessageBox::warning(this, tr("新建失败"), error);
        return;
    }
    dataset_project_ = std::move(created);
    current_dataset_sample_id_.clear();
    current_annotations_.clear();
    QSettings().setValue(QStringLiteral("yolo/dataset_parent"), parent);
    refreshDatasetUi();
    updateDatasetOverlays();
    refreshVisibleOverlays();
    emit statusMessage(tr("数据集已创建：%1").arg(root));
}

void YoloWorkspaceWidget::openDataset()
{
    const QString root = QFileDialog::getExistingDirectory(
        this, tr("打开 CameraView 数据集"),
        QSettings().value(QStringLiteral("yolo/dataset_parent")).toString());
    if (root.isEmpty()) return;
    QString error;
    if (!loadDatasetProject(root, &error)) {
        QMessageBox::warning(this, tr("打开失败"), error);
        return;
    }
    QSettings().setValue(QStringLiteral("yolo/dataset_parent"), QFileInfo(root).absolutePath());
    refreshDatasetUi();
    updateDatasetOverlays();
    refreshVisibleOverlays();
    emit statusMessage(tr("数据集已打开：%1").arg(root));
}

void YoloWorkspaceWidget::refreshDatasetUi(const QString& selectSampleId)
{
    if (!dataset_project_edit_ || !dataset_editor_) return;
    const bool opened = dataset_project_.isOpen();
    dataset_project_edit_->setText(opened ? dataset_project_.rootDirectory() : QString());
    dataset_editor_->setEnabled(opened);
    dataset_task_combo_->setEnabled(!opened);
    if (!opened) {
        dataset_summary_->setText(tr("创建或打开数据集后即可开始标注。"));
        dataset_class_combo_->clear();
        dataset_annotation_list_->clear();
        dataset_sample_list_->clear();
        return;
    }

    const int task_index = dataset_task_combo_->findData(yoloTaskKey(dataset_project_.task()));
    if (task_index >= 0) dataset_task_combo_->setCurrentIndex(task_index);
    const int previous_class = dataset_class_combo_->currentIndex();
    dataset_class_combo_->clear();
    dataset_class_combo_->addItems(dataset_project_.classes());
    if (dataset_class_combo_->count() > 0) {
        dataset_class_combo_->setCurrentIndex(std::clamp(previous_class, 0, dataset_class_combo_->count() - 1));
    }
    dataset_annotate_button_->setText(dataset_project_.task() == YoloTask::Detection
        ? tr("绘制检测框")
        : dataset_project_.task() == YoloTask::Segmentation
            ? tr("绘制分割多边形") : tr("设置当前图像分类"));

    int train_count = 0;
    int val_count = 0;
    int test_count = 0;
    int annotation_count = 0;
    for (const YoloDatasetSample& sample : dataset_project_.samples()) {
        if (sample.split == QStringLiteral("train")) ++train_count;
        else if (sample.split == QStringLiteral("val")) ++val_count;
        else if (sample.split == QStringLiteral("test")) ++test_count;
        annotation_count += sample.annotations.size();
    }
    dataset_summary_->setText(tr("%1 · %2 · %3 个类别 · %4 个样本 / %5 个标注\n训练 %6 · 验证 %7 · 测试 %8")
        .arg(dataset_project_.name(), yoloTaskDisplayName(dataset_project_.task()))
        .arg(dataset_project_.classes().size()).arg(dataset_project_.samples().size())
        .arg(annotation_count).arg(train_count).arg(val_count).arg(test_count));

    dataset_annotation_list_->clear();
    for (int index = 0; index < current_annotations_.size(); ++index) {
        const YoloDatasetAnnotation& annotation = current_annotations_.at(index);
        const QString class_name = annotation.classId >= 0 && annotation.classId < dataset_project_.classes().size()
            ? dataset_project_.classes().at(annotation.classId) : tr("无效类别");
        QString geometry;
        if (dataset_project_.task() == YoloTask::Classification) {
            geometry = tr("整张图像");
        } else if (dataset_project_.task() == YoloTask::Detection && annotation.points.size() == 2) {
            const QRectF bounds(annotation.points.at(0), annotation.points.at(1));
            const QRectF normalized = bounds.normalized();
            geometry = tr("矩形 %1,%2  %3×%4")
                .arg(normalized.x(), 0, 'f', 0).arg(normalized.y(), 0, 'f', 0)
                .arg(normalized.width(), 0, 'f', 0).arg(normalized.height(), 0, 'f', 0);
        } else {
            geometry = tr("多边形 %1 点").arg(annotation.points.size());
        }
        auto* item = new QListWidgetItem(QStringLiteral("%1 · %2").arg(class_name, geometry), dataset_annotation_list_);
        item->setData(Qt::UserRole, index);
    }

    const QString wanted_sample = selectSampleId.isEmpty()
        ? (dataset_sample_list_->currentItem()
            ? dataset_sample_list_->currentItem()->data(Qt::UserRole).toString() : QString())
        : selectSampleId;
    dataset_sample_list_->clear();
    int selected_row = -1;
    for (const YoloDatasetSample& sample : dataset_project_.samples()) {
        const QString split = sample.split == QStringLiteral("train") ? tr("训练")
            : sample.split == QStringLiteral("val") ? tr("验证") : tr("测试");
        auto* item = new QListWidgetItem(
            tr("[%1] %2 · %3 个标注").arg(split, sample.sourceName).arg(sample.annotations.size()),
            dataset_sample_list_);
        item->setData(Qt::UserRole, sample.id);
        item->setToolTip(dataset_project_.absoluteImagePath(sample));
        if (sample.id == wanted_sample) selected_row = dataset_sample_list_->count() - 1;
    }
    if (selected_row >= 0) dataset_sample_list_->setCurrentRow(selected_row);
}

void YoloWorkspaceWidget::addDatasetClass()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("新增类别"), tr("类别名称"), QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    QString error;
    if (!dataset_project_.addClass(name, &error)) {
        QMessageBox::warning(this, tr("新增失败"), error);
        return;
    }
    refreshDatasetUi();
    dataset_class_combo_->setCurrentIndex(dataset_class_combo_->count() - 1);
}

void YoloWorkspaceWidget::renameDatasetClass()
{
    const int index = dataset_class_combo_->currentIndex();
    if (index < 0) return;
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("重命名类别"), tr("类别名称"), QLineEdit::Normal,
        dataset_project_.classes().at(index), &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    QString error;
    if (!dataset_project_.renameClass(index, name, &error)) {
        QMessageBox::warning(this, tr("重命名失败"), error);
        return;
    }
    refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

void YoloWorkspaceWidget::removeDatasetClass()
{
    const int index = dataset_class_combo_->currentIndex();
    if (index < 0) return;
    if (QMessageBox::question(this, tr("删除类别"),
            tr("确定删除类别“%1”？").arg(dataset_project_.classes().at(index))) != QMessageBox::Yes) return;
    QString error;
    if (!dataset_project_.removeClass(index, &error)) {
        QMessageBox::warning(this, tr("删除失败"), error);
        return;
    }
    for (int annotation_index = current_annotations_.size() - 1; annotation_index >= 0; --annotation_index) {
        if (current_annotations_.at(annotation_index).classId == index) {
            current_annotations_.removeAt(annotation_index);
        } else if (current_annotations_.at(annotation_index).classId > index) {
            --current_annotations_[annotation_index].classId;
        }
    }
    refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

void YoloWorkspaceWidget::beginDatasetAnnotation()
{
    if (!dataset_project_.isOpen() || current_image_.isNull()) {
        emit statusMessage(tr("请先打开数据集和要标注的图像"));
        return;
    }
    const int class_id = dataset_class_combo_->currentIndex();
    if (class_id < 0) {
        emit statusMessage(tr("请先选择标注类别"));
        return;
    }
    if (dataset_project_.task() == YoloTask::Classification) {
        current_annotations_ = {{class_id, {}}};
        refreshDatasetUi(current_dataset_sample_id_);
        updateDatasetOverlays();
        refreshVisibleOverlays();
        emit statusMessage(tr("已设置当前图像分类，点击保存写入数据集"));
        return;
    }
    const CanvasTool tool = dataset_project_.task() == YoloTask::Detection
        ? CanvasTool::Rectangle : CanvasTool::Polygon;
    const QString hint = tool == CanvasTool::Rectangle
        ? tr("请在图像上选择检测框的两个对角点")
        : tr("请依次选择分割轮廓顶点，双击完成");
    emit annotationToolRequested(tool, hint);
}

void YoloWorkspaceWidget::removeCurrentAnnotation()
{
    const int row = dataset_annotation_list_->currentRow();
    if (row < 0 || row >= current_annotations_.size()) return;
    current_annotations_.removeAt(row);
    refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

void YoloWorkspaceWidget::clearCurrentAnnotations()
{
    current_annotations_.clear();
    refreshDatasetUi(current_dataset_sample_id_);
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

void YoloWorkspaceWidget::saveCurrentDatasetSample()
{
    if (!dataset_project_.isOpen() || current_image_.isNull()) {
        emit statusMessage(tr("请先打开数据集和要保存的图像"));
        return;
    }
    const QString split = dataset_split_combo_->currentData().toString();
    QString source_name = current_source_.isEmpty() ? QStringLiteral("current-image") : current_source_;
    const int existing_index = dataset_project_.sampleIndex(current_dataset_sample_id_);
    if (existing_index >= 0) source_name = dataset_project_.samples().at(existing_index).sourceName;
    QString saved_id;
    QString error;
    if (!dataset_project_.saveSample(current_image_, source_name, split, current_annotations_,
            current_dataset_sample_id_, &saved_id, &error)) {
        QMessageBox::warning(this, tr("保存样本失败"), error);
        return;
    }
    current_dataset_sample_id_ = saved_id;
    refreshDatasetUi(saved_id);
    emit statusMessage(tr("样本已保存到 %1").arg(dataset_project_.rootDirectory()));
}

void YoloWorkspaceWidget::openSelectedDatasetSample()
{
    QListWidgetItem* item = dataset_sample_list_->currentItem();
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    const int index = dataset_project_.sampleIndex(id);
    if (index < 0) return;
    QImage image;
    QVector<YoloDatasetAnnotation> annotations;
    QString error;
    if (!dataset_project_.loadSample(id, &image, &annotations, &error)) {
        QMessageBox::warning(this, tr("打开样本失败"), error);
        return;
    }
    pending_dataset_sample_id_ = id;
    pending_annotations_ = annotations;
    pending_split_ = dataset_project_.samples().at(index).split;
    emit imageOpenRequested(dataset_project_.absoluteImagePath(dataset_project_.samples().at(index)));
}

void YoloWorkspaceWidget::removeSelectedDatasetSample()
{
    QListWidgetItem* item = dataset_sample_list_->currentItem();
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("删除样本"), tr("确定从数据集删除选中图像和标签？")) != QMessageBox::Yes) return;
    QString error;
    if (!dataset_project_.removeSample(id, &error)) {
        QMessageBox::warning(this, tr("删除失败"), error);
        return;
    }
    if (current_dataset_sample_id_ == id) {
        current_dataset_sample_id_.clear();
        current_annotations_.clear();
    }
    refreshDatasetUi();
    updateDatasetOverlays();
    refreshVisibleOverlays();
}

void YoloWorkspaceWidget::useDatasetForTraining()
{
    if (!dataset_project_.isOpen()) return;
    QString error;
    if (!dataset_project_.validate(&error)) {
        QMessageBox::warning(this, tr("数据集不可用"), error);
        return;
    }
    const bool has_training_sample = std::any_of(
        dataset_project_.samples().cbegin(), dataset_project_.samples().cend(),
        [](const YoloDatasetSample& sample) { return sample.split == QStringLiteral("train"); });
    if (!has_training_sample) {
        QMessageBox::information(this, tr("缺少训练样本"), tr("请先保存至少一张训练集图像。"));
        return;
    }
    dataset_edit_->setText(dataset_project_.trainingDataPath());
    const int task_index = task_combo_->findData(yoloTaskKey(dataset_project_.task()));
    if (task_index >= 0) task_combo_->setCurrentIndex(task_index);
    workspace_tabs_->setCurrentIndex(training_tab_index_);
    emit statusMessage(tr("训练数据已设置：%1").arg(dataset_project_.trainingDataPath()));
}

void YoloWorkspaceWidget::updateDatasetOverlays()
{
    dataset_overlays_.clear();
    if (!dataset_project_.isOpen()) return;
    for (const YoloDatasetAnnotation& annotation : current_annotations_) {
        if (annotation.points.isEmpty()) continue;
        const QString label = annotation.classId >= 0 && annotation.classId < dataset_project_.classes().size()
            ? dataset_project_.classes().at(annotation.classId) : tr("无效类别");
        const CanvasTool kind = dataset_project_.task() == YoloTask::Detection
            ? CanvasTool::Rectangle : CanvasTool::Polygon;
        dataset_overlays_.push_back({kind, annotation.points, label, classColor(annotation.classId)});
    }
}

void YoloWorkspaceWidget::refreshVisibleOverlays()
{
    if (!workspace_tabs_) return;
    emit overlaysChanged(workspace_tabs_->currentIndex() == dataset_tab_index_
        ? dataset_overlays_ : inference_overlays_);
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
        auto* item = new QListWidgetItem(text, result_list_);
        if (!points.isEmpty()) {
            double left = points.first().x();
            double right = left;
            double top = points.first().y();
            double bottom = top;
            for (const QPointF& point : points) {
                left = std::min(left, point.x());
                right = std::max(right, point.x());
                top = std::min(top, point.y());
                bottom = std::max(bottom, point.y());
            }
            const QRectF bounds(QPointF(left, top), QPointF(right, bottom));
            item->setData(Qt::UserRole, bounds);
            item->setToolTip(tr("双击定位到图像中的此目标"));
            overlays.push_back({kind, points, text, classColor(class_id)});
        }
    }
    if (predictions.isEmpty()) result_list_->addItem(tr("未发现符合阈值的结果"));
    result_list_->addItem(tr("耗时 %1 ms").arg(result.value(QStringLiteral("elapsed_ms")).toDouble(), 0, 'f', 1));
    inference_overlays_ = std::move(overlays);
    refreshVisibleOverlays();
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
