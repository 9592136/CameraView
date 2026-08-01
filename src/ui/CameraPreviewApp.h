#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <commctrl.h>

#include <memory>

#include "app/DiagnosticReportActions.h"
#include "app/ExportActions.h"
#include "app/ProjectActions.h"
#include "app/ProjectSessionRestorer.h"
#include "camera/CameraControlStatusFormatter.h"
#include "camera/CameraDevice.h"
#include "camera/CameraDeviceListFormatter.h"
#include "camera/CameraTelemetryFormatter.h"
#include "camera/FrameBuffer.h"
#include "camera/MUCamCameraDriver.h"
#include "domain/CalibrationProfile.h"
#include "domain/ImageFrame.h"
#include "domain/Measurement.h"
#include "domain/MeasurementCollection.h"
#include "domain/MeasurementNameFormatter.h"
#include "imaging/ChannelFusionEngine.h"
#include "imaging/DyeLibrary.h"
#include "imaging/EdfProcessor.h"
#include "imaging/GeometryOps.h"
#include "imaging/FluorescenceChannelListActions.h"
#include "imaging/FluorescenceChannelUpdater.h"
#include "imaging/ImageStitcher.h"
#include "imaging/LiveStitchCapturePlanner.h"
#include "imaging/LiveStitchPreviewBuilder.h"
#include "imaging/ImageViewport.h"
#include "imaging/OverlayRenderer.h"
#include "imaging/PreviewDisplayActions.h"
#include "imaging/ProcessingParameterRules.h"
#include "imaging/ProcessingPanelActions.h"
#include "imaging/ProcessingJobState.h"
#include "imaging/ProcessingRetryActions.h"
#include "imaging/ProcessingStartActions.h"
#include "imaging/ProcessingResultActions.h"
#include "imaging/ProcessingResultFrames.h"
#include "imaging/ProcessingRetryState.h"
#include "imaging/ProcessingWorkerActions.h"
#include "imaging/StitchTileListActions.h"
#include "imaging/ViewportInteractionActions.h"
#include "platform/FileDialog.h"
#include "storage/ImageExporter.h"
#include "platform/TextInputParser.h"
#include "ui/ControlIds.h"
#include "ui/CameraPanelActions.h"
#include "ui/DyeLibraryActions.h"
#include "ui/DyeProfileFormPresenter.h"
#include "ui/FluorescenceDisplayActions.h"
#include "ui/FluorescenceChannelFormPresenter.h"
#include "ui/MeasurementDisplayActions.h"
#include "ui/MeasurementEditSession.h"
#include "ui/MeasurementHitTester.h"
#include "ui/MeasurementInteractionActions.h"
#include "ui/MeasurementInteractionState.h"
#include "ui/MeasurementListActions.h"
#include "ui/MeasurementToolAvailability.h"
#include "ui/MeasurementToolStartActions.h"
#include "ui/ProcessingBuildInputActions.h"
#include "ui/ProcessingQueueActions.h"
#include "ui/StitchTileDisplayActions.h"
#include "ui/WindowControlDefinitions.h"
#include "ui/WindowControlLayout.h"
#include "ui/WindowLayout.h"

#include "ui/LayoutUtils.h"
#include "ui/CameraViewCoordinator.h"
#include "ui/FluorescenceViewCoordinator.h"
#include "ui/MeasurementViewCoordinator.h"
#include "ui/ProcessingViewCoordinator.h"
#include "ui/ViewportViewCoordinator.h"
#include "imaging/HistogramCalculator.h"
#include "ui/HistogramRenderer.h"
#include "imaging/ImageAdjuster.h"
#include "i18n/Localization.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ui/UIConstants.h"
#include "ui/WindowProperties.h"
#include "ui/MainMenu.h"
#include "ui/StringUtils.h"


