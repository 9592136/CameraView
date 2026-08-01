#pragma once

#include <windows.h>

#include "ControlIds.h"

/// Aggregates WM_COMMAND dispatch for the Processing panel
/// (stitch, EDF, live-stitch, retry / clear).
class ProcessingViewCoordinator {
public:
    /// Dispatches a WM_COMMAND to the appropriate CameraPreviewApp method.
    /// Returns true when the command was recognised and handled.
    /// @tparam TApp  Type providing CameraPreviewApp's public processing API.
    template <typename TApp>
    static bool DispatchCommand(TApp& app, HWND hwnd, WPARAM wparam, LPARAM /*lparam*/) {
        const int id   = LOWORD(wparam);
        const int code = HIWORD(wparam);

        if ((id == kIdStitchModeCombo   ||
             id == kIdStitchMethodCombo ||
             id == kIdStitchTransformCombo ||
             id == kIdStitchBlendCombo) &&
            code == CBN_SELCHANGE) {
            app.UpdateStitchSettingsFromControls();
            return true;
        }

        switch (id) {
        case kIdSelectStitchDirectory:
            app.SelectStitchDirectory();
            return true;
        case kIdSelectStitchFiles:
            app.SelectStitchFiles();
            return true;
        case kIdAddStitchTile:
            app.AddCurrentFrameAsStitchTile();
            return true;
        case kIdStartLiveStitch:
            app.StartLiveStitchCapture();
            return true;
        case kIdStopLiveStitch:
            app.StopLiveStitchCapture();
            return true;
        case kIdBuildStitch:
            app.BuildStitchPreview();
            return true;
        case kIdSaveStitchResult:
            app.SaveStitchResult();
            return true;
        case kIdStitchOrbRegistration:
            app.UpdateStitchRegistrationMode();
            return true;
        case kIdDeleteStitchTile:
            app.DeleteSelectedStitchTile();
            return true;
        case kIdClearStitchTiles:
            app.ClearStitchTiles();
            return true;
        case kIdAddEdfFrame:
            app.AddCurrentFrameAsEdfFrame();
            return true;
        case kIdBuildEdf:
            app.BuildEdfPreview();
            return true;
        case kIdShowEdfComposite:
            app.ShowEdfCompositeFrame();
            return true;
        case kIdShowEdfFocusMap:
            app.ShowEdfFocusMap();
            return true;
        case kIdRetryProcessing:
            app.RetryProcessing();
            return true;
        case kIdClearProcessing:
            app.ClearProcessing();
            return true;
        default:
            return false;
        }
    }
};
