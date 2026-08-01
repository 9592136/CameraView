#pragma once

#include <memory>
#include <vector>

#include "imaging/LiveStitchCapturePlanner.h"
#include "imaging/LiveStitchPreviewBuilder.h"

namespace CameraViewUI {

constexpr UINT kMsgFrameReady = WM_APP + 1;
constexpr UINT kMsgStatusChanged = WM_APP + 2;
constexpr UINT kMsgProcessingFinished = WM_APP + 3;
constexpr UINT kMsgLiveStitchPreviewReady = WM_APP + 4;
constexpr UINT kMsgLiveStitchCaptureReady = WM_APP + 5;
constexpr UINT_PTR kLiveStitchTimerId = 5001;
constexpr int kDefaultLiveStitchIntervalMs = 1200;
constexpr int kMinLiveStitchIntervalMs = 250;
constexpr int kMaxLiveStitchIntervalMs = 10000;
constexpr int kLiveStitchPreviewMaxEdge = 960;
constexpr int kLiveStitchPreviewTileMaxEdge = 640;
constexpr int kLiveStitchRegistrationMaxEdge = 224;
constexpr int kLiveStitchMinMovementPercent = 15;
constexpr int kLiveStitchMinOverlapPercent = 15;
constexpr int kLiveStitchReferenceTileCount = 5;
constexpr int kLiveStitchOutOfRangeWarningFrames = 3;
constexpr int kLiveStitchMissingMatchWarningFrames = 6;
constexpr DWORD kLiveStitchStatusMinIntervalMs = 1000;
constexpr DWORD kLiveStitchPreviewStatusMinIntervalMs = 700;
constexpr DWORD kLiveStitchWarningBeepMinIntervalMs = 2500;
constexpr DWORD kLivePreviewOverlayMinIntervalMs = 50;
constexpr int kLivePreviewOverlayMaxEdge = 420;
constexpr int kPanelTitleHeight = 34;
constexpr int kFunctionPanelResizeGripWidth = 8;
constexpr const wchar_t* kFunctionPanelVisibleProperty = L"CameraViewFunctionPanelVisible";
constexpr const wchar_t* kFunctionPanelDockLeftProperty = L"CameraViewFunctionPanelDockLeft";
constexpr const wchar_t* kFunctionPanelWidthProperty = L"CameraViewFunctionPanelWidth";
constexpr const wchar_t* kReportTemplateDesignerClassName = L"CameraViewReportTemplateDesigner";
constexpr const wchar_t* kReportTemplatePreviewClassName = L"CameraViewReportTemplatePreview";
constexpr int kReportTemplateDesignerDefaultWidth = 980;
constexpr int kReportTemplateDesignerDefaultHeight = 940;
constexpr int kReportTemplateDesignerMinWidth = 780;
constexpr int kReportTemplateDesignerMinHeight = 720;
constexpr INT_PTR kFunctionPanelVisibleValue = 1;
constexpr INT_PTR kFunctionPanelHiddenValue = 2;
constexpr INT_PTR kFunctionPanelDockLeftValue = 1;
constexpr INT_PTR kFunctionPanelDockRightValue = 2;
constexpr int kTemplateDesignerTitleLabel = 4101;
constexpr int kTemplateDesignerTitleEdit = 4102;
constexpr int kTemplateDesignerImage = 4103;
constexpr int kTemplateDesignerSummary = 4104;
constexpr int kTemplateDesignerTable = 4105;
constexpr int kTemplateDesignerCalibration = 4106;
constexpr int kTemplateDesignerProcessing = 4107;
constexpr int kTemplateDesignerFooter = 4108;
constexpr int kTemplateDesignerPreview = 4109;
constexpr int kTemplateDesignerStatus = 4110;
constexpr int kTemplateDesignerDefault = 4111;
constexpr int kTemplateDesignerApply = 4112;
constexpr int kTemplateDesignerSave = 4113;
constexpr int kTemplateDesignerClose = 4114;
constexpr int kTemplateDesignerSectionLabel = 4115;
constexpr int kTemplateDesignerSectionList = 4116;
constexpr int kTemplateDesignerMoveUp = 4117;
constexpr int kTemplateDesignerMoveDown = 4118;
constexpr int kTemplateDesignerNotes = 4119;
constexpr int kTemplateDesignerNotesLabel = 4120;
constexpr int kTemplateDesignerNotesEdit = 4121;
constexpr int kTemplateDesignerSubtitleLabel = 4122;
constexpr int kTemplateDesignerSubtitleEdit = 4123;
constexpr int kTemplateDesignerAccentLabel = 4124;
constexpr int kTemplateDesignerAccentBlue = 4125;
constexpr int kTemplateDesignerAccentGreen = 4126;
constexpr int kTemplateDesignerAccentGold = 4127;
constexpr int kTemplateDesignerAccentMagenta = 4128;
constexpr int kTemplateDesignerInfo = 4129;
constexpr int kTemplateDesignerInfoLabel = 4130;
constexpr int kTemplateDesignerInfoEdit = 4131;
constexpr int kTemplateDesignerImageSizeLabel = 4132;
constexpr int kTemplateDesignerImageSizeOriginal = 4133;
constexpr int kTemplateDesignerImageSizeFit = 4134;
constexpr int kTemplateDesignerImageSizeCompact = 4135;
constexpr int kTemplateDesignerRawValues = 4136;
constexpr int kTemplateDesignerGroupMeasurements = 4137;
constexpr int kTemplateDesignerImageCaptionLabel = 4138;
constexpr int kTemplateDesignerImageCaptionEdit = 4139;
constexpr int kTemplateDesignerSectionHeadingLabel = 4140;
constexpr int kTemplateDesignerSectionHeadingEdit = 4141;
constexpr int kTemplateDesignerFooterTextLabel = 4142;
constexpr int kTemplateDesignerFooterTextEdit = 4143;
constexpr int kTemplateDesignerInsertField = 4144;
constexpr int kTemplateDesignerPageLayoutLabel = 4145;
constexpr int kTemplateDesignerPageLayoutStandard = 4146;
constexpr int kTemplateDesignerPageLayoutWide = 4147;
constexpr int kTemplateDesignerPageLayoutCompact = 4148;
constexpr int kTemplateDesignerPrintOrientationLabel = 4149;
constexpr int kTemplateDesignerPrintOrientationPortrait = 4150;
constexpr int kTemplateDesignerPrintOrientationLandscape = 4151;
constexpr int kTemplateDesignerMeasurementPrecisionLabel = 4152;
constexpr int kTemplateDesignerMeasurementPrecisionAuto = 4153;
constexpr int kTemplateDesignerMeasurementPrecisionTwo = 4154;
constexpr int kTemplateDesignerMeasurementPrecisionThree = 4155;
constexpr int kTemplateDesignerLeftScrollBar = 4156;
constexpr int kTemplatePlaceholderMenuBase = 4300;

struct LiveStitchCaptureRequest {
    std::vector<LiveStitchPreviewTile> reference_tiles;
    std::shared_ptr<const ImageFrame> frame;
    LiveStitchCaptureOptions options;
    unsigned long long generation = 0;
    unsigned long long sequence = 0;
    std::size_t base_tile_count = 0;
};

struct LiveStitchCaptureResult {
    LiveStitchCaptureDecision decision;
    std::shared_ptr<const ImageFrame> frame;
    unsigned long long generation = 0;
    unsigned long long sequence = 0;
    std::size_t base_tile_count = 0;
    long long elapsed_ms = 0;
};

enum class PreviewFrameCacheKind {
    None,
    PseudoColor,
    Fusion
};

} // namespace CameraViewUI

// Bring constants into global scope for backward compatibility
using namespace CameraViewUI;