class CameraPreviewApp {
    struct ReportTemplateDesignerState {
        CameraPreviewApp* app = nullptr;
        HWND title_label = nullptr;
        HWND title_edit = nullptr;
        HWND subtitle_label = nullptr;
        HWND subtitle_edit = nullptr;
        HWND page_layout_label = nullptr;
        HWND page_layout_standard = nullptr;
        HWND page_layout_wide = nullptr;
        HWND page_layout_compact = nullptr;
        HWND print_orientation_label = nullptr;
        HWND print_orientation_portrait = nullptr;
        HWND print_orientation_landscape = nullptr;
        HWND image_checkbox = nullptr;
        HWND image_size_label = nullptr;
        HWND image_size_original = nullptr;
        HWND image_size_fit = nullptr;
        HWND image_size_compact = nullptr;
        HWND image_caption_label = nullptr;
        HWND image_caption_edit = nullptr;
        HWND info_checkbox = nullptr;
        HWND notes_checkbox = nullptr;
        HWND summary_checkbox = nullptr;
        HWND table_checkbox = nullptr;
        HWND raw_values_checkbox = nullptr;
        HWND group_measurements_checkbox = nullptr;
        HWND measurement_precision_label = nullptr;
        HWND measurement_precision_auto = nullptr;
        HWND measurement_precision_two = nullptr;
        HWND measurement_precision_three = nullptr;
        HWND calibration_checkbox = nullptr;
        HWND processing_checkbox = nullptr;
        HWND footer_checkbox = nullptr;
        HWND footer_text_label = nullptr;
        HWND footer_text_edit = nullptr;
        HWND accent_label = nullptr;
        HWND accent_blue = nullptr;
        HWND accent_green = nullptr;
        HWND accent_gold = nullptr;
        HWND accent_magenta = nullptr;
        HWND info_label = nullptr;
        HWND info_edit = nullptr;
        HWND notes_label = nullptr;
        HWND notes_edit = nullptr;
        HWND section_label = nullptr;
        HWND section_list = nullptr;
        HWND section_heading_label = nullptr;
        HWND section_heading_edit = nullptr;
        HWND left_scroll_bar = nullptr;
        HWND preview = nullptr;
        HWND status = nullptr;
        HWND last_text_target = nullptr;
        struct PreviewSectionHitArea {
            ImageReportTemplateSection section = ImageReportTemplateSection::CurrentImage;
            RECT bounds{};
        };
        std::vector<PreviewSectionHitArea> preview_hit_areas;
        int left_scroll_offset = 0;
        int left_scroll_max = 0;
        int section_heading_anchor_top = 0;
        int section_heading_anchor_height = 0;
        std::vector<ImageReportTemplateSection> section_order;
        std::wstring current_image_heading;
        std::wstring report_information_heading;
        std::wstring notes_heading;
        std::wstring measurement_summary_heading;
        std::wstring measurement_table_heading;
        std::wstring image_details_heading;
        bool initialized = false;
        bool syncing_section_heading = false;
    };

public:
    explicit CameraPreviewApp(HWND hwnd);
    void Start();
    void Stop();
    bool RefreshCameraList(HWND combo);
    void UpdateSelectedCamera(HWND combo);
    void ApplyExposure(HWND edit);
    void ApplyAutoExposure();
    void ApplyGain(HWND edit);
    void ApplyWhiteBalance();
    void UpdatePseudoColor(HWND combo);
    void UpdateHistogramChannel(HWND combo);
    void UpdateAdjustValueLabels(HWND hwnd);
    void ResetImageAdjust(HWND hwnd);
    void OnHistogramSlider(HWND hwnd, LPARAM lp);
    void ConstrainWindowWidth(HWND hwnd);
    void SyncPanelCardButtons() const;
    void ShowPanelCategory(int panel_category);
    int PanelCategory() const;
    bool FunctionPanelVisible() const;
    bool FunctionPanelDockedLeft() const;
    int FunctionPanelWidth() const;
        UILanguage CurrentLanguage() const;
    void SyncFunctionPanelChrome() const;
    void SetFunctionPanelVisible(bool visible);
    void ToggleFunctionPanel();
    void ReloadUILanguage(UILanguage new_lang);
    // ── About Dialog ──────────────────────────────────────────────────────

    struct AboutDialogData {
        const wchar_t* title;
        const wchar_t* desc;
        const wchar_t* version;
        const wchar_t* author;
        const wchar_t* ok_text;
    };

