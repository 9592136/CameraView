#pragma once

#include "YoloModelRegistry.h"
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

class YoloWorkspaceWidget final : public QWidget {
    Q_OBJECT

public:
    explicit YoloWorkspaceWidget(QWidget* parent = nullptr);
    void setCurrentImage(const QImage& image, const QString& sourceName = {});

signals:
    void overlaysChanged(QVector<CanvasOverlay> overlays);
    void statusMessage(QString message);

private:
    void buildUi();
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
    void renderInference(const QJsonObject& result);
    void setControlsBusy(bool busy, const QString& operation);
    static QColor classColor(int classId);

    YoloModelRegistry registry_;
    YoloProcessController controller_;
    QImage current_image_;
    QString current_source_;

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
