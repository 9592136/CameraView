#pragma once

#include "ImageCanvas.h"
#include "camera/CameraDevice.h"
#include "domain/CalibrationProfile.h"
#include "domain/ImageFrame.h"
#include "domain/MeasurementCollection.h"
#include "imaging/EdfProcessor.h"
#include "imaging/Fluorescence.h"
#include "imaging/HistogramCalculator.h"
#include "imaging/ImageAdjuster.h"
#include "imaging/ImageFilterProcessor.h"
#include "imaging/ImageStitcher.h"
#include "imaging/PseudoColorMapper.h"
#include "imaging/SmartTargetCounter.h"

#include <QMainWindow>
#include <QElapsedTimer>
#include <QStringList>
#include <QThread>

#include <atomic>
#include <memory>

class CameraWorker;
class HistogramWidget;
class YoloWorkspaceWidget;
class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QDockWidget;
class QLabel;
class QListWidget;
class QPushButton;
class QProgressBar;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;
class QToolBar;

class CameraMainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit CameraMainWindow(QWidget* parent = nullptr);
    ~CameraMainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void openImage();
    void exportImage();
    void saveProject();
    void openProject();
    void refreshDevices();
    void openSelectedCamera();
    void stopCamera();
    void onDevicesReady(QStringList labels, QVector<int> indices, QString diagnostic);
    void onCameraFrame(QImage image, quint64 sequence, quint32 timestamp);
    void onCanvasPoints(CanvasTool tool, QVector<QPointF> points);
    void addFluorescenceChannel();
    void clearFluorescenceChannels();
    void toggleFusion(bool enabled);
    void addStitchTile();
    void buildStitch();
    void addEdfFrame();
    void buildEdf();
    void showEdfFocusMap();
    void clearProcessing();
    void clearMeasurements();
    void deleteSelectedMeasurement();
    void exportMeasurements();
    void startSmartTargetSampleSelection();
    void runSmartTargetCounting();
    void clearSmartTargetCounting();
    void show3DView();
    void startProfileMeasurement();
    void applySelectedImageFilter();
    void undoImageFilter();
    void clearImageFilters();
    void updateImageFilterControls();
    void updateImagePresentation();
    void updateMeasurementList();

