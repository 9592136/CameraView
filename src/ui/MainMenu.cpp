#include "ui/MainMenu.h"

#include "ui/ControlIds.h"
#include "ui/UIConstants.h"
#include "ui/WindowProperties.h"

HMENU CreateMainMenu(UILanguage lang)
{
    HMENU menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    HMENU view_menu = CreatePopupMenu();
    HMENU camera_menu = CreatePopupMenu();
    HMENU processing_menu = CreatePopupMenu();
    HMENU measurement_menu = CreatePopupMenu();
    HMENU language_menu = CreatePopupMenu();

    AppendMenuW(file_menu, MF_STRING, kIdOpenImage, GetLocStr(LocId::MF_OPEN_IMAGE, lang));
    AppendMenuW(file_menu, MF_STRING, kIdExportImage, GetLocStr(LocId::MF_EXPORT_IMAGE, lang));
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdOpenProject, GetLocStr(LocId::MF_OPEN_PROJECT, lang));
    AppendMenuW(file_menu, MF_STRING, kIdSaveProject, GetLocStr(LocId::MF_SAVE_PROJECT, lang));
    AppendMenuW(file_menu, MF_STRING, kIdDesignReportTemplate, GetLocStr(LocId::MF_DESIGN_REPORT_TEMPLATE, lang));
    AppendMenuW(file_menu, MF_STRING, kIdSaveDiagnostics, GetLocStr(LocId::MF_SAVE_REPORT, lang));
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, kIdExit, GetLocStr(LocId::MF_EXIT, lang));

    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdToggleFunctionPanel, GetLocStr(LocId::MV_FUNCTION_PANEL, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING | MF_CHECKED, kIdDockFunctionPanelLeft, GetLocStr(LocId::MV_DOCK_LEFT, lang));
    AppendMenuW(view_menu, MF_STRING, kIdDockFunctionPanelRight, GetLocStr(LocId::MV_DOCK_RIGHT, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING, kIdFitView, GetLocStr(LocId::MV_FIT_IMAGE, lang));
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(language_menu, MF_STRING | (lang == UILanguage::English ? MF_CHECKED : 0),
                kIdLanguageEnglish, GetLocStr(LocId::ML_ENGLISH, lang));
    AppendMenuW(language_menu, MF_STRING | (lang == UILanguage::Chinese ? MF_CHECKED : 0),
                kIdLanguageChinese, GetLocStr(LocId::ML_CHINESE, lang));
    AppendMenuW(view_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language_menu), GetLocStr(LocId::MENU_LANGUAGE, lang));

    AppendMenuW(camera_menu, MF_STRING, kIdRefreshDevices, GetLocStr(LocId::MC_REFRESH_DEVICES, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdOpen, GetLocStr(LocId::MC_OPEN_CAMERA, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdStop, GetLocStr(LocId::MC_STOP_CAMERA, lang));
    AppendMenuW(camera_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(camera_menu, MF_STRING, kIdAutoExposure, GetLocStr(LocId::MC_AUTO_EXPOSURE, lang));
    AppendMenuW(camera_menu, MF_STRING, kIdWhiteBalance, GetLocStr(LocId::MC_WHITE_BALANCE, lang));

    AppendMenuW(processing_menu, MF_STRING, kIdAddStitchTile, GetLocStr(LocId::MP_ADD_TILE, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdStartLiveStitch, GetLocStr(LocId::MP_START_LIVE_STITCH, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdStopLiveStitch, GetLocStr(LocId::MP_STOP_LIVE_STITCH, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdBuildStitch, GetLocStr(LocId::MP_STITCH, lang));
    AppendMenuW(processing_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(processing_menu, MF_STRING, kIdAddEdfFrame, GetLocStr(LocId::MP_ADD_EDF_FRAME, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdBuildEdf, GetLocStr(LocId::MP_BUILD_EDF, lang));
    AppendMenuW(processing_menu, MF_STRING, kIdClearProcessing, GetLocStr(LocId::MP_CLEAR_PROCESSING, lang));

    AppendMenuW(measurement_menu, MF_STRING, kIdCalibrate, GetLocStr(LocId::MM_CALIBRATE, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdClearCalibration, GetLocStr(LocId::MM_CLEAR_CALIBRATION, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdLengthTool, GetLocStr(LocId::MM_LENGTH, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdAngleTool, GetLocStr(LocId::MM_ANGLE, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdRectangleAreaTool, GetLocStr(LocId::MM_RECTANGLE_AREA, lang));
    AppendMenuW(measurement_menu, MF_STRING, kIdPolygonAreaTool, GetLocStr(LocId::MM_POLYGON_AREA, lang));
    AppendMenuW(measurement_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(measurement_menu, MF_STRING, kIdExportCsv, GetLocStr(LocId::MM_EXPORT_CSV, lang));

    // Help menu
    HMENU help_menu = CreatePopupMenu();
    AppendMenuW(help_menu, MF_STRING, kIdAbout, GetLocStr(LocId::MH_ABOUT, lang));

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), GetLocStr(LocId::MENU_FILE, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), GetLocStr(LocId::MENU_VIEW, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(camera_menu), GetLocStr(LocId::MENU_CAMERA, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(processing_menu), GetLocStr(LocId::MENU_PROCESSING, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(measurement_menu), GetLocStr(LocId::MENU_MEASUREMENT, lang));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), GetLocStr(LocId::MENU_HELP, lang));
    return menu;
}

void SyncMainMenu(HWND hwnd)
{
    HMENU menu = GetMenu(hwnd);
    if (!menu) {
        return;
    }
    CheckMenuItem(
        menu,
        kIdToggleFunctionPanel,
        MF_BYCOMMAND | (IsFunctionPanelVisible(hwnd) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu,
        kIdDockFunctionPanelLeft,
        MF_BYCOMMAND | (IsFunctionPanelDockedLeft(hwnd) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        menu,
        kIdDockFunctionPanelRight,
        MF_BYCOMMAND | (IsFunctionPanelDockedLeft(hwnd) ? MF_UNCHECKED : MF_CHECKED));

    const UILanguage lang = GetLanguageProperty(hwnd);
    CheckMenuItem(menu, kIdLanguageEnglish,
        MF_BYCOMMAND | (lang == UILanguage::English ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdLanguageChinese,
        MF_BYCOMMAND | (lang == UILanguage::Chinese ? MF_CHECKED : MF_UNCHECKED));

    DrawMenuBar(hwnd);
}