    static INT_PTR CALLBACK AboutDialogProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
    // Build a dialog template in dynamically allocated memory
    static std::vector<unsigned char> BuildAboutDialogTemplate();
    void ShowAboutDialog();
    void UpdatePanelControlTexts();
    void ApplyFunctionPanelDockedLeft(bool dock_left, bool announce);
    void SetFunctionPanelDockedLeft(bool dock_left);
    void ToggleFunctionPanelDock();
    RECT FunctionPanelTitleRect() const;
    bool PointInFunctionPanelTitle(POINT point) const;
    RECT FunctionPanelResizeGripRect() const;
    bool PointInFunctionPanelResizeGrip(POINT point) const;
    bool ShouldShowFunctionPanelDockCursor(POINT point) const;
    bool BeginFunctionPanelResize(POINT point);
    bool ContinueFunctionPanelResize(POINT point);
    bool EndFunctionPanelResize();
    bool BeginFunctionPanelDockDrag(POINT point);
    bool ContinueFunctionPanelDockDrag(POINT point);
    bool EndFunctionPanelDockDrag();
    int PanelScrollOffset() const;
    void ClampPanelScroll();
    void SyncPanelScrollBar();
    bool DrawPanelCategoryButton(const DRAWITEMSTRUCT& item) const;
    // ── Toolbar icon button drawing ───────────────────────────────────────