private:
    void setupUi();
    void setupMenusAndToolbar();
    QWidget* buildCameraPage();
    QWidget* buildImagePage();
    QWidget* buildFluorescencePage();
    QWidget* buildProcessingPage();
    QWidget* buildMeasurementPage();
    QWidget* buildProjectPage();
    void setCurrentFrame(ImageFrame frame, const QString& source, const QString& sourceIdentity = {});
    bool loadImageFile(const QString& fileName);
    ImageFrame currentVisibleFrame() const;
    QVector<CanvasOverlay> measurementOverlays() const;
    QVector<CanvasOverlay> smartTargetOverlays() const;
    QRectF measurementBounds(MeasurementReference reference) const;
    void focusSelectedMeasurement();
    void updateSmartTargetUi();
    void updateImageFilterPipelineUi();
    void rebuildOverlays();
    void setMeasurementTool(CanvasTool tool, const QString& hint);
    void setBusy(bool busy, const QString& message);
    void updateProcessingLabels();
    void refreshStitchTileList(int selectedRow = -1);
    void deleteSelectedStitchTile();
    StitchProcessingOptions stitchOptionsFromUi() const;
    void startLiveStitch();
    void stopLiveStitch(bool showStatus = true);
    void evaluateLiveStitch();
    void refreshLiveStitchPreview();
    void importStitchFiles(const QStringList& files, const QString& sourceDescription);
    void saveStitchResult();
    void invalidateStitchResult();
    static ImageFrame imageFrameFromQImage(const QImage& image, quint64 sequence = 0, quint32 timestamp = 0);
    static QImage qImageFromFrame(const ImageFrame& frame);
    static ImagePoint imagePoint(const QPointF& point);

    ImageCanvas* canvas_ = nullptr;
    QDockWidget* function_dock_ = nullptr;
    HistogramWidget* histogram_ = nullptr;
    QTabWidget* function_tabs_ = nullptr;
    YoloWorkspaceWidget* yolo_workspace_ = nullptr;
    QLabel* source_label_ = nullptr;
    QLabel* coordinate_label_ = nullptr;
    QLabel* zoom_label_ = nullptr;
    QLabel* preview_fps_label_ = nullptr;
    QLabel* camera_state_label_ = nullptr;
    QComboBox* device_combo_ = nullptr;
    QDoubleSpinBox* exposure_spin_ = nullptr;
    QDoubleSpinBox* gain_spin_ = nullptr;
    QComboBox* palette_combo_ = nullptr;
    QComboBox* histogram_channel_combo_ = nullptr;
    QComboBox* image_filter_combo_ = nullptr;
    QLabel* image_filter_parameter_label_ = nullptr;
    QSpinBox* image_filter_parameter_spin_ = nullptr;
    QLabel* image_filter_pipeline_label_ = nullptr;
    QSlider* brightness_slider_ = nullptr;
    QSlider* contrast_slider_ = nullptr;
    QSlider* gamma_slider_ = nullptr;
    QSlider* level_slider_ = nullptr;
    QSlider* width_slider_ = nullptr;
    QComboBox* dye_combo_ = nullptr;
    QListWidget* channel_list_ = nullptr;
    QCheckBox* fusion_check_ = nullptr;
    QCheckBox* channel_visible_check_ = nullptr;
    QSpinBox* channel_black_spin_ = nullptr;
    QSpinBox* channel_white_spin_ = nullptr;
    QLabel* stitch_count_label_ = nullptr;
    QLabel* stitch_backend_label_ = nullptr;
    QListWidget* stitch_tile_list_ = nullptr;
    QComboBox* stitch_layout_combo_ = nullptr;
    QSpinBox* stitch_rows_spin_ = nullptr;
    QSpinBox* stitch_cols_spin_ = nullptr;
    QSpinBox* stitch_overlap_spin_ = nullptr;
    QComboBox* stitch_registration_combo_ = nullptr;
    QComboBox* stitch_transform_combo_ = nullptr;
    QComboBox* stitch_blend_combo_ = nullptr;
    QProgressBar* stitch_progress_ = nullptr;
    QPushButton* stitch_start_button_ = nullptr;
    QPushButton* stitch_cancel_button_ = nullptr;
    QPushButton* stitch_save_button_ = nullptr;
    QSpinBox* live_stitch_interval_spin_ = nullptr;
    QPushButton* live_stitch_start_button_ = nullptr;
    QPushButton* live_stitch_stop_button_ = nullptr;
    QLabel* live_stitch_status_label_ = nullptr;
    QTimer* live_stitch_timer_ = nullptr;
    QLabel* edf_count_label_ = nullptr;
    QPushButton* focus_map_button_ = nullptr;
    QDoubleSpinBox* calibration_length_spin_ = nullptr;
    QComboBox* calibration_unit_combo_ = nullptr;
    QComboBox* display_unit_combo_ = nullptr;
    QListWidget* measurement_list_ = nullptr;
    QLabel* measurement_count_label_ = nullptr;
    QCheckBox* edge_snap_check_ = nullptr;
    QSpinBox* edge_snap_radius_spin_ = nullptr;
    QLabel* calibration_label_ = nullptr;
    QLabel* smart_sample_label_ = nullptr;
    QLabel* smart_result_label_ = nullptr;
    QDoubleSpinBox* smart_similarity_spin_ = nullptr;
    QSpinBox* smart_scale_tolerance_spin_ = nullptr;
    QPushButton* smart_select_button_ = nullptr;
    QPushButton* smart_count_button_ = nullptr;
    QProgressBar* smart_count_progress_ = nullptr;
    QListWidget* smart_result_list_ = nullptr;
    QAction* export_action_ = nullptr;
    QToolBar* measurement_toolbar_ = nullptr;

    QThread camera_thread_;
    CameraWorker* camera_worker_ = nullptr;
    QVector<int> camera_indices_;
    bool camera_open_ = false;
    QElapsedTimer preview_fps_timer_;
    int preview_frames_since_sample_ = 0;
    bool busy_ = false;
    bool ai_annotation_active_ = false;

    ImageFrame current_frame_;
    ImageFrame latest_camera_frame_;
    ImageFrame display_frame_;
    std::vector<ImageFilterStep> image_filter_pipeline_;
    QString current_source_;
    QString current_source_identity_;
    PseudoColorPalette palette_ = PseudoColorPalette::Original;
    HistogramChannel histogram_channel_ = HistogramChannel::Luminance;
    ImageAdjustParams adjustments_;
    CalibrationProfile calibration_ = CalibrationProfile::Uncalibrated();
    MeasurementUnit display_unit_ = MeasurementUnit::Micrometers;
    MeasurementCollection measurements_;
    QVector<CanvasOverlay> ai_overlays_;
    QVector<QPointF> profile_line_points_;
    std::vector<SmartTargetRegion> smart_target_samples_;
    SmartTargetCountResult smart_target_result_;
    std::shared_ptr<std::atomic_bool> smart_count_cancel_token_;
    bool smart_count_running_ = false;
    bool smart_count_session_active_ = false;
    quint64 image_generation_ = 0;
    std::vector<DyeProfile> dyes_;
    std::vector<FluorescenceChannel> channels_;
    std::vector<StitchTile> stitch_tiles_;
    QStringList stitch_tile_sources_;
    std::shared_ptr<std::atomic_bool> stitch_cancel_token_;
    ImageFrame stitch_result_;
    bool live_stitch_active_ = false;
    bool live_stitch_evaluating_ = false;
    bool live_stitch_preview_running_ = false;
    bool live_stitch_preview_pending_ = false;
    quint64 live_stitch_generation_ = 0;
    std::vector<ImageFrame> edf_stack_;
    EdfResult edf_result_;
    bool fusion_enabled_ = false;
};
