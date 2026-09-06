#pragma once

#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudGeometricModel.h"
#include "pointcloud/PointCloudProcessor.h"
#include "pointcloud/PointCloudSection.h"

#include <QDialog>
#include <QList>
#include <QVector>

#include <array>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>

class PointCloudWidget;
class PointCloudSectionPlotWidget;
class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QGroupBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QWidget;

enum class PointCloudWorkspacePage {
    DataDisplay,
    Processing,
    GeometryMeasurement,
    AnalysisReport
};

enum class PointCloudInteractionMode {
    Browse,
    Select,
    Measure,
    SectionDraw,
    SectionEdit
};

enum class PointCloudCommandId {
    Open,
    ExportCloud,
    Browse,
    Select,
    ClearSelection,
    Undo,
    Redo,
    FitPlane,
    FitSphere,
    FitCylinder,
    MeasurePoint,
    MeasureDistance,
    MeasureHeight,
    MeasureAngle,
    MeasurePointToPlane,
    MeasurePlaneAngle,
    MeasureLineIntersection,
    DrawSection,
    ViewIsometric,
    ViewTop,
    ViewFront,
    ViewRight,
    ResetView,
    DeleteModel,
    ClearModels,
    DeleteMeasurement,
    ClearMeasurements,
    ExportMeasurements,
    DeleteSection,
    ClearSections,
    ExportSectionCsv,
    ExportSectionPng,
    ExportSectionReport,
    Count
};

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

signals:
    void sectionReportChanged(const QString& html);