    static bool IsToolbarButton(int control_id);
    // Draw a toolbar icon button with hover/active states and a colored icon
    bool DrawToolbarButton(const DRAWITEMSTRUCT& item);
    void InitializeObjectiveControls(HWND combo, HWND name_edit);
    void SelectObjective(HWND combo, HWND name_edit);
    void AddObjective(HWND combo, HWND name_edit);
    void RenameSelectedObjective(HWND combo, HWND name_edit);
    void DeleteSelectedObjective(HWND combo, HWND name_edit);
    void InitializeDyeCombo(HWND combo);
    void SyncSelectedDyeControls(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit, HWND red_edit, HWND green_edit, HWND blue_edit);
    void SaveDyeProfile(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit, HWND red_edit, HWND green_edit, HWND blue_edit);
    void DeleteSelectedDye(
        HWND combo,
        HWND name_edit,
        HWND excitation_edit,
        HWND emission_edit, HWND red_edit, HWND green_edit, HWND blue_edit);
    void UpdateFusionPreview(HWND checkbox);
    void AddCurrentFrameAsChannel(HWND dye_combo, HWND channel_list, HWND fusion_checkbox);
    void ClearFluorescenceChannels(HWND channel_list, HWND fusion_checkbox);
    void SyncSelectedChannelControls(HWND list, HWND visible_checkbox, HWND black_edit, HWND white_edit);
    void ApplySelectedChannelSettings(HWND list, HWND visible_checkbox, HWND black_edit, HWND white_edit);
    void AddCurrentFrameAsStitchTile();
    LiveStitchPreviewOptions CurrentLiveStitchPreviewOptions() const;
    LiveStitchCaptureOptions CurrentLiveStitchCaptureOptions() const;
    void ClearLiveStitchPreviewTileCache();
    void ClearLiveStitchReferenceTileCache();
    static LiveStitchPreviewTile BuildSharedTileRef( const StitchTile& tile, std::shared_ptr<const ImageFrame> shared_frame = nullptr);
    static std::vector<StitchTile> BuildStitchTilesFromRefs( const std::vector<LiveStitchPreviewTile>& refs);
    void AppendLiveStitchReferenceTileCache( const StitchTile& tile, std::shared_ptr<const ImageFrame> shared_frame = nullptr);
    void RebuildLiveStitchReferenceTileCache();
    void AppendLiveStitchPreviewTileCache( const StitchTile& tile, std::shared_ptr<const ImageFrame> shared_frame = nullptr);
    void RebuildLiveStitchPreviewTileCache();
    std::vector<LiveStitchPreviewTile> BuildLiveStitchPreviewSnapshot() const;
    std::vector<LiveStitchPreviewTile> BuildLiveStitchReferenceSnapshot();
    void InvalidateLiveStitchPreviewState();
    void WaitForLiveStitchPreviewWorker();
    void RequestLiveStitchPreviewUpdate();
    void LiveStitchPreviewThread();
    void ApplyLiveStitchPreviewResult();
    void InvalidateLiveStitchCaptureState();
    void WaitForLiveStitchCaptureWorker();
    void RequestLiveStitchCaptureEvaluation( std::shared_ptr<const ImageFrame> frame, LiveStitchCaptureOptions options);
    void LiveStitchCaptureThread();
    void ApplyLiveStitchCaptureResult();
    void SyncLiveStitchControls();
    void StartLiveStitchCapture();
    void StopLiveStitchCapture(bool update_status = true);
    void CaptureLiveStitchTick();
    void DeleteSelectedStitchTile();
    void SyncStitchRegistrationControl();
    static int ComboSelection(HWND combo, int fallback);
    void SyncStitchOverlapControls();
    void SyncStitchSourceStatusControl();
    void SetStitchSourceStatus(std::wstring status);
    void SetLiveStitchStatus(const std::wstring& message, bool force = false);
    void SyncStitchSettingsControls();
    void InitializeStitchControls();
    bool ReadIntegerRange(HWND edit, int min_value, int max_value, int& value, const std::wstring& message);
    std::optional<StitchProcessingOptions> ReadStitchProcessingOptions();
    void UpdateStitchOverlapFromSlider(HWND slider);
    void UpdateStitchSettingsFromControls();
    void UpdateStitchRegistrationMode();
    void ImportStitchImageFiles(const std::vector<std::wstring>& file_names, const std::wstring& source_label);
    void SelectStitchDirectory();
    void SelectStitchFiles();
    void BuildStitchPreview();
    void SaveStitchResult();
    void BuildEdfPreview();
    void ShowEdfFocusMap();
    void ShowEdfCompositeFrame();
    void ClearProcessing();
    void RetryProcessing();
    bool IsLiveCameraPreviewOverlayVisible() const;
    static bool HasDrawableBgrRows(const ImageFrame& frame);
    static int FastDownsampleScaleFor(const ImageFrame& source, int max_edge);
    static ImageFrame BuildFastDownsampledFrame(const ImageFrame& source, int scale);
    static ImageFrame BuildLivePreviewOverlayFrame(const ImageFrame& source);
    RECT LiveCameraPreviewOverlayRect(const RECT& preview) const;
    static RECT FitFrameRect(const RECT& bounds, const ImageFrame& frame);
    void DrawLiveCameraPreviewOverlay(HDC hdc, const RECT& preview);
    void Paint(HDC hdc);
    void PaintToWindow(HDC hdc, const RECT& dirty);
    bool HandleMouseWheel(POINT screen_point, short wheel_delta);
    bool ScrollPanel(short wheel_delta);
    bool HandlePanelScrollCommand(WORD scroll_request);
    bool ScrollPanelTo(int target_offset);
    void FitView();
    bool BeginPan(POINT point);
    bool ContinuePan(POINT point);
    void EndPan();
    void BeginCalibration(HWND length_edit, HWND unit_combo);
    void ClearCalibration();
    void BeginLengthMeasurement();
    void BeginAngleMeasurement();
    void BeginRectangleAreaMeasurement();
    void BeginPolygonAreaMeasurement();
    void FinishPolygonAreaMeasurement();
    void ClearMeasurements(HWND list);
    void DeleteSelectedMeasurement(HWND list);
    void SyncSelectedMeasurementName(HWND list, HWND edit);
    void RenameSelectedMeasurement(HWND list, HWND edit);
    bool BeginMeasurementEdit(POINT point, HWND list);
    bool ContinueMeasurementEdit(POINT point);
    void EndMeasurementEdit();
    void ExportMeasurementsCsv();
    void ExportImage();
    void OpenImage();
    bool OpenImageFile(const std::wstring& file_name, bool dropped);
    void OpenDroppedFiles(const std::vector<std::wstring>& file_names);
    void SaveDiagnosticsReport();
    void LoadReportTemplate();
    void ClearReportTemplate();
    void ShowReportTemplateDesigner();
    void SaveProject();
    void OpenProject();
    bool HandleLeftClick(POINT point);
private:
    MeasurementToolStartResult CurrentMeasurementToolAvailability(MeasurementToolStartKind kind) const;
    bool ApplyMeasurementInteractionActionResult(const MeasurementInteractionActionResult& result);
    void PublishProcessingResult(ProcessingJobResult result);
    void WaitForProcessingWorker();
    void RequestProcessingCancel();
    bool StartStitchProcessing( std::vector<StitchTile> tiles, StitchProcessingOptions options, bool remember_snapshot);
public:
    void InvalidatePreviewFrameCache();
    void HandleFrameReady();
    void ApplyProcessingResult();
private:
    static std::filesystem::path ApplicationDirectory();
    static std::filesystem::path ProcessingOutputDirectory(const wchar_t* subdirectory);
    static std::filesystem::path NextProcessingOutputPath( const wchar_t* subdirectory, const wchar_t* file_prefix);
    static std::filesystem::path AbsolutePathOrSelf(const std::filesystem::path& path);
    std::wstring SaveVisibleProcessingResult( const wchar_t* subdirectory, const wchar_t* file_prefix, const std::wstring& display_mode) const;
    static std::wstring ReadEditText(HWND edit, int max_chars);
    static std::wstring ReadWindowText(HWND control);
    static std::filesystem::path EnsureFileExtension( const std::filesystem::path& path, const std::wstring& extension);
    static std::filesystem::path ReportImagePathFor(const std::filesystem::path& report_path);
    static std::wstring AbsolutePathText(const std::filesystem::path& path);
    static void SetDesignerStatus(HWND status, const std::wstring& text);
    static void SetCheckbox(HWND checkbox, bool checked);
    static bool CheckboxChecked(HWND checkbox);
    static void SyncTemplateDesignerControlAvailability(ReportTemplateDesignerState* state);
    static ImageReportTemplateAccent ReadVisualTemplateAccent(const ReportTemplateDesignerState* state);
    static void SetVisualTemplateAccent( ReportTemplateDesignerState* state, ImageReportTemplateAccent accent);
    static ImageReportTemplateImageSize ReadVisualTemplateImageSize( const ReportTemplateDesignerState* state);
    static void SetVisualTemplateImageSize( ReportTemplateDesignerState* state, ImageReportTemplateImageSize image_size);
    static ImageReportTemplatePageLayout ReadVisualTemplatePageLayout( const ReportTemplateDesignerState* state);
    static void SetVisualTemplatePageLayout( ReportTemplateDesignerState* state, ImageReportTemplatePageLayout page_layout);
    static ImageReportTemplatePrintOrientation ReadVisualTemplatePrintOrientation( const ReportTemplateDesignerState* state);
    static void SetVisualTemplatePrintOrientation( ReportTemplateDesignerState* state, ImageReportTemplatePrintOrientation orientation);
    static ImageReportTemplateMeasurementPrecision ReadVisualTemplateMeasurementPrecision( const ReportTemplateDesignerState* state);
    static void SetVisualTemplateMeasurementPrecision( ReportTemplateDesignerState* state, ImageReportTemplateMeasurementPrecision precision);
    static std::vector<ImageReportTemplateSection> DefaultVisualTemplateSectionOrder();
    static const wchar_t* TemplateSectionLabel(ImageReportTemplateSection section);
    static const wchar_t* DefaultTemplateSectionHeading(ImageReportTemplateSection section);
    static std::wstring VisualTemplateSectionHeading( const ImageReportTemplateOptions& options, ImageReportTemplateSection section);
    static std::wstring TemplateSectionHeadingText( const ReportTemplateDesignerState* state, ImageReportTemplateSection section);
    static void SetTemplateSectionHeadingText( ReportTemplateDesignerState* state, ImageReportTemplateSection section, const std::wstring& heading);
    static std::wstring TemplateSectionListText( const ReportTemplateDesignerState* state, ImageReportTemplateSection section);
    static COLORREF TemplateAccentColor(ImageReportTemplateAccent accent);
    static COLORREF TemplateAccentSoftColor(ImageReportTemplateAccent accent);
    static void RefreshTemplateSectionList( ReportTemplateDesignerState* state, std::size_t selected_index = 0);
    static void MoveSelectedTemplateSection(ReportTemplateDesignerState* state, int delta);
    static void RefreshTemplateSectionHeadingEdit(ReportTemplateDesignerState* state);
    static void SaveSelectedTemplateSectionHeading(ReportTemplateDesignerState* state);
    struct TemplatePlaceholderEntry {
        const wchar_t* label;
        const wchar_t* token;
    };

