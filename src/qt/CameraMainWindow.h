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
#include "imaging/ImageStitcher.h"
#include "imaging/PseudoColorMapper.h"

#include <QMainWindow>
#include <QThread>

class CameraWorker;
class HistogramWidget;
class YoloWorkspaceWidget;
class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;

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
    void rebuildOverlays();
    void setMeasurementTool(CanvasTool tool, const QString& hint);
    void setBusy(bool busy, const QString& message);
    void updateProcessingLabels();
    static ImageFrame imageFrameFromQImage(const QImage& image, quint64 sequence = 0, quint32 timestamp = 0);
    static QImage qImageFromFrame(const ImageFrame& frame);
    static ImagePoint imagePoint(const QPointF& point);

    ImageCanvas* canvas_ = nullptr;
    HistogramWidget* histogram_ = nullptr;
    QTabWidget* function_tabs_ = nullptr;
    YoloWorkspaceWidget* yolo_workspace_ = nullptr;
    QLabel* source_label_ = nullptr;
    QLabel* coordinate_label_ = nullptr;
    QLabel* zoom_label_ = nullptr;
    QLabel* camera_state_label_ = nullptr;
    QComboBox* device_combo_ = nullptr;
    QDoubleSpinBox* exposure_spin_ = nullptr;
    QDoubleSpinBox* gain_spin_ = nullptr;
    QComboBox* palette_combo_ = nullptr;
    QComboBox* histogram_channel_combo_ = nullptr;
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
    QLabel* edf_count_label_ = nullptr;
    QPushButton* focus_map_button_ = nullptr;
    QDoubleSpinBox* calibration_length_spin_ = nullptr;
    QComboBox* calibration_unit_combo_ = nullptr;
    QComboBox* display_unit_combo_ = nullptr;
    QListWidget* measurement_list_ = nullptr;
    QLabel* calibration_label_ = nullptr;
    QAction* export_action_ = nullptr;

    QThread camera_thread_;
    CameraWorker* camera_worker_ = nullptr;
    QVector<int> camera_indices_;
    bool camera_open_ = false;
    bool busy_ = false;
    bool ai_annotation_active_ = false;

    ImageFrame current_frame_;
    ImageFrame display_frame_;
    QString current_source_;
    QString current_source_identity_;
    PseudoColorPalette palette_ = PseudoColorPalette::Original;
    HistogramChannel histogram_channel_ = HistogramChannel::Luminance;
    ImageAdjustParams adjustments_;
    CalibrationProfile calibration_ = CalibrationProfile::Uncalibrated();
    MeasurementUnit display_unit_ = MeasurementUnit::Micrometers;
    MeasurementCollection measurements_;
    QVector<CanvasOverlay> ai_overlays_;
    std::vector<DyeProfile> dyes_;
    std::vector<FluorescenceChannel> channels_;
    std::vector<StitchTile> stitch_tiles_;
    std::vector<ImageFrame> edf_stack_;
    EdfResult edf_result_;
    bool fusion_enabled_ = false;
};