private:
    void resizeEvent(QResizeEvent* event) override;
    void buildUi();
    void setWorkspacePage(PointCloudWorkspacePage page, bool reveal = true);
    void setInteractionMode(PointCloudInteractionMode mode);
    void updateToolbarPresentation();
    QString interactionStatusText() const;
    void updateResponsiveLayout(bool force = false);
    void setCompactDrawerOpen(bool open, bool showSection = false);
    void moveSectionWorkspace(bool intoDrawer);
    void beginInlineTask(const QString& operation, bool determinate = false);
    void updateInlineTaskProgress(int value, int maximum = 1000);
    void endInlineTask(const QString& message, const QString& status = QStringLiteral("ok"));
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
    void runSectionAnalysis(PointCloudSectionDefinition definition, bool preview = false);
    void acceptSectionProfile(PointCloudSectionProfile profile,
        std::uint64_t revision, std::uint64_t requestId);
    void refreshSectionList(int preferredRow = -1);
    void selectSectionRow(int row);
    void updateSectionEditors();
    void scheduleSectionReanalysis();
    void duplicateSection();
    void offsetSection();
    void deleteSection();
    void clearSections();
    void exportSectionCsv();
    void exportSectionPng();
    void exportSectionReport();
    void refreshSectionFeatureList();
    void applySectionFeatureEdit();
    void updateSectionCursorMetrics(double firstDistance, double secondDistance);
    void updateSectionPresentation();
    QString buildSectionReportHtml(bool completeDocument) const;
    void updateCloudPresentation(const QString& operation = {}, bool reset_view = false);
    void resetInspectionResults();
    void updateProcessingDefaults();
    void applyVoxelDownsample();
    void applyOutlierRemoval();
    void applySmartDenoise();
    void applyHoleRepair();
    void acceptBoxSelection(const QVector<int>& indices);
    void applyInteractiveCrop(bool keep_selected);
    void clearInteractiveCrop();
    void updateSelectionPresentation();
    void updateActionStates();
    QAction* commandAction(PointCloudCommandId id) const;
    void clearFittedPlane();
    void clearGeometricModels();
    void requestClearGeometricModels();
    void fitGeometricModel(PointCloudGeometricModelType type);
    void cancelActiveFit(const QString& message);
    void acceptFitResult(PointCloudFitResult result, std::uint64_t revision);
    void refreshModelList();
    void selectModelRow(int row);
    void deleteSelectedModel();
    void setSelectedModelVisible(bool visible);
    void selectMeasurementRow(int row);
    std::vector<std::size_t> selectedSourceIndices() const;
    void fitPlane();
    void levelCloud();
    void undoProcessing();
    void redoProcessing();
    void restoreOriginal();
    void setMeasureMode(PointCloudMeasureMode mode);
    void finishMeasurement();
    void refreshMeasurementList();
    void deleteSelectedMeasurement();
    void clearMeasurements();
    void requestClearMeasurements();
    void requestClearSections();
    QString unitLabel() const;
    bool pushProcessedCloud(PointCloud cloud, const QString& operation);
    void runCloudTask(
        const QString& operation,
        std::function<PointCloud()> task,
        std::function<void()> completed = {});
    void loadSettings();
    void saveSettings() const;

    PointCloudWidget* cloud_widget_ = nullptr;
    QButtonGroup* measurement_tool_group_ = nullptr;
    QSplitter* workspace_splitter_ = nullptr;
    QSplitter* view_section_splitter_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTabWidget* compact_drawer_tabs_ = nullptr;
    QWidget* side_panel_ = nullptr;
    QWidget* tool_panel_host_ = nullptr;
    QFrame* section_workspace_ = nullptr;
    QFrame* data_summary_bar_ = nullptr;
    QFrame* task_bar_ = nullptr;
    QLabel* task_page_title_ = nullptr;
    QLabel* task_label_ = nullptr;
    QProgressBar* task_progress_ = nullptr;
    QToolButton* task_cancel_button_ = nullptr;
    QToolButton* drawer_toggle_button_ = nullptr;
    QToolButton* toolbar_open_button_ = nullptr;
    QToolButton* toolbar_fit_button_ = nullptr;
    QToolButton* toolbar_measure_button_ = nullptr;
    QToolButton* toolbar_section_button_ = nullptr;
    QToolButton* toolbar_view_button_ = nullptr;
    QAction* open_action_ = nullptr;
    QAction* browse_action_ = nullptr;
    QAction* select_action_ = nullptr;
    QAction* clear_selection_action_ = nullptr;
    QAction* undo_action_ = nullptr;
    QAction* redo_action_ = nullptr;
    QAction* section_action_ = nullptr;
    std::array<QAction*, static_cast<std::size_t>(PointCloudCommandId::Count)>
        command_actions_{};
    QLabel* source_label_ = nullptr;
    QLabel* statistics_label_ = nullptr;
    QLabel* texture_status_label_ = nullptr;
    QLabel* backend_label_ = nullptr;
    QLabel* plane_label_ = nullptr;
    QCheckBox* show_plane_check_ = nullptr;
    QLabel* measurement_hint_ = nullptr;
    QLabel* tolerance_summary_ = nullptr;
    QLabel* section_status_ = nullptr;
    QLabel* section_cursor_status_ = nullptr;
    QLabel* selection_status_ = nullptr;
    QPushButton* navigation_button_ = nullptr;
    QPushButton* free_selection_button_ = nullptr;
    QPushButton* clear_selection_button_ = nullptr;
    QLabel* workspace_status_ = nullptr;
    QLabel* fit_status_ = nullptr;
    QLabel* model_details_ = nullptr;
    QComboBox* unit_combo_ = nullptr;
    QComboBox* color_combo_ = nullptr;
    QComboBox* view_preset_combo_ = nullptr;
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
    QPushButton* fit_plane_model_button_ = nullptr;
    QPushButton* fit_sphere_button_ = nullptr;
    QPushButton* fit_cylinder_button_ = nullptr;
    QPushButton* cancel_fit_button_ = nullptr;
    QPushButton* show_model_button_ = nullptr;
    QPushButton* hide_model_button_ = nullptr;
    QPushButton* delete_model_button_ = nullptr;
    QPushButton* clear_models_button_ = nullptr;
    QPushButton* delete_measurement_button_ = nullptr;
    QPushButton* clear_measurements_button_ = nullptr;
    QPushButton* export_measurements_button_ = nullptr;
    QLabel* crop_selection_label_ = nullptr;
    QListWidget* measurement_list_ = nullptr;
    QListWidget* model_list_ = nullptr;
    QListWidget* section_list_ = nullptr;
    QListWidget* section_feature_list_ = nullptr;
    QDoubleSpinBox* flatness_tolerance_ = nullptr;
    QDoubleSpinBox* cylindricity_tolerance_ = nullptr;
    QDoubleSpinBox* circularity_tolerance_ = nullptr;
    QDoubleSpinBox* warpage_tolerance_ = nullptr;
    QDoubleSpinBox* profile_tolerance_ = nullptr;
    QDoubleSpinBox* section_width_spin_ = nullptr;
    QDoubleSpinBox* section_spacing_spin_ = nullptr;
    QComboBox* section_reference_combo_ = nullptr;
    std::array<QDoubleSpinBox*, 6> section_endpoint_spins_{};
    QSpinBox* section_median_window_spin_ = nullptr;
    QSpinBox* section_smoothing_window_spin_ = nullptr;
    QSpinBox* section_gap_spin_ = nullptr;
    QDoubleSpinBox* section_sensitivity_spin_ = nullptr;
    QSpinBox* section_plateau_spin_ = nullptr;
    QCheckBox* section_visible_check_ = nullptr;
    QCheckBox* section_report_check_ = nullptr;
    QComboBox* section_feature_type_combo_ = nullptr;
    QDoubleSpinBox* section_feature_start_spin_ = nullptr;
    QDoubleSpinBox* section_feature_end_spin_ = nullptr;
    PointCloudSectionPlotWidget* section_plot_ = nullptr;
    QPushButton* begin_section_button_ = nullptr;
    QPushButton* duplicate_section_button_ = nullptr;
    QPushButton* offset_section_button_ = nullptr;
    QPushButton* delete_section_button_ = nullptr;
    QPushButton* clear_sections_button_ = nullptr;
    QPushButton* export_section_csv_button_ = nullptr;
    QPushButton* export_section_png_button_ = nullptr;
    QPushButton* export_section_report_button_ = nullptr;
    QDoubleSpinBox* fit_threshold_spin_ = nullptr;
    QDoubleSpinBox* minimum_radius_spin_ = nullptr;
    QDoubleSpinBox* maximum_radius_spin_ = nullptr;
    QCheckBox* residual_coloring_check_ = nullptr;
    QCheckBox* axes_check_ = nullptr;
    QCheckBox* texture_enhance_check_ = nullptr;

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
    std::vector<PointCloudSectionProfile> section_profiles_;
    std::uint64_t active_model_id_ = 0;
    std::uint64_t reference_plane_model_id_ = 0;
    std::uint64_t next_model_id_ = 1;
    std::uint64_t cloud_revision_ = 0;
    std::uint64_t active_section_id_ = 0;
    std::uint64_t next_section_id_ = 1;
    std::uint64_t section_request_id_ = 0;
    int plane_model_count_ = 0;
    int sphere_model_count_ = 0;
    int cylinder_model_count_ = 0;
    bool fit_running_ = false;
    bool section_editor_updating_ = false;
    std::shared_ptr<std::atomic_bool> section_cancel_token_;
    std::uint64_t fit_request_id_ = 0;
    PointCloudWorkspacePage workspace_page_ = PointCloudWorkspacePage::DataDisplay;
    PointCloudInteractionMode interaction_mode_ = PointCloudInteractionMode::Browse;
    bool compact_layout_ = false;
    bool compact_drawer_open_ = false;
    bool task_running_ = false;
    std::uint64_t task_status_revision_ = 0;
    int last_rendered_point_count_ = 0;
    QList<int> wide_workspace_sizes_{900, 380};
    QList<int> wide_section_sizes_{650, 0};
    std::shared_ptr<std::atomic_bool> active_task_cancel_token_;

private slots:
    void acceptPickedPoint(int index);
};
