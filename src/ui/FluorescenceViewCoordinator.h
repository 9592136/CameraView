#pragma once

#include <windows.h>

#include "ControlIds.h"

/// Aggregates WM_COMMAND dispatch for the Fluorescence & Dye panel
/// (dye profiles, fluorescence channels, pseudo-colour, histogram).
class FluorescenceViewCoordinator {
public:
    /// Dispatches a WM_COMMAND to the appropriate CameraPreviewApp method.
    /// Returns true when the command was recognised and handled.
    /// @tparam TApp  Type providing CameraPreviewApp's public fluorescence API.
    template <typename TApp>
    static bool DispatchCommand(TApp& app, HWND hwnd, WPARAM wparam, LPARAM /*lparam*/) {
        const int id   = LOWORD(wparam);
        const int code = HIWORD(wparam);

        if (id == kIdDyeCombo && code == CBN_SELCHANGE) {
            app.SyncSelectedDyeControls(GetDlgItem(hwnd, kIdDyeCombo),
                                        GetDlgItem(hwnd, kIdDyeNameEdit),
                                        GetDlgItem(hwnd, kIdDyeExcitationEdit),
                                        GetDlgItem(hwnd, kIdDyeEmissionEdit),
                                        GetDlgItem(hwnd, kIdDyeRedEdit),
                                        GetDlgItem(hwnd, kIdDyeGreenEdit),
                                        GetDlgItem(hwnd, kIdDyeBlueEdit));
            return true;
        }
        if (id == kIdChannelList && code == LBN_SELCHANGE) {
            app.SyncSelectedChannelControls(GetDlgItem(hwnd, kIdChannelList),
                                             GetDlgItem(hwnd, kIdChannelVisible),
                                             GetDlgItem(hwnd, kIdChannelBlackEdit),
                                             GetDlgItem(hwnd, kIdChannelWhiteEdit));
            return true;
        }
        if (id == kIdPseudoColorCombo && code == CBN_SELCHANGE) {
            app.UpdatePseudoColor(GetDlgItem(hwnd, kIdPseudoColorCombo));
            return true;
        }
        if (id == kIdHistogramChannelCombo && code == CBN_SELCHANGE) {
            app.UpdateHistogramChannel(GetDlgItem(hwnd, kIdHistogramChannelCombo));
            return true;
        }
        if (id == kIdHistogramResetAdjust && code == BN_CLICKED) {
            app.ResetImageAdjust(hwnd);
            return true;
        }

        switch (id) {
        case kIdSaveDye:
            app.SaveDyeProfile(GetDlgItem(hwnd, kIdDyeCombo),
                               GetDlgItem(hwnd, kIdDyeNameEdit),
                               GetDlgItem(hwnd, kIdDyeExcitationEdit),
                               GetDlgItem(hwnd, kIdDyeEmissionEdit),
                               GetDlgItem(hwnd, kIdDyeRedEdit),
                               GetDlgItem(hwnd, kIdDyeGreenEdit),
                               GetDlgItem(hwnd, kIdDyeBlueEdit));
            return true;
        case kIdDeleteDye:
            app.DeleteSelectedDye(GetDlgItem(hwnd, kIdDyeCombo),
                                  GetDlgItem(hwnd, kIdDyeNameEdit),
                                  GetDlgItem(hwnd, kIdDyeExcitationEdit),
                                  GetDlgItem(hwnd, kIdDyeEmissionEdit),
                                  GetDlgItem(hwnd, kIdDyeRedEdit),
                                  GetDlgItem(hwnd, kIdDyeGreenEdit),
                                  GetDlgItem(hwnd, kIdDyeBlueEdit));
            return true;
        case kIdAddChannel:
            app.AddCurrentFrameAsChannel(GetDlgItem(hwnd, kIdDyeCombo),
                                         GetDlgItem(hwnd, kIdChannelList),
                                         GetDlgItem(hwnd, kIdFusionPreview));
            return true;
        case kIdFusionPreview:
            app.UpdateFusionPreview(GetDlgItem(hwnd, kIdFusionPreview));
            return true;
        case kIdClearChannels:
            app.ClearFluorescenceChannels(GetDlgItem(hwnd, kIdChannelList),
                                          GetDlgItem(hwnd, kIdFusionPreview));
            return true;
        case kIdChannelVisible:
        case kIdApplyChannel:
            app.ApplySelectedChannelSettings(GetDlgItem(hwnd, kIdChannelList),
                                              GetDlgItem(hwnd, kIdChannelVisible),
                                              GetDlgItem(hwnd, kIdChannelBlackEdit),
                                              GetDlgItem(hwnd, kIdChannelWhiteEdit));
            return true;
        default:
            return false;
        }
    }
};
