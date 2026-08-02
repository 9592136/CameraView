#pragma once

#include "YoloModelRegistry.h"
#include "YoloDatasetProject.h"
#include "YoloProcessController.h"
#include "../ImageCanvas.h"

#include <QImage>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTabWidget;

class YoloWorkspaceWidget final : public QWidget {
    Q_OBJECT

public:
    explicit YoloWorkspaceWidget(QWidget* parent = nullptr);
    void setCurrentImage(
        const QImage& image,
        const QString& sourceName = {},
        const QString& sourceIdentity = {});
    bool loadDatasetProject(const QString& rootDirectory, QString* error = nullptr);
    void acceptCanvasAnnotation(CanvasTool tool, QVector<QPointF> points);
    void cancelPendingDatasetImageOpen();

signals:
    void overlaysChanged(QVector<CanvasOverlay> overlays);
    void focusRequested(QRectF imageBounds);
    void annotationToolRequested(CanvasTool tool, QString hint);
    void imageOpenRequested(QString path);
    void statusMessage(QString message);

private slots:
    void renderInference(const QJsonObject& result);

private:
    void buildUi();
    QWidget* buildDatasetPage();
    void connectController();
    void discoverPython();
    void refreshModels(const QString& selectId = {});
    const YoloModelRecord* selectedModel() const;
    YoloTask selectedTask() const;
    void choosePython();
    void importModel();
    void removeModel();
    void inspectModel();
    void runInference();
    void chooseDataset();
    void chooseTrainingModel();
    void chooseTrainingOutput();
    void startTraining();
    void createDataset();
    void openDataset();
    void refreshDatasetUi(const QString& selectSampleId = {});
    void addDatasetClass();
    void renameDatasetClass();
    void removeDatasetClass();
    void beginDatasetAnnotation();
    void removeCurrentAnnotation();
    void clearCurrentAnnotations();
    void saveCurrentDatasetSample();
    void openSelectedDatasetSample();
    void removeSelectedDatasetSample();
    void useDatasetForTraining();
    void updateDatasetOverlays();
    void refreshVisibleOverlays();
    void setControlsBusy(bool busy, const QString& operation);
    static QColor classColor(int classId);

    YoloModelRegistry registry_;
    YoloDatasetProject dataset_project_;
    YoloProcessController controller_;
    QImage current_image_;
    QString current_source_;
    QString current_source_identity_;
    QVector<CanvasOverlay> inference_overlays_;
    QVector<CanvasOverlay> dataset_overlays_;
    QVector<YoloDatasetAnnotation> current_annotations_;
    QString current_dataset_sample_id_;
    QString pending_dataset_sample_id_;
    QVector<YoloDatasetAnnotation> pending_annotations_;
    QString pending_split_;

    QTabWidget* workspace_tabs_ = nullptr;
    int inference_tab_index_ = -1;
    int dataset_tab_index_ = -1;
    int training_tab_index_ = -1;

    QLineEdit* python_edit_ = nullptr;
    QLabel* runtime_status_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QComboBox* task_combo_ = nullptr;
    QLabel* model_info_ = nullptr;
    QDoubleSpinBox* confidence_spin_ = nullptr;
    QDoubleSpinBox* iou_spin_ = nullptr;
    QSpinBox* image_size_spin_ = nullptr;
    QSpinBox* max_detections_spin_ = nullptr;
    QLineEdit* device_edit_ = nullptr;
    QPushButton* infer_button_ = nullptr;
    QListWidget* result_list_ = nullptr;

    QLineEdit* dataset_project_edit_ = nullptr;
    QComboBox* dataset_task_combo_ = nullptr;
    QLabel* dataset_summary_ = nullptr;
    QWidget* dataset_editor_ = nullptr;
    QComboBox* dataset_split_combo_ = nullptr;
    QComboBox* dataset_class_combo_ = nullptr;
    QPushButton* dataset_annotate_button_ = nullptr;
    QListWidget* dataset_annotation_list_ = nullptr;
    QListWidget* dataset_sample_list_ = nullptr;

    QLineEdit* training_model_edit_ = nullptr;
    QLineEdit* dataset_edit_ = nullptr;
    QLineEdit* output_edit_ = nullptr;
    QLineEdit* run_name_edit_ = nullptr;
    QSpinBox* epochs_spin_ = nullptr;
    QSpinBox* batch_spin_ = nullptr;
    QSpinBox* workers_spin_ = nullptr;
    QSpinBox* patience_spin_ = nullptr;
    QPushButton* train_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};