    static const TemplatePlaceholderEntry* TemplatePlaceholderEntries(std::size_t& count);
    static bool IsTemplateTextTarget(const ReportTemplateDesignerState* state, HWND control);
    static bool IsTemplateTextTargetId(int control_id);
    static HWND ResolveTemplateTextTarget(ReportTemplateDesignerState* state);
    static void InsertTemplatePlaceholder( ReportTemplateDesignerState* state, const std::wstring& token);
    static void ShowTemplatePlaceholderMenu(HWND hwnd, ReportTemplateDesignerState* state);
    static ImageReportTemplateOptions ReadVisualTemplateOptions(const ReportTemplateDesignerState* state);
    static void ApplyVisualTemplateOptions( ReportTemplateDesignerState* state, const ImageReportTemplateOptions& options);
    static void RefreshTemplatePreview(ReportTemplateDesignerState* state);
    static std::optional<std::size_t> SelectedTemplateSectionIndex( const ReportTemplateDesignerState* state);
    static bool SelectTemplateSectionIndex( ReportTemplateDesignerState* state, std::size_t selected_index);
    static bool SelectTemplateSection( ReportTemplateDesignerState* state, ImageReportTemplateSection section);
    static std::optional<ImageReportTemplateSection> HitTestTemplatePreviewSection( const ReportTemplateDesignerState* state, POINT point);
    static std::wstring PreviewFrameSizeText(const ImageFrame& frame);
    static RECT FitPreviewImageRect(const RECT& bounds, const ImageFrame& frame);
    static void DrawPreviewImageThumbnail(HDC hdc, const RECT& bounds, const ImageFrame& frame);
    static std::wstring PreviewMeasurementTableText( const CameraPreviewApp* app, const ImageReportTemplateOptions& options);
    static std::wstring PreviewImageDetailsText(const CameraPreviewApp* app);
    void ApplyDesignedReportTemplate(ReportTemplateDesignerState* state);
    void SaveDesignedReportTemplate(HWND owner, ReportTemplateDesignerState* state);
    static void DrawTemplatePreview(HWND hwnd, ReportTemplateDesignerState* state);
    static bool ScrollReportTemplateDesignerLeftTo( HWND hwnd, ReportTemplateDesignerState* state, int target_offset);
    static bool ScrollReportTemplateDesignerLeftRangeIntoView(
        HWND hwnd, ReportTemplateDesignerState* state, int content_top, int content_height);
    static void ScrollReportTemplateDesignerToSectionHeading( HWND hwnd, ReportTemplateDesignerState* state);
    static bool HandleReportTemplateDesignerLeftScroll( HWND hwnd, ReportTemplateDesignerState* state, int scroll_request);
    static void RegisterReportTemplateDesignerClass();
    static LRESULT CALLBACK ReportTemplatePreviewWindowProc(
        HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK ReportTemplateDesignerWindowProc(
        HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static bool ReadPositiveNumber(HWND edit, double& value);
    static bool ReadByteValue(HWND edit, int& value);
    static bool DecodeUtf8(const std::string& bytes, std::wstring& text);
    static bool DecodeAnsi(const std::string& bytes, std::wstring& text);
    static bool DecodeUtf16Le(const std::string& bytes, std::wstring& text);
    static bool ReadTextFile( const std::filesystem::path& path, std::wstring& text, std::wstring& error);
    static std::vector<CalibrationProfile> MakeDefaultObjectiveCalibrations();
    static std::vector<std::wstring> MakeDefaultObjectiveLabels();
    static MeasurementUnit SelectedCalibrationUnit(HWND combo);
    int NormalizeObjectiveIndex(int index) const;
    void EnsureObjectiveCalibrationCount();
    std::wstring ActiveObjectiveLabel() const;
    void StoreActiveObjectiveCalibration();
    void SyncObjectiveComboSelection(HWND combo) const;
    void RefreshObjectiveCombo(HWND combo) const;
    void SyncObjectiveNameEdit(HWND edit) const;
    bool ObjectiveLabelExists(const std::wstring& label, int ignored_index = -1) const;
    void SyncCalibrationStatus() const;
    void SyncReportTemplateStatus() const;
    static void ApplyCameraDeviceListPresentation( HWND combo, const CameraDeviceListPresentation& presentation);
    MeasurementUnit DisplayUnit() const;
    DyeProfile SelectedDye(HWND combo) const;
    std::optional<std::size_t> SelectedDyeIndex(HWND combo) const;
    std::optional<std::size_t> SelectedChannelIndex(HWND list) const;
    bool HasVisibleFusionPreviewChannel() const;
    bool IsSamePreviewCacheSource(const ImageFrame& source) const;
    void RecordPreviewCacheSource(const ImageFrame& source) const;
    const ImageFrame& CurrentPreviewFrame() const;
    MeasurementOverlayModel BuildMeasurementOverlayModel() const;
    std::size_t MeasurementCount() const;
    std::optional<MeasurementReference> SelectedMeasurement(HWND list) const;
    void SelectMeasurementInList(HWND list, MeasurementReference reference) const;
    void RefreshMeasurementList(HWND list);
    void RefreshChannelList(HWND list);
    void RefreshStitchTileList(HWND list);
    void AppendStitchTileListItem(HWND list);
    void ClearLatestFrame();
    void OnFrameReady();
    void CaptureThread();
    static std::wstring FormatStitchTilePositions(const StitchResultMetadata& metadata);
    DiagnosticReportActionInput BuildDiagnosticsInput();
    std::wstring BuildDiagnosticsReport();
    std::wstring BuildImageReport( const std::wstring& image_file_name, const ImageFrame& report_image);
    std::wstring BuildSdkTelemetry() const;
    void SetStatus(const std::wstring& text);
    void SetPreviewTelemetry(const std::wstring& text);
    void SetLatestFrameSource(const std::wstring& text);
    static void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color);
    bool EnsurePaintBuffer(HDC reference_dc, int width, int height);
    void ReleasePaintBuffer();
    HWND hwnd_ = nullptr;
    MUCamCameraDriver camera_driver_;
    std::vector<CameraDevice> camera_devices_;

    std::atomic_bool running_ = false;
    std::thread worker_;
    std::thread processing_worker_;
    std::thread live_stitch_capture_worker_;
    std::thread live_stitch_preview_worker_;

    std::mutex status_mutex_;
    std::mutex settings_mutex_;
    std::mutex live_stitch_capture_mutex_;
    std::mutex live_stitch_preview_mutex_;

    FrameBuffer frame_buffer_;
    FrameBuffer live_preview_overlay_buffer_;
    mutable std::shared_ptr<const ImageFrame> preview_source_frame_;
    mutable ImageFrame empty_preview_frame_;
    mutable ImageFrame preview_frame_cache_;
    mutable bool preview_frame_cache_valid_ = false;
    mutable PreviewFrameCacheKind preview_frame_cache_kind_ = PreviewFrameCacheKind::None;
    mutable int preview_frame_cache_source_width_ = 0;
    mutable int preview_frame_cache_source_height_ = 0;
    mutable int preview_frame_cache_source_stride_ = 0;
    mutable unsigned long long preview_frame_cache_source_sequence_ = 0;
    mutable unsigned long preview_frame_cache_source_timestamp_ = 0;
    mutable const unsigned char* preview_frame_cache_source_data_ = nullptr;
    std::wstring status_ = L"Ready.";
    std::wstring preview_telemetry_;
    std::wstring latest_frame_source_;
    std::wstring report_template_text_;
    std::wstring report_template_path_;
    ImageReportTemplateOptions visual_report_template_options_;
    HWND report_template_designer_ = nullptr;
    float requested_exposure_ms_ = 10.0f;
    std::atomic_int camera_count_ = 0;
    std::atomic_int selected_camera_index_ = -1;
    ImageViewport image_viewport_;
    OverlayRenderer overlay_renderer_;
    PseudoColorPalette pseudo_color_palette_ = PseudoColorPalette::Original;
    HistogramData histogram_data_;
    HistogramChannel histogram_channel_ = HistogramChannel::Luminance;
    HistogramRenderer histo_renderer_;
    UILanguage current_language_ = UILanguage::English;
    ImageAdjustParams image_adjust_;
    mutable ImageFrame adjusted_preview_;
    mutable unsigned long long histogram_cache_seq_ = 0;
    mutable HistogramChannel histogram_cache_ch_ = static_cast<HistogramChannel>(-1);
    mutable ImageAdjustParams histogram_cache_adjust_;
    std::vector<DyeProfile> dye_library_ = DyeLibrary::DefaultDyes();
    std::vector<FluorescenceChannel> fluorescence_channels_;
    std::vector<StitchTile> stitch_tiles_;
    int stitch_search_percent_ = ProcessingParameterRules::DefaultStitchSearchPercent();
    int stitch_overlap_percent_ = ProcessingParameterRules::DefaultStitchOverlapPercent();
    bool stitch_use_orb_registration_ = true;
    StitchProcessingOptions stitch_options_;
    std::wstring stitch_source_status_ = L"(no images selected)";
    bool live_stitch_active_ = false;
    int live_stitch_interval_ms_ = kDefaultLiveStitchIntervalMs;
    bool live_stitch_tick_in_progress_ = false;
    unsigned long long live_stitch_last_evaluated_sequence_ = 0;
    std::size_t live_stitch_capture_count_ = 0;
    bool live_stitch_out_of_range_warning_ = false;
    int live_stitch_out_of_range_candidate_count_ = 0;
    int live_stitch_missing_match_count_ = 0;
    DWORD live_stitch_last_status_tick_ = 0;
    DWORD live_stitch_preview_status_tick_ = 0;
    DWORD live_stitch_last_warning_beep_tick_ = 0;
    std::wstring live_stitch_last_status_message_;
    bool live_stitch_capture_worker_running_ = false;
    bool live_stitch_capture_request_pending_ = false;
    bool live_stitch_capture_ready_ = false;
    unsigned long long live_stitch_capture_generation_ = 0;
    unsigned long long live_stitch_capture_min_generation_ = 0;
    LiveStitchCaptureRequest live_stitch_capture_pending_request_;
    LiveStitchCaptureResult live_stitch_capture_ready_result_;
    std::vector<LiveStitchPreviewTile> live_stitch_preview_tiles_;
    std::vector<LiveStitchPreviewTile> live_stitch_reference_tiles_;
    int live_stitch_preview_tile_scale_ = 0;
    bool live_stitch_preview_worker_running_ = false;
    bool live_stitch_preview_request_pending_ = false;
    bool live_stitch_preview_ready_ = false;
    unsigned long long live_stitch_preview_generation_ = 0;
    unsigned long long live_stitch_preview_min_generation_ = 0;
    unsigned long long live_stitch_preview_pending_generation_ = 0;
    unsigned long long live_stitch_preview_ready_generation_ = 0;
    std::vector<LiveStitchPreviewTile> live_stitch_preview_pending_tiles_;
    LiveStitchPreviewOptions live_stitch_preview_pending_options_;
    ImageFrame live_stitch_preview_ready_image_;
    StitchResultMetadata live_stitch_preview_ready_metadata_;
    unsigned long long live_preview_overlay_last_sequence_ = 0;
    DWORD live_preview_overlay_last_invalidate_tick_ = 0;
    std::vector<ImageFrame> edf_stack_;
    ProcessingRetryState processing_retry_;
    ProcessingResultFrames processing_frames_;
    EdfOptions edf_options_ = ProcessingParameterRules::DefaultEdfOptions();
    ProcessingJobState processing_state_;
    CalibrationProfile calibration_ = CalibrationProfile::Uncalibrated();
    std::vector<std::wstring> objective_labels_ = MakeDefaultObjectiveLabels();
    std::vector<CalibrationProfile> objective_calibrations_ = MakeDefaultObjectiveCalibrations();
    int selected_objective_index_ = 0;
    MeasurementCollection measurements_;
    MeasurementInteractionState measurement_interaction_;
    MeasurementEditSession edit_session_;
    double pending_calibration_length_ = 100.0;
    MeasurementUnit pending_calibration_unit_ = MeasurementUnit::Micrometers;
    bool show_fusion_preview_ = false;
    bool function_panel_visible_ = true;
    bool function_panel_docked_left_ = true;
    bool function_panel_drag_active_ = false;
    bool function_panel_resize_active_ = false;
    int function_panel_width_ = 0;
    int panel_category_ = 0;
    int panel_scroll_offset_ = 0;
    ViewportPanState viewport_pan_;
    HDC paint_buffer_dc_ = nullptr;
    HBITMAP paint_buffer_bitmap_ = nullptr;
    HGDIOBJ paint_buffer_old_bitmap_ = nullptr;
    int paint_buffer_width_ = 0;
    int paint_buffer_height_ = 0;
};

inline CameraPreviewApp* GetApp(HWND hwnd)
{
    return reinterpret_cast<CameraPreviewApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}