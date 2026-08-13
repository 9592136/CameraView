#pragma once

#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudGeometricModel.h"
#include "pointcloud/PointCloudProcessor.h"

#include <QDialog>
#include <QVector>

#include <vector>
#include <functional>

class PointCloudWidget;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSplitter;
class QSpinBox;
class QTabWidget;

enum class PointCloudMeasureMode {
    Navigate,
    Point,
    Distance,
    HeightDifference,
    Angle,
    PointToPlane,
    PlaneAngle,
    LineIntersection
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
    ~PointCloudDialog() override;

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
    void evaluateTolerances();
    void showDeviationDistribution();
    void beginSectionAnalysis();
    void acceptSectionSelection(
        const QVector<int>& indices,
        const QPointF& first,
        const QPointF& second);
    void updateCloudPresentation(const QString& operation = {}, bool reset_view = false);
    void resetInspectionResults();
    void updateProcessingDefaults();
    void applyVoxelDownsample();
    void applyOutlierRemoval();
    void applySmartDenoise();
    void applyHoleRepair();
    void beginInteractiveCrop();
    void acceptBoxSelection(const QVector<int>& indices);
    void applyInteractiveCrop(bool keep_selected);
    void clearInteractiveCrop();
    void updateSelectionPresentation();
    void clearFittedPlane();
    void clearGeometricModels();
    void fitGeometricModel(PointCloudGeometricModelType type);
    void acceptFitResult(PointCloudFitResult result, std::uint64_t revision);
    void refreshModelList();
    void selectModelRow(int row);
    void deleteSelectedModel();
    void setSelectedModelVisible(bool visible);
    std::vector<std::size_t> selectedSourceIndices() const;
    void fitPlane();
    void levelCloud();
    void undoProcessing();
    void redoProcessing();
    void restoreOriginal();
    void setMeasureMode(PointCloudMeasureMode mode);
    void finishMeasurement();
    void refreshMeasurementList();
    QString unitLabel() const;
    void pushProcessedCloud(PointCloud cloud, const QString& operation);
    void runCloudTask(
        const QString& operation,
        std::function<PointCloud()> task,
        std::function<void()> completed = {});
    void loadSettings();
    void saveSettings() const;

    PointCloudWidget* cloud_widget_ = nullptr;
    QButtonGroup* measurement_tool_group_ = nullptr;
    QSplitter* workspace_splitter_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QLabel* source_label_ = nullptr;
    QLabel* statistics_label_ = nullptr;
    QLabel* backend_label_ = nullptr;
    QLabel* plane_label_ = nullptr;
    QCheckBox* show_plane_check_ = nullptr;
    QLabel* measurement_hint_ = nullptr;
    QLabel* tolerance_summary_ = nullptr;
    QLabel* section_status_ = nullptr;
    QLabel* selection_status_ = nullptr;
    QLabel* workspace_status_ = nullptr;
    QLabel* fit_status_ = nullptr;
    QLabel* model_details_ = nullptr;
    QComboBox* unit_combo_ = nullptr;
    QComboBox* color_combo_ = nullptr;
    QComboBox* fit_scope_combo_ = nullptr;
    QComboBox* cylinder_axis_combo_ = nullptr;
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
    QPushButton* undo_button_ = nullptr;
    QPushButton* redo_button_ = nullptr;
    QPushButton* level_button_ = nullptr;
    QPushButton* open_button_ = nullptr;
    QPushButton* begin_crop_button_ = nullptr;
    QPushButton* keep_crop_button_ = nullptr;
    QPushButton* remove_crop_button_ = nullptr;
    QLabel* crop_selection_label_ = nullptr;
    QListWidget* measurement_list_ = nullptr;
    QListWidget* model_list_ = nullptr;
    QDoubleSpinBox* flatness_tolerance_ = nullptr;
    QDoubleSpinBox* cylindricity_tolerance_ = nullptr;
    QDoubleSpinBox* circularity_tolerance_ = nullptr;
    QDoubleSpinBox* warpage_tolerance_ = nullptr;
    QDoubleSpinBox* profile_tolerance_ = nullptr;
    QDoubleSpinBox* section_width_spin_ = nullptr;
    QDoubleSpinBox* fit_threshold_spin_ = nullptr;
    QDoubleSpinBox* minimum_radius_spin_ = nullptr;
    QDoubleSpinBox* maximum_radius_spin_ = nullptr;
    QCheckBox* residual_coloring_check_ = nullptr;
    QCheckBox* axes_check_ = nullptr;

    PointCloud original_cloud_;
    PointCloud current_cloud_;
    std::vector<PointCloud> undo_stack_;
    std::vector<PointCloud> redo_stack_;
    PointCloudPlane fitted_plane_;
    PointCloudMeasureMode measure_mode_ = PointCloudMeasureMode::Navigate;
    QVector<int> pending_points_;
    std::vector<PointCloudMeasurementRecord> measurements_;
    QVector<int> crop_selection_;
    std::vector<PointCloudGeometricModel> geometric_models_;
    std::uint64_t active_model_id_ = 0;
    std::uint64_t next_model_id_ = 1;
    std::uint64_t cloud_revision_ = 0;
    int plane_model_count_ = 0;
    int sphere_model_count_ = 0;
    int cylinder_model_count_ = 0;

private slots:
    void acceptPickedPoint(int index);
};
