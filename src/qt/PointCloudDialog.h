#pragma once

#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudProcessor.h"

#include <QDialog>
#include <QVector>

#include <vector>

class PointCloudWidget;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

enum class PointCloudMeasureMode {
    Navigate,
    Point,
    Distance,
    HeightDifference,
    Angle,
    PointToPlane
};

struct PointCloudMeasurementRecord {
    QString type;
    QString value;
    QVector<int> point_indices;
};

class PointCloudDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PointCloudDialog(QWidget* parent = nullptr);
    explicit PointCloudDialog(const PointCloud& cloud, QWidget* parent = nullptr);

    PointCloudWidget* cloudWidget() const { return cloud_widget_; }
    const PointCloud& cloud() const { return current_cloud_; }
    int measurementCount() const { return static_cast<int>(measurements_.size()); }
    PointCloudMeasureMode measurementMode() const { return measure_mode_; }
    void setCloud(const PointCloud& cloud);
    void setMeasurementMode(PointCloudMeasureMode mode) { setMeasureMode(mode); }

private:
    void buildUi();
    void openCloud();
    void startCloudLoad(const QString& path);
    void exportCloud();
    void exportMeasurements();
    void updateCloudPresentation(const QString& operation = {});
    void updateCropRanges();
    void applyVoxelDownsample();
    void applyOutlierRemoval();
    void applySmartDenoise();
    void applyHoleRepair();
    void applyCrop();
    void beginInteractiveCrop();
    void acceptBoxSelection(const QVector<int>& indices);
    void applyInteractiveCrop(bool keep_selected);
    void clearInteractiveCrop();
    void fitPlane();
    void levelCloud();
    void undoProcessing();
    void restoreOriginal();
    void setMeasureMode(PointCloudMeasureMode mode);
    void finishMeasurement();
    void refreshMeasurementList();
    QString unitLabel() const;
    void pushProcessedCloud(PointCloud cloud, const QString& operation);

    PointCloudWidget* cloud_widget_ = nullptr;
    QLabel* source_label_ = nullptr;
    QLabel* statistics_label_ = nullptr;
    QLabel* backend_label_ = nullptr;
    QLabel* plane_label_ = nullptr;
    QLabel* measurement_hint_ = nullptr;
    QComboBox* unit_combo_ = nullptr;
    QComboBox* color_combo_ = nullptr;
    QDoubleSpinBox* point_size_spin_ = nullptr;
    QDoubleSpinBox* voxel_spin_ = nullptr;
    QDoubleSpinBox* outlier_radius_spin_ = nullptr;
    QSpinBox* outlier_neighbors_spin_ = nullptr;
    QDoubleSpinBox* smart_radius_spin_ = nullptr;
    QSpinBox* smart_neighbors_spin_ = nullptr;
    QDoubleSpinBox* smart_sigma_spin_ = nullptr;
    QDoubleSpinBox* smart_deviation_spin_ = nullptr;
    QDoubleSpinBox* smart_smoothing_spin_ = nullptr;
    QDoubleSpinBox* repair_spacing_spin_ = nullptr;
    QSpinBox* repair_max_cells_spin_ = nullptr;
    QSpinBox* repair_search_spin_ = nullptr;
    QLabel* smart_filter_report_ = nullptr;
    QLabel* hole_repair_report_ = nullptr;
    QDoubleSpinBox* crop_min_x_ = nullptr;
    QDoubleSpinBox* crop_max_x_ = nullptr;
    QDoubleSpinBox* crop_min_y_ = nullptr;
    QDoubleSpinBox* crop_max_y_ = nullptr;
    QDoubleSpinBox* crop_min_z_ = nullptr;
    QDoubleSpinBox* crop_max_z_ = nullptr;
    QPushButton* undo_button_ = nullptr;
    QPushButton* level_button_ = nullptr;
    QPushButton* open_button_ = nullptr;
    QPushButton* begin_crop_button_ = nullptr;
    QPushButton* keep_crop_button_ = nullptr;
    QPushButton* remove_crop_button_ = nullptr;
    QLabel* crop_selection_label_ = nullptr;
    QListWidget* measurement_list_ = nullptr;

    PointCloud original_cloud_;
    PointCloud current_cloud_;
    std::vector<PointCloud> undo_stack_;
    PointCloudPlane fitted_plane_;
    PointCloudMeasureMode measure_mode_ = PointCloudMeasureMode::Navigate;
    QVector<int> pending_points_;
    std::vector<PointCloudMeasurementRecord> measurements_;
    QVector<int> crop_selection_;

private slots:
    void acceptPickedPoint(int index);
};
