#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

enum class UILanguage {
    English = 0,
    Chinese = 1
};

// ──── LocId ──────────────────────────────────────────────────────────────

enum class LocId : std::size_t {
    // Menus - File
    MENU_FILE = 0,
    MF_OPEN_IMAGE,
    MF_EXPORT_IMAGE,
    MF_OPEN_PROJECT,
    MF_SAVE_PROJECT,
    MF_DESIGN_REPORT_TEMPLATE,
    MF_SAVE_REPORT,
    MF_EXIT,

    // Menus - View
    MENU_VIEW,
    MV_FUNCTION_PANEL,
    MV_DOCK_LEFT,
    MV_DOCK_RIGHT,
    MV_FIT_IMAGE,
    MENU_LANGUAGE,
    ML_ENGLISH,
    ML_CHINESE,

    // Menus - Camera
    MENU_CAMERA,
    MC_REFRESH_DEVICES,
    MC_OPEN_CAMERA,
    MC_STOP_CAMERA,
    MC_AUTO_EXPOSURE,
    MC_WHITE_BALANCE,

    // Menus - Processing
    MENU_PROCESSING,
    MP_ADD_TILE,
    MP_START_LIVE_STITCH,
    MP_STOP_LIVE_STITCH,
    MP_STITCH,
    MP_ADD_EDF_FRAME,
    MP_BUILD_EDF,
    MP_CLEAR_PROCESSING,

    // Menus - Measurement
    MENU_MEASUREMENT,
    MM_CALIBRATE,
    MM_CLEAR_CALIBRATION,
    MM_LENGTH,
    MM_ANGLE,
    MM_RECTANGLE_AREA,
    MM_POLYGON_AREA,
    MM_EXPORT_CSV,

    // Panel categories
    PANEL_CAMERA,
    PANEL_IMAGE,
    PANEL_FLUORESCENCE,
    PANEL_PROCESSING,
    PANEL_MEASUREMENT,
    PANEL_PROJECT,
    PANEL_HISTOGRAM,

    // UI general
    UI_FUNCTIONS,
    UI_READY,

    // Button labels
    TB_HIDE_PANEL,
    TB_SHOW_PANEL,
    TB_DOCK_LEFT,
    TB_DOCK_RIGHT,

    // Histogram labels
    HIST_BRIGHTNESS,
    HIST_CONTRAST,
    HIST_GAMMA,
    HIST_WINDOW_LEVEL,
    HIST_WINDOW_WIDTH,
    HIST_RESET,
    HIST_CHANNEL,

    // ── Status bar messages ──

    // General
    STATUS_FUNCTION_CARD,
    STATUS_PANEL_SHOWN,
    STATUS_PANEL_HIDDEN,
    STATUS_REGISTER_CLASS_FAILED,
    STATUS_CREATE_WINDOW_FAILED,

    // Camera / Device
    STATUS_OPENING_CAMERA,
    STATUS_NO_CAMERA_SELECTED,
    STATUS_NO_CAMERA_SELECTED_SHORT,
    STATUS_NO_MUCAM,
    STATUS_DEVICES_FOUND,
    STATUS_DEVICE_SELECTED,
    STATUS_DEVICE_NOT_AVAILABLE,
    STATUS_SDK_DLL_NOT_LOADED,
    STATUS_NO_CAMERA_FOUND,
    STATUS_MULTIPLE_CAMERA_FOUND,
    STATUS_DEVICE_PREFIX,
    STATUS_CAMERA_ERROR,
    STATUS_CAMERA_DISCONNECTED,
    STATUS_PREVIEW_STOPPED,

    // Exposure / Gain / WB
    STATUS_AEX_NEED_OPEN_CAMERA,
    STATUS_AEX_NOT_AVAILABLE,
    STATUS_AEX_APPLIED,
    STATUS_GAIN_NEED_POSITIVE,
    STATUS_GAIN_NEED_OPEN_CAMERA,
    STATUS_GAIN_NOT_AVAILABLE,
    STATUS_GAIN_SET,
    STATUS_WB_NEED_OPEN_CAMERA,
    STATUS_WB_NOT_AVAILABLE,
    STATUS_WB_APPLIED,
    STATUS_EXPOSURE_NEED_POSITIVE,
    STATUS_EXPOSURE_SET,
    STATUS_EXPOSURE_FAILED,
    STATUS_EXPOSURE_PENDING,

    // Panel docking
    STATUS_PANEL_DOCKED_LEFT,
    STATUS_PANEL_DOCKED_RIGHT,
    STATUS_PANEL_RESIZING,
    STATUS_PANEL_WIDTH_ADJUSTED,
    STATUS_PANEL_DRAGGING,
    STATUS_RELEASE_DOCK_LEFT,
    STATUS_RELEASE_DOCK_RIGHT,
    STATUS_IDLE,
    STATUS_PREPARING,
    STATUS_DONE,
    STATUS_FAILED,
    STATUS_NO_FRAME,

    // Calibration
    STATUS_CALIBRATED,
    STATUS_CALIBRATION_CLEARED,
    STATUS_CALIBRATION_NO_OPEN_IMG,
    STATUS_CALIBRATION_CALIBRATE_PROMPT,
    STATUS_CALIBRATION_ACTIVATED,
    STATUS_CALIBRATION_ZOOM_SET,
    STATUS_CALIBRATION_EXPORT_FAILED,
    STATUS_ZOOM_FACTOR_NOT_SET,

    // Objectives
    OBJECTIVE_ENTER_NAME,
    OBJECTIVE_EXISTS,
    OBJECTIVE_ADDED,
    OBJECTIVE_RENAMED,
    OBJECTIVE_KEEP_ONE,
    OBJECTIVE_DELETED,

    // Fusion / EDF
    STATUS_FUSION_ON,
    STATUS_FUSION_OFF,

    // Channel
    STATUS_CHANNEL_RANGE,

    // Live Stitch
    STATUS_LS_ALREADY_RUNNING,
    STATUS_LS_NEED_OPEN_CAMERA,
    STATUS_LS_WAITING_FRAME,
    STATUS_LS_STARTED,
    STATUS_LS_STOPPED,
    STATUS_LS_FAILED_ADJACENCY,
    STATUS_LS_COVERAGE_OVERLAP,
    STATUS_LS_SCALE_ADJUSTMENT,

    // Stitch / EDF results
    STATUS_STITCH_READY,
    STATUS_STITCH_IMAGE_READY,
    STATUS_STITCH_FILE_READY,
    STATUS_STITCH_ERROR,
    STATUS_STITCH_OK,
    STATUS_STITCH_EXPORTED,
    STATUS_STITCH_NO_RESULT,
    STATUS_EDF_READY,
    STATUS_EDF_IMAGE_READY,
    STATUS_EDF_FILE_READY,
    STATUS_EDF_ERROR,
    STATUS_EDF_OK,
    STATUS_EDF_EXPORTED,
    STATUS_EDF_NO_RESULT,
    STATUS_PROCESSING_READY,
    STATUS_PROCESSING_IMAGE_READY,

    // File operations
    STATUS_IMAGE_SAVED,
    STATUS_IMAGE_LOADED,
    STATUS_IMAGE_LOAD_ERROR,
    STATUS_IMAGE_NO_FILE,
    STATUS_DROPPED_IMAGES_LOADED,
    STATUS_OPEN_CAMERA_OR_IMAGE,
    STATUS_NO_ACTIVE_IMAGE,
    STATUS_LINE_PROFILE_NO_IMAGE,
    STATUS_ZOOM_FIT,
    STATUS_ZOOM_100,
    STATUS_UNDO_APPLIED,
    STATUS_REDO_APPLIED,
    STATUS_NO_MORE_UNDO,
    STATUS_NO_MORE_REDO,
    STATUS_FLIP_H,
    STATUS_FLIP_V,
    STATUS_ROTATE_CW,
    STATUS_ROTATE_CCW,
    STATUS_OVERLAY_ON,
    STATUS_OVERLAY_OFF,

    // Project / Report
    STATUS_PROJECT_SAVED,
    STATUS_PROJECT_OPENED,
    STATUS_REPORT_TEMPLATE_SAVED,
    STATUS_REPORT_SAVED,
    STATUS_REPORT_SAVE_ERROR,

    // Processing formatter
    PROC_CLEARED,
    PROC_CLEARED_RUNNING,
    PROC_ALREADY_RUNNING,
    PROC_NO_RETRY,
    PROC_NO_RETRY_STITCH,
    PROC_NO_RETRY_EDF,
    PROC_RETRY_STARTED,
    PROC_RETRY_STARTED_STITCH,
    PROC_RETRY_STARTED_EDF,
    PROC_STARTED,
    PROC_STARTED_STITCH,
    PROC_STARTED_EDF,
    PROC_PROGRESS,
    PROC_CANCELED,
    PROC_FAILED_GENERIC,
    PROC_FAILED_STITCH,
    PROC_FAILED_EDF,
    PROC_READY_STITCH,
    PROC_READY_EDF,
    PROC_READY_GENERIC,
    PROC_KIND_STITCH,
    PROC_KIND_EDF,
    PROC_KIND_GENERIC,

    // Objective selector
    OBJ_SEL_DEFAULT_LABEL,

    // Tile info
    TILE_ADDED,
    TILE_ADD_FAILED,

    // Frame processing
    FRAME_DROPPED,
    FRAME_COPIED,
    FRAME_READY,

    // Resolution
    STATUS_PREVIEW_RESOLUTION,
    STATUS_CAPTURE_RESOLUTION,
    STATUS_DEVICE_INFO,

    // Camera panel actions
    STATUS_PREVIEWING_DEVICE,
    STATUS_PENDING_TELEMETRY,
    STATUS_TELEMETRY_FRAME,
    STATUS_CAM_NOT_READY_OPEN,
    STATUS_CAM_ALREADY_OPEN,
    STATUS_CAM_FLIP_H,
    STATUS_CAM_FLIP_V,
    STATUS_CAM_ROTATION,

    // LiveGainFormatter
    STATUS_LIVE_GAIN_APPLIED,
    STATUS_LIVE_GAIN_FAILED,

    // Failed to open camera (from callback)
    STATUS_FAILED_TO_OPEN_CAMERA,

    // ── Dialog/MessageBox ──
    DIALOG_ENTER_OBJECTIVE,
    DIALOG_EDIT_OBJECTIVE,
    DIALOG_REMOVE_OBJECTIVE_TITLE,
    DIALOG_REMOVE_OBJECTIVE_MSG,
    DIALOG_EXPORT_CSV,
    DIALOG_EXPORT_IMAGE,
    DIALOG_OPEN_IMAGE,
    DIALOG_OPEN_PROJECT,
    DIALOG_SAVE_PROJECT,
    DIALOG_SAVE_REPORT,
    DIALOG_EXPORT_STITCH,
    DIALOG_EXPORT_EDF,

    // Image/export filter descriptions
    FILTER_IMAGE_ALL,
    FILTER_CSV,
    FILTER_PROJECT,
    FILTER_REPORT,

    // Status - File / Export (misc)
    STATUS_IMAGE_NO_FRAME_FIT,
    STATUS_IMAGE_FIT_VIEW,
    STATUS_STITCH_SETTINGS_UPDATED,
    STATUS_NO_FILES_STITCH,
    STATUS_NO_FILES_STITCH_TILES,
    STATUS_IMG_DIR_CANCELED,
    STATUS_IMG_DIR_READ_FAILED,
    STATUS_IMG_FILE_SEL_CANCELED,
    STATUS_STITCH_SAVE_CANCELED,
    STATUS_MEASUREMENTS_CLEARED,
    STATUS_MEASUREMENT_DRAG_EDIT,
    STATUS_MEASUREMENT_POINT_UPDATED,
    STATUS_NO_MEASUREMENTS_EXPORT,
    STATUS_CSV_EXPORT_CANCELED,
    STATUS_NO_IMAGE_EXPORT,
    STATUS_IMAGE_EXPORT_CANCELED,
    STATUS_IMAGE_OPEN_CANCELED,
    STATUS_NO_IMAGE_FILE,
    STATUS_NO_DROPPED_FILES,
    STATUS_NO_DROPPED_FILES_TILES,
    STATUS_NO_IMAGE_FOR_REPORT,
    STATUS_REPORT_SAVE_CANCELED,
    STATUS_REPORT_IMAGE_FOLDER_FAILED,
    STATUS_REPORT_IMAGE_FAILED,
    STATUS_REPORT_TEMPLATE_LOAD_CANCELED,
    STATUS_REPORT_TEMPLATE_EMPTY,
    STATUS_REPORT_TEMPLATE_CLEARED,
    STATUS_REPORT_TEMPLATE_DESIGNER_FAILED,
    STATUS_REPORT_TEMPLATE_DESIGNER_OPENED,
    STATUS_REPORT_TEMPLATE_APPLIED,
    STATUS_PROJECT_SAVE_CANCELED,
    STATUS_PROJECT_OPEN_CANCELED,
    STATUS_REPORT_TEMPLATE_SAVE_CANCELED,

    // Menus - Help
    MENU_HELP,
    MH_ABOUT,

    // About dialog
    ABOUT_TITLE,
    ABOUT_VERSION,
    ABOUT_AUTHOR,
    ABOUT_DESCRIPTION,
    ABOUT_OK,

    // Panel cards
    PANEL_AI,

    // AI Panel
    AI_TASK_TYPE,
    AI_BACKEND,
    AI_INPUT_SIZE,
    AI_MODEL_NAME,
    AI_LABEL_NAME,
    AI_LABELS,
    AI_ADD_LABEL,
    AI_DELETE_LABEL,
    AI_CAPTURE_SAMPLE,
    AI_SAMPLE_COUNT,
    AI_TRAIN_MODEL,
    AI_TRAINING_PROGRESS,
    AI_MODELS,
    AI_LOAD_MODEL,
    AI_SAVE_MODEL,
    AI_DELETE_MODEL,
    AI_RUN_INFERENCE,
    AI_RESULTS,
    AI_SHOW_BOXES,
    AI_SEG_OVERLAY,
    AI_CLEAR_RESULTS,
    AI_STATUS_NO_LABEL_SELECTED,
    AI_STATUS_NO_IMAGE,
    AI_STATUS_NO_TRAINING_SAMPLES,
    AI_STATUS_NO_LABELS,
    AI_STATUS_MODEL_TRAINED,
    AI_STATUS_MODEL_SELECTED,
    AI_STATUS_NO_MODEL,
    AI_STATUS_MODEL_SAVED,
    AI_STATUS_MODEL_SAVE_FAILED,
    AI_STATUS_INFERENCE_COMPLETE,
    AI_STATUS_RESULTS_CLEARED,
    AI_STATUS_SAMPLE_CAPTURED,
    AI_STATUS_SELECT_MODEL,

    // AI Panel — Section Headers
    AI_SECTION_MODEL_CONFIG,
    AI_SECTION_YOLO_ADVANCED,
    AI_SECTION_DATASET,
    AI_SECTION_TRAINING,
    AI_SECTION_MODEL_CENTER,
    AI_SECTION_INFERENCE,

    // AI Panel — YOLO Architecture
    AI_ARCH_TINY_YOLO,
    AI_ARCH_CLASS_CNN,
    AI_ARCH_SEG_NET,

    // AI Panel — YOLO Advanced
    AI_YOLO_ANCHORS,
    AI_YOLO_OBJ_THRESHOLD,
    AI_YOLO_NMS_THRESHOLD,
    AI_YOLO_LEARNING_RATE,
    AI_YOLO_BATCH_SIZE,
    AI_YOLO_INPUT_SIZE_320,
    AI_YOLO_INPUT_SIZE_416,
    AI_YOLO_INPUT_SIZE_512,
    AI_YOLO_INPUT_SIZE_608,

    // AI Panel — Training config
    AI_TRAINING_EPOCHS,
    AI_VALIDATION_SPLIT,
    AI_CONF_THRESHOLD,
    AI_CREATE_MODEL,

    // AI Panel — Dataset
    AI_DATASET_PATH,
    AI_IMPORT_DATASET,
    AI_DATASET_STATUS,
    AI_CLEAR_SAMPLES,
    AI_TRAINING_STATUS,
    AI_TRAINING_LOSS,

    // AI Panel — Model Center
    AI_MODEL_ARCHITECTURE,
    AI_MODEL_LIST,
    AI_DEPLOY,
    AI_EVALUATE,
    AI_MODEL_ACCURACY,
    AI_DEPLOY_STATUS,
    AI_MODEL_VERSION_LIST,

    // AI Panel — Inference
    AI_INFERENCE_TIME,

    // AI Status Messages — YOLO specific
    AI_STATUS_MODEL_CREATED,
    AI_STATUS_TRAINING_STARTED,
    AI_STATUS_TRAINING_EPOCH,
    AI_STATUS_TRAINING_COMPLETE,
    AI_STATUS_TRAINING_SAVED,
    AI_STATUS_TRAINING_SAVE_FAILED,
    AI_STATUS_NO_YOLO_MODEL,
    AI_STATUS_YOLO_MODEL_LOADED,
    AI_STATUS_YOLO_MODEL_LOAD_FAILED,
    AI_STATUS_YOLO_INFERENCE_DONE,
    AI_STATUS_YOLO_DETECTIONS,
    AI_STATUS_YOLO_INFERENCE_TIME,
    AI_STATUS_NO_SAMPLES_FOR_TRAINING,
    AI_STATUS_NEED_LABELS_AND_SAMPLES,
    AI_STATUS_SAMPLES_CLEARED,
    AI_STATUS_DATASET_IMPORTED,
    AI_STATUS_DATASET_IMPORT_FAILED,
    AI_STATUS_MODEL_EVALUATED,

    // AI — Additional status messages
    AI_STATUS_SELECT_MODEL_LOAD,
    AI_STATUS_NO_MODEL_TO_SAVE,
    AI_STATUS_DATASET_PATH_CTRL,
    AI_STATUS_SELECT_VERSION_DEPLOY,
    AI_STATUS_VERSION_DEPLOYED,
    AI_DEPLOY_STATUS_FORMAT,
    AI_DEPLOY_STATUS_NONE,

    // AI — Task type labels
    AI_TASK_CLASSIFICATION,
    AI_TASK_DETECTION,
    AI_TASK_SEGMENTATION,
    AI_TASK_UNKNOWN,

    // AI — Backend labels
    AI_BACKEND_KNN,
    AI_BACKEND_SVM,
    AI_BACKEND_KMEANS,
    AI_BACKEND_UNKNOWN,

    // ── Sentinel ──
    COUNT
};

// ──── Translation Entry ───────────────────────────────────────────────────

struct LocEntry {
    LocId id;
    const wchar_t* en;
    const wchar_t* zh;
};

// ──── Translation Table (compile-time constant) ───────────────────────────

constexpr LocEntry kTranslationTable[] = {
    // ── Menus - File ──
    { LocId::MENU_FILE,               L"File",               L"文件" },
    { LocId::MF_OPEN_IMAGE,           L"Open Image...",      L"打开图像..." },
    { LocId::MF_EXPORT_IMAGE,         L"Export Image...",    L"导出图像..." },
    { LocId::MF_OPEN_PROJECT,         L"Open Project...",    L"打开项目..." },
    { LocId::MF_SAVE_PROJECT,         L"Save Project...",    L"保存项目..." },
    { LocId::MF_DESIGN_REPORT_TEMPLATE, L"Design Report Template...", L"设计报告模板..." },
    { LocId::MF_SAVE_REPORT,          L"Save Report...",     L"保存报告..." },
    { LocId::MF_EXIT,                 L"Exit",               L"退出" },

    // ── Menus - View ──
    { LocId::MENU_VIEW,               L"View",               L"视图" },
    { LocId::MV_FUNCTION_PANEL,       L"Function Panel",     L"功能面板" },
    { LocId::MV_DOCK_LEFT,            L"Dock Left",          L"左侧停靠" },
    { LocId::MV_DOCK_RIGHT,           L"Dock Right",         L"右侧停靠" },
    { LocId::MV_FIT_IMAGE,            L"Fit Image",          L"适应图像" },
    { LocId::MENU_LANGUAGE,           L"Language",           L"语言" },
    { LocId::ML_ENGLISH,              L"English",            L"English" },
    { LocId::ML_CHINESE,              L"中文",               L"中文" },

    // ── Menus - Camera ──
    { LocId::MENU_CAMERA,             L"Camera",             L"相机" },
    { LocId::MC_REFRESH_DEVICES,      L"Refresh Devices",    L"刷新设备" },
    { LocId::MC_OPEN_CAMERA,          L"Open Camera",        L"打开相机" },
    { LocId::MC_STOP_CAMERA,          L"Stop Camera",        L"停止相机" },
    { LocId::MC_AUTO_EXPOSURE,        L"Auto Exposure",      L"自动曝光" },
    { LocId::MC_WHITE_BALANCE,        L"White Balance",      L"白平衡" },

    // ── Menus - Processing ──
    { LocId::MENU_PROCESSING,         L"Processing",         L"处理" },
    { LocId::MP_ADD_TILE,             L"Add Tile",           L"添加瓦片" },
    { LocId::MP_START_LIVE_STITCH,    L"Start Live Stitch",  L"开始实时拼接" },
    { LocId::MP_STOP_LIVE_STITCH,     L"Stop Live Stitch",   L"停止实时拼接" },
    { LocId::MP_STITCH,               L"Stitch",             L"拼接" },
    { LocId::MP_ADD_EDF_FRAME,        L"Add EDF Frame",     L"添加EDF帧" },
    { LocId::MP_BUILD_EDF,            L"Build EDF",         L"构建EDF" },
    { LocId::MP_CLEAR_PROCESSING,     L"Clear Processing",   L"清除处理" },

    // ── Menus - Measurement ──
    { LocId::MENU_MEASUREMENT,        L"Measurement",        L"测量" },
    { LocId::MM_CALIBRATE,            L"Calibrate",          L"标定" },
    { LocId::MM_CLEAR_CALIBRATION,    L"Clear Calibration",  L"清除标定" },
    { LocId::MM_LENGTH,               L"Length",             L"长度" },
    { LocId::MM_ANGLE,                L"Angle",              L"角度" },
    { LocId::MM_RECTANGLE_AREA,       L"Rectangle Area",     L"矩形面积" },
    { LocId::MM_POLYGON_AREA,         L"Polygon Area",       L"多边形面积" },
    { LocId::MM_EXPORT_CSV,           L"Export CSV...",      L"导出CSV..." },

    // ── Panel categories ──
    { LocId::PANEL_CAMERA,            L"Camera",             L"相机" },
    { LocId::PANEL_IMAGE,             L"Image",              L"图像" },
    { LocId::PANEL_FLUORESCENCE,      L"Fluorescence",       L"荧光" },
    { LocId::PANEL_PROCESSING,        L"Processing",         L"处理" },
    { LocId::PANEL_MEASUREMENT,       L"Measurement",        L"测量" },
    { LocId::PANEL_PROJECT,           L"Project",            L"项目" },
    { LocId::PANEL_HISTOGRAM,         L"Histogram",          L"直方图" },

    // ── UI general ──
    { LocId::UI_FUNCTIONS,            L"Functions",          L"功能" },
    { LocId::UI_READY,                L"Ready.",             L"就绪。" },

    // ── Button labels ──
    { LocId::TB_HIDE_PANEL,           L"Hide Panel",         L"隐藏面板" },
    { LocId::TB_SHOW_PANEL,           L"Show Panel",         L"显示面板" },
    { LocId::TB_DOCK_LEFT,            L"Dock Left",          L"左侧停靠" },
    { LocId::TB_DOCK_RIGHT,           L"Dock Right",         L"右侧停靠" },

    // ── Histogram labels ──
    { LocId::HIST_BRIGHTNESS,         L"Brightness",         L"亮度" },
    { LocId::HIST_CONTRAST,           L"Contrast",           L"对比度" },
    { LocId::HIST_GAMMA,              L"Gamma",              L"伽马" },
    { LocId::HIST_WINDOW_LEVEL,       L"Window Level",       L"窗口电平" },
    { LocId::HIST_WINDOW_WIDTH,       L"Window Width",       L"窗口宽度" },
    { LocId::HIST_RESET,              L"Reset",              L"重置" },
    { LocId::HIST_CHANNEL,            L"Channel",            L"通道" },

    // ── Status - General ──
    { LocId::STATUS_FUNCTION_CARD,    L"Function card: ",    L"功能卡：" },
    { LocId::STATUS_PANEL_SHOWN,      L"Function panel shown.",    L"功能面板已显示。" },
    { LocId::STATUS_PANEL_HIDDEN,     L"Function panel hidden.",   L"功能面板已隐藏。" },
    { LocId::STATUS_REGISTER_CLASS_FAILED, L"Failed to register window class.", L"注册窗口类失败。" },
    { LocId::STATUS_CREATE_WINDOW_FAILED,  L"Failed to create main window.", L"创建主窗口失败。" },

    // ── Status - Camera / Device ──
    { LocId::STATUS_OPENING_CAMERA,      L"Opening camera...",                       L"正在打开相机..." },
    { LocId::STATUS_NO_CAMERA_SELECTED,  L"No camera selected. Click Refresh and choose a device.", L"未选择相机。请点击刷新并选择设备。" },
    { LocId::STATUS_NO_CAMERA_SELECTED_SHORT, L"No camera selected.",                L"未选择相机。" },
    { LocId::STATUS_NO_MUCAM,            L"No MUCam camera found.",                  L"未找到MUCam相机。" },
    { LocId::STATUS_DEVICES_FOUND,       L"Found {count} camera(s). Select a device and click Open.", L"找到 {count} 个相机。选择设备并点击打开。" },
    { LocId::STATUS_DEVICE_SELECTED,     L"Selected device {index}. Click Open to preview.", L"已选择设备 {index}。点击打开预览。" },
    { LocId::STATUS_DEVICE_NOT_AVAILABLE,L"Selected camera is no longer available. Refresh the device list.", L"所选相机不再可用。请刷新设备列表。" },
    { LocId::STATUS_SDK_DLL_NOT_LOADED,  L"SDK DLL not loaded",                      L"SDK DLL 未加载" },
    { LocId::STATUS_NO_CAMERA_FOUND,     L"No camera found",                         L"未找到相机" },
    { LocId::STATUS_MULTIPLE_CAMERA_FOUND, L"Found {count} camera(s). Select and open.", L"找到 {count} 个相机。选择并打开。" },
    { LocId::STATUS_DEVICE_PREFIX,       L"Device",                                  L"设备" },
    { LocId::STATUS_CAMERA_ERROR,        L"Camera error.",                           L"相机错误。" },
    { LocId::STATUS_CAMERA_DISCONNECTED, L"Camera disconnected.",                    L"相机已断开连接。" },
    { LocId::STATUS_PREVIEW_STOPPED,     L"Preview stopped.",                        L"预览已停止。" },

    // ── Status - Exposure / Gain / WB ──
    { LocId::STATUS_AEX_NEED_OPEN_CAMERA,  L"Open camera before auto exposure.",           L"请先打开相机再进行自动曝光。" },
    { LocId::STATUS_AEX_NOT_AVAILABLE,     L"Auto exposure is not available for this camera.", L"该相机不支持自动曝光。" },
    { LocId::STATUS_AEX_APPLIED,           L"Auto exposure applied.",                      L"自动曝光已应用。" },
    { LocId::STATUS_GAIN_NEED_POSITIVE,    L"Gain must be a positive number.",             L"增益必须为正数。" },
    { LocId::STATUS_GAIN_NEED_OPEN_CAMERA, L"Open camera before setting gain.",            L"请先打开相机再设置增益。" },
    { LocId::STATUS_GAIN_NOT_AVAILABLE,    L"Gain control is not available for this camera.", L"该相机不支持增益控制。" },
    { LocId::STATUS_GAIN_SET,              L"Gain set.",                                   L"增益已设置。" },
    { LocId::STATUS_WB_NEED_OPEN_CAMERA,   L"Open camera before white balance.",           L"请先打开相机再进行白平衡。" },
    { LocId::STATUS_WB_NOT_AVAILABLE,      L"White balance is not available for this camera.", L"该相机不支持白平衡。" },
    { LocId::STATUS_WB_APPLIED,            L"White balance applied.",                      L"白平衡已应用。" },
    { LocId::STATUS_EXPOSURE_NEED_POSITIVE,L"Exposure must be a positive number.",         L"曝光时间必须为正数。" },
    { LocId::STATUS_EXPOSURE_SET,          L"Exposure set to {value} ms.",                 L"曝光时间已设置为 {value} ms。" },
    { LocId::STATUS_EXPOSURE_FAILED,       L"Failed to set exposure.",                     L"设置曝光失败。" },
    { LocId::STATUS_EXPOSURE_PENDING,      L"Exposure will be applied when the camera opens.", L"曝光将在相机打开后应用。" },

    // ── Status - Panel docking ──
    { LocId::STATUS_PANEL_DOCKED_LEFT,     L"Function panel docked left.",     L"功能面板已停靠到左侧。" },
    { LocId::STATUS_PANEL_DOCKED_RIGHT,    L"Function panel docked right.",    L"功能面板已停靠到右侧。" },
    { LocId::STATUS_PANEL_RESIZING,        L"Resizing function panel.",        L"正在调整功能面板大小。" },
    { LocId::STATUS_PANEL_WIDTH_ADJUSTED,  L"Function panel width adjusted.",  L"功能面板宽度已调整。" },
    { LocId::STATUS_PANEL_DRAGGING,        L"Dragging function panel.",        L"正在拖动功能面板。" },
    { LocId::STATUS_RELEASE_DOCK_LEFT,     L"Release to dock panel left.",     L"释放以停靠到左侧。" },
    { LocId::STATUS_RELEASE_DOCK_RIGHT,    L"Release to dock panel right.",    L"释放以停靠到右侧。" },
    { LocId::STATUS_IDLE,                  L"Idle",                              L"空闲" },
    { LocId::STATUS_PREPARING,             L"Preparing...",                      L"准备中..." },
    { LocId::STATUS_DONE,                  L"Done",                              L"完成" },
    { LocId::STATUS_FAILED,                L"Failed",                            L"失败" },
    { LocId::STATUS_NO_FRAME,              L"No image available. Open a camera or load an image first.", L"无可用图像，请打开摄像头或加载图像。" },

    // ── Status - Calibration ──
    { LocId::STATUS_CALIBRATED,            L"Calibrated: {scale} pix/µm. Magnification: {mag}x.", L"已标定：{scale} pix/µm。倍率：{mag}x。" },
    { LocId::STATUS_CALIBRATION_CLEARED,   L"Calibration cleared.",              L"标定已清除。" },
    { LocId::STATUS_CALIBRATION_NO_OPEN_IMG,L"Open an image before calibration.", L"请先打开图像再进行标定。" },
    { LocId::STATUS_CALIBRATION_CALIBRATE_PROMPT, L"Draw a line for calibration.", L"绘制标定参考线。" },
    { LocId::STATUS_CALIBRATION_ACTIVATED, L"Calibration tool activated. Drag to measure.", L"标定工具已激活。拖动以测量。" },
    { LocId::STATUS_CALIBRATION_ZOOM_SET,  L"Zoom factor set to {zoom}x.",       L"缩放系数已设置为 {zoom}x。" },
    { LocId::STATUS_CALIBRATION_EXPORT_FAILED, L"Failed to export calibration.", L"导出标定失败。" },
    { LocId::STATUS_ZOOM_FACTOR_NOT_SET,   L"Set zoom factor before calibration.", L"请先设置缩放系数再进行标定。" },

    // ── Status - Objectives ──
    { LocId::OBJECTIVE_ENTER_NAME,    L"Enter an objective magnification name.", L"请输入物镜倍率名称。" },
    { LocId::OBJECTIVE_EXISTS,        L"Objective already exists.",               L"物镜已存在。" },
    { LocId::OBJECTIVE_ADDED,         L"Objective added: {name}.",                L"已添加物镜：{name}。" },
    { LocId::OBJECTIVE_RENAMED,       L"Objective renamed: {name}.",              L"物镜已重命名：{name}。" },
    { LocId::OBJECTIVE_KEEP_ONE,      L"Keep at least one objective.",            L"请保留至少一个物镜。" },
    { LocId::OBJECTIVE_DELETED,       L"Objective deleted: {name}.",              L"已删除物镜：{name}。" },

    // ── Status - Fusion / EDF ──
    { LocId::STATUS_FUSION_ON,        L"Fusion preview: On.",                     L"融合预览：开启。" },
    { LocId::STATUS_FUSION_OFF,       L"Fusion preview: Off.",                    L"融合预览：关闭。" },

    // ── Status - Channel ──
    { LocId::STATUS_CHANNEL_RANGE,    L"Channel range must be 0-255.",            L"通道范围必须在 0-255 之间。" },

    // ── Status - Live Stitch ──
    { LocId::STATUS_LS_ALREADY_RUNNING,   L"Live stitch is already running.",                       L"实时拼接已在运行。" },
    { LocId::STATUS_LS_NEED_OPEN_CAMERA,  L"Open camera before live stitch capture.",                L"请先打开相机再进行实时拼接采集。" },
    { LocId::STATUS_LS_WAITING_FRAME,     L"Live stitch waiting for the first camera frame.",        L"实时拼接等待第一帧相机图像。" },
    { LocId::STATUS_LS_STARTED,           L"Live stitch started. Move the stage; frames will be captured automatically.", L"实时拼接已启动。移动载物台，帧将自动采集。" },
    { LocId::STATUS_LS_STOPPED,           L"Live stitch stopped. Tiles: {count}.",                   L"实时拼接已停止。瓦片数：{count}。" },
    { LocId::STATUS_LS_FAILED_ADJACENCY,  L"Live stitch failed due to adjacency issue.",             L"实时拼接因邻接问题失败。" },
    { LocId::STATUS_LS_COVERAGE_OVERLAP,  L"Live stitch: coverage overlap limit reached.",           L"实时拼接：已达到覆盖重叠限制。" },
    { LocId::STATUS_LS_SCALE_ADJUSTMENT,  L"Live stitch triggered scale adjustment.",                L"实时拼接触发了缩放调整。" },

    // ── Status - Stitch results ──
    { LocId::STATUS_STITCH_READY,        L"Stitch ready. Click Stitch to build.",       L"拼接就绪。点击拼接构建。" },
    { LocId::STATUS_STITCH_IMAGE_READY,  L"Stitched image ready.",                      L"拼接图像已就绪。" },
    { LocId::STATUS_STITCH_FILE_READY,   L"Stitched file ready.",                       L"拼接文件已就绪。" },
    { LocId::STATUS_STITCH_ERROR,        L"Stitch error: {msg}",                        L"拼接错误：{msg}" },
    { LocId::STATUS_STITCH_OK,           L"Stitch OK.",                                 L"拼接成功。" },
    { LocId::STATUS_STITCH_EXPORTED,     L"Stitch result exported.",                    L"拼接结果已导出。" },
    { LocId::STATUS_STITCH_NO_RESULT,    L"No stitch result to save.",                  L"没有可保存的拼接结果。" },

    // ── Status - EDF results ──
    { LocId::STATUS_EDF_READY,           L"EDF ready. Click Build EDF.",                L"EDF就绪。点击构建EDF。" },
    { LocId::STATUS_EDF_IMAGE_READY,     L"EDF image ready.",                           L"EDF图像已就绪。" },
    { LocId::STATUS_EDF_FILE_READY,      L"EDF file ready.",                            L"EDF文件已就绪。" },
    { LocId::STATUS_EDF_ERROR,           L"EDF error: {msg}",                           L"EDF错误：{msg}" },
    { LocId::STATUS_EDF_OK,              L"EDF OK.",                                    L"EDF成功。" },
    { LocId::STATUS_EDF_EXPORTED,        L"EDF result exported.",                       L"EDF结果已导出。" },
    { LocId::STATUS_EDF_NO_RESULT,       L"No EDF result to save.",                     L"没有可保存的EDF结果。" },

    // ── Status - Processing generic ──
    { LocId::STATUS_PROCESSING_READY,     L"Processing ready.",                          L"处理就绪。" },
    { LocId::STATUS_PROCESSING_IMAGE_READY, L"Processed image ready.",                   L"处理后的图像已就绪。" },

    // ── Status - File operations ──
    { LocId::STATUS_IMAGE_SAVED,          L"Image saved: {file}",                        L"图像已保存：{file}" },
    { LocId::STATUS_IMAGE_LOADED,         L"Image loaded: {file}",                       L"图像已加载：{file}" },
    { LocId::STATUS_IMAGE_LOAD_ERROR,     L"Image load error: {msg}",                   L"图像加载错误：{msg}" },
    { LocId::STATUS_IMAGE_NO_FILE,        L"Image cannot be loaded: no file.",           L"无法加载图像：无文件。" },
    { LocId::STATUS_DROPPED_IMAGES_LOADED,L"Dropped images loaded from path.",           L"已从路径加载拖放的图像。" },
    { LocId::STATUS_OPEN_CAMERA_OR_IMAGE, L"Open a camera or image first.",              L"请先打开相机或图像。" },
    { LocId::STATUS_NO_ACTIVE_IMAGE,      L"No active image.",                           L"无活动图像。" },
    { LocId::STATUS_LINE_PROFILE_NO_IMAGE,L"No image for line profile.",                 L"无线剖面图的图像。" },
    { LocId::STATUS_ZOOM_FIT,             L"Zoom fit.",                                  L"缩放以适合。" },
    { LocId::STATUS_ZOOM_100,             L"Zoom 100%.",                                 L"缩放100%。" },
    { LocId::STATUS_UNDO_APPLIED,         L"Undo applied.",                              L"已撤销。" },
    { LocId::STATUS_REDO_APPLIED,         L"Redo applied.",                              L"已重做。" },
    { LocId::STATUS_NO_MORE_UNDO,         L"No more undo steps.",                        L"无法继续撤销。" },
    { LocId::STATUS_NO_MORE_REDO,         L"No more redo steps.",                        L"无法继续重做。" },
    { LocId::STATUS_FLIP_H,               L"Image flipped horizontally.",                L"图像已水平翻转。" },
    { LocId::STATUS_FLIP_V,               L"Image flipped vertically.",                  L"图像已垂直翻转。" },
    { LocId::STATUS_ROTATE_CW,            L"Image rotated clockwise.",                   L"图像已顺时针旋转。" },
    { LocId::STATUS_ROTATE_CCW,           L"Image rotated counter-clockwise.",           L"图像已逆时针旋转。" },
    { LocId::STATUS_OVERLAY_ON,           L"Image overlay enabled.",                     L"图像叠加已启用。" },
    { LocId::STATUS_OVERLAY_OFF,          L"Image overlay disabled.",                    L"图像叠加已禁用。" },

    // ── Status - Project / Report ──
    { LocId::STATUS_PROJECT_SAVED,        L"Project saved.",                             L"项目已保存。" },
    { LocId::STATUS_PROJECT_OPENED,       L"Project opened.",                            L"项目已打开。" },
    { LocId::STATUS_REPORT_TEMPLATE_SAVED,L"Report template saved.",                     L"报告模板已保存。" },
    { LocId::STATUS_REPORT_SAVED,         L"Report saved.",                              L"报告已保存。" },
    { LocId::STATUS_REPORT_SAVE_ERROR,    L"Report save error.",                         L"报告保存错误。" },

    // ── Status - Processing formatter ──
    { LocId::PROC_CLEARED,                L"Processing stacks cleared.",                 L"处理栈已清除。" },
    { LocId::PROC_CLEARED_RUNNING,        L"Processing stacks cleared. Running job result will be ignored.", L"处理栈已清除。正在运行的作业结果将被忽略。" },
    { LocId::PROC_ALREADY_RUNNING,        L"Processing job is already running.",         L"处理作业已在运行。" },
    { LocId::PROC_NO_RETRY,               L"No processing job to retry.",                L"没有可重试的处理作业。" },
    { LocId::PROC_NO_RETRY_STITCH,        L"No stitch processing job to retry.",         L"没有可重试的拼接处理作业。" },
    { LocId::PROC_NO_RETRY_EDF,           L"No EDF processing job to retry.",            L"没有可重试的EDF处理作业。" },
    { LocId::PROC_RETRY_STARTED,          L"Retrying processing in background.",         L"正在后台重试处理。" },
    { LocId::PROC_RETRY_STARTED_STITCH,   L"Retrying stitch processing in background.",  L"正在后台重试拼接处理。" },
    { LocId::PROC_RETRY_STARTED_EDF,      L"Retrying EDF processing in background.",     L"正在后台重试EDF处理。" },
    { LocId::PROC_STARTED,                L"Processing started in background.",          L"处理已在后台启动。" },
    { LocId::PROC_STARTED_STITCH,         L"Stitch processing started in background.",   L"拼接处理已在后台启动。" },
    { LocId::PROC_STARTED_EDF,            L"EDF processing started in background.",      L"EDF处理已在后台启动。" },
    { LocId::PROC_PROGRESS,               L"{kind} processing {percent}%.",              L"{kind} 处理 {percent}%。" },
    { LocId::PROC_CANCELED,               L"{kind} processing canceled.",                L"{kind} 处理已取消。" },
    { LocId::PROC_FAILED_GENERIC,         L"Failed to build processed image.",           L"构建处理后图像失败。" },
    { LocId::PROC_FAILED_STITCH,          L"Failed to build stitched image.",            L"构建拼接图像失败。" },
    { LocId::PROC_FAILED_EDF,             L"Failed to build EDF image.",                 L"构建EDF图像失败。" },
    { LocId::PROC_READY_STITCH,           L"Stitched image ready: {w}x{h}. Optimized {n} relation(s).", L"拼接图像已就绪：{w}x{h}。已优化 {n} 个关系。" },
    { LocId::PROC_READY_EDF,              L"EDF image ready: {w}x{h}.",                  L"EDF图像已就绪：{w}x{h}。" },
    { LocId::PROC_READY_GENERIC,          L"Processed image ready: {w}x{h}.",            L"处理后图像已就绪：{w}x{h}。" },
    { LocId::PROC_KIND_STITCH,            L"Stitch",                                     L"拼接" },
    { LocId::PROC_KIND_EDF,               L"EDF",                                        L"EDF" },
    { LocId::PROC_KIND_GENERIC,           L"Processing",                                 L"处理" },

    // ── Status - Objective selector ──
    { LocId::OBJ_SEL_DEFAULT_LABEL,       L"(no objective)",                             L"（无物镜）" },

    // ── Status - Tile info ──
    { LocId::TILE_ADDED,                  L"Tile added: {idx} ({w}x{h})",                L"瓦片已添加：{idx} ({w}x{h})" },
    { LocId::TILE_ADD_FAILED,             L"Tile add failed: {msg}",                     L"添加瓦片失败：{msg}" },

    // ── Status - Frame processing ──
    { LocId::FRAME_DROPPED,               L"Frame dropped.",                             L"帧已丢弃。" },
    { LocId::FRAME_COPIED,                L"Frame copied.",                              L"帧已复制。" },
    { LocId::FRAME_READY,                 L"Frame ready.",                               L"帧已就绪。" },

    // ── Status - Resolution ──
    { LocId::STATUS_PREVIEW_RESOLUTION,   L"Preview resolution: {w}x{h}.",               L"预览分辨率：{w}x{h}。" },
    { LocId::STATUS_CAPTURE_RESOLUTION,   L"Capture resolution: {w}x{h}.",               L"采集分辨率：{w}x{h}。" },
    { LocId::STATUS_DEVICE_INFO,          L"Device {idx} | type {type} | {w}x{h}",       L"设备 {idx} | 类型 {type} | {w}x{h}" },

    // ── Status - Camera panel actions ──
    { LocId::STATUS_PREVIEWING_DEVICE,    L"Previewing device {idx}, camera type {type}, {res}.", L"正在预览设备 {idx}，相机类型 {type}，{res}。" },
    { LocId::STATUS_PENDING_TELEMETRY,    L"Device {idx} | type {type} | {res} | -- fps", L"设备 {idx} | 类型 {type} | {res} | -- fps" },
    { LocId::STATUS_TELEMETRY_FRAME,      L"Device {idx} | type {type} | {res} | {fps} fps | ts {ts}", L"设备 {idx} | 类型 {type} | {res} | {fps} fps | ts {ts}" },
    { LocId::STATUS_CAM_NOT_READY_OPEN,   L"Camera not ready to open.",                  L"相机未就绪，无法打开。" },
    { LocId::STATUS_CAM_ALREADY_OPEN,     L"Camera already open.",                       L"相机已打开。" },
    { LocId::STATUS_CAM_FLIP_H,           L"Camera flip horizontal toggled.",            L"相机水平翻转已切换。" },
    { LocId::STATUS_CAM_FLIP_V,           L"Camera flip vertical toggled.",              L"相机垂直翻转已切换。" },
    { LocId::STATUS_CAM_ROTATION,         L"Camera rotation toggled.",                   L"相机旋转已切换。" },

    // ── Status - Live Gain ──
    { LocId::STATUS_LIVE_GAIN_APPLIED,    L"Live gain applied.",                         L"实时增益已应用。" },
    { LocId::STATUS_LIVE_GAIN_FAILED,     L"Live gain failed.",                          L"实时增益失败。" },

    // ── Status - Failed to open ──
    { LocId::STATUS_FAILED_TO_OPEN_CAMERA, L"Failed to open camera.",                   L"打开相机失败。" },

    // ── Dialog / MessageBox ──
    { LocId::DIALOG_ENTER_OBJECTIVE,      L"Enter Objective",                            L"输入物镜" },
    { LocId::DIALOG_EDIT_OBJECTIVE,       L"Edit Objective",                             L"编辑物镜" },
    { LocId::DIALOG_REMOVE_OBJECTIVE_TITLE, L"Remove Objective",                         L"删除物镜" },
    { LocId::DIALOG_REMOVE_OBJECTIVE_MSG, L"Are you sure you want to remove this objective?", L"确定要删除此物镜吗？" },
    { LocId::DIALOG_EXPORT_CSV,           L"Export CSV",                                 L"导出CSV" },
    { LocId::DIALOG_EXPORT_IMAGE,         L"Export Image",                               L"导出图像" },
    { LocId::DIALOG_OPEN_IMAGE,           L"Open Image",                                 L"打开图像" },
    { LocId::DIALOG_OPEN_PROJECT,         L"Open Project",                               L"打开项目" },
    { LocId::DIALOG_SAVE_PROJECT,         L"Save Project",                               L"保存项目" },
    { LocId::DIALOG_SAVE_REPORT,          L"Save Report",                                L"保存报告" },
    { LocId::DIALOG_EXPORT_STITCH,        L"Export Stitched Image",                      L"导出拼接图像" },
    { LocId::DIALOG_EXPORT_EDF,           L"Export EDF Image",                           L"导出EDF图像" },

    // ── File filter descriptions ──
    { LocId::FILTER_IMAGE_ALL,            L"Image Files (*.bmp;*.jpg;*.png;*.tif)\0*.bmp;*.jpg;*.png;*.tif\0All Files (*.*)\0*.*\0", L"图像文件 (*.bmp;*.jpg;*.png;*.tif)\0*.bmp;*.jpg;*.png;*.tif\0所有文件 (*.*)\0*.*\0" },
    { LocId::FILTER_CSV,                  L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0", L"CSV文件 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0" },
    { LocId::FILTER_PROJECT,              L"Project Files (*.json)\0*.json\0All Files (*.*)\0*.*\0", L"项目文件 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0" },
    { LocId::FILTER_REPORT,               L"Report Files (*.html)\0*.html\0All Files (*.*)\0*.*\0", L"报告文件 (*.html)\0*.html\0所有文件 (*.*)\0*.*\0" },

    // ── Status - File / Export (misc) ──
    { LocId::STATUS_IMAGE_NO_FRAME_FIT,      L"No image frame to fit.",                    L"无可调整的图像帧。" },
    { LocId::STATUS_IMAGE_FIT_VIEW,          L"Image fit to view. ",                       L"图像已适应视图。" },
    { LocId::STATUS_STITCH_SETTINGS_UPDATED, L"Stitch settings updated.",                  L"拼接设置已更新。" },
    { LocId::STATUS_NO_FILES_STITCH,         L"No image files selected for stitching.",    L"未选择拼接图像文件。" },
    { LocId::STATUS_NO_FILES_STITCH_TILES,   L"No selected image files could be added to stitch tiles.", L"所选图像文件无法添加为拼接瓦片。" },
    { LocId::STATUS_IMG_DIR_CANCELED,        L"Image directory selection canceled.",        L"图像目录选择已取消。" },
    { LocId::STATUS_IMG_DIR_READ_FAILED,     L"Failed to read selected image directory.",   L"读取所选图像目录失败。" },
    { LocId::STATUS_IMG_FILE_SEL_CANCELED,   L"Image file selection canceled.",             L"图像文件选择已取消。" },
    { LocId::STATUS_STITCH_SAVE_CANCELED,    L"Stitch result save canceled.",               L"拼接结果保存已取消。" },
    { LocId::STATUS_MEASUREMENTS_CLEARED,    L"Measurements cleared.",                      L"测量结果已清除。" },
    { LocId::STATUS_MEASUREMENT_DRAG_EDIT,   L"Drag to edit measurement point.",            L"拖动以编辑测量点。" },
    { LocId::STATUS_MEASUREMENT_POINT_UPDATED, L"Measurement point updated.",               L"测量点已更新。" },
    { LocId::STATUS_NO_MEASUREMENTS_EXPORT,  L"No measurements to export.",                 L"无测量结果可导出。" },
    { LocId::STATUS_CSV_EXPORT_CANCELED,     L"CSV export canceled.",                       L"CSV导出已取消。" },
    { LocId::STATUS_NO_IMAGE_EXPORT,         L"No image frame to export.",                  L"无图像帧可导出。" },
    { LocId::STATUS_IMAGE_EXPORT_CANCELED,   L"Image export canceled.",                     L"图像导出已取消。" },
    { LocId::STATUS_IMAGE_OPEN_CANCELED,     L"Image open canceled.",                       L"图像打开已取消。" },
    { LocId::STATUS_NO_IMAGE_FILE,           L"No image file selected.",                    L"未选择图像文件。" },
    { LocId::STATUS_NO_DROPPED_FILES,        L"No dropped image files.",                    L"无拖放的图像文件。" },
    { LocId::STATUS_NO_DROPPED_FILES_TILES,  L"No dropped image files could be added to stitch tiles.", L"拖放的图像文件无法添加为拼接瓦片。" },
    { LocId::STATUS_NO_IMAGE_FOR_REPORT,     L"No image frame to report.",                  L"无图像帧用于报告。" },
    { LocId::STATUS_REPORT_SAVE_CANCELED,    L"Report save canceled.",                      L"报告保存已取消。" },
    { LocId::STATUS_REPORT_IMAGE_FOLDER_FAILED, L"Failed to create report image folder.",   L"创建报告图像文件夹失败。" },
    { LocId::STATUS_REPORT_IMAGE_FAILED,     L"Report image failed: {msg}",                 L"报告图像失败：{msg}" },
    { LocId::STATUS_REPORT_TEMPLATE_LOAD_CANCELED, L"Report template load canceled.",       L"报告模板加载已取消。" },
    { LocId::STATUS_REPORT_TEMPLATE_EMPTY,   L"Report template is empty.",                  L"报告模板为空。" },
    { LocId::STATUS_REPORT_TEMPLATE_CLEARED, L"Report template cleared.",                   L"报告模板已清除。" },
    { LocId::STATUS_REPORT_TEMPLATE_DESIGNER_FAILED, L"Failed to open report template designer.", L"打开报告模板设计器失败。" },
    { LocId::STATUS_REPORT_TEMPLATE_DESIGNER_OPENED, L"Report template designer opened.",   L"报告模板设计器已打开。" },
    { LocId::STATUS_REPORT_TEMPLATE_APPLIED, L"Visual report template applied.",            L"可视化报告模板已应用。" },
    { LocId::STATUS_PROJECT_SAVE_CANCELED,   L"Project save canceled.",                     L"项目保存已取消。" },
    { LocId::STATUS_PROJECT_OPEN_CANCELED,   L"Project open canceled.",                     L"项目打开已取消。" },
    { LocId::STATUS_REPORT_TEMPLATE_SAVE_CANCELED, L"Report template save canceled.",       L"报告模板保存已取消。" },

    // ── Menu - Help ──
    { LocId::MENU_HELP,                      L"&Help",                              L"帮助(&H)" },

    // ── Menu - Help/About ──
    { LocId::MH_ABOUT,                       L"&About CameraView...",               L"关于 CameraView(&A)..." },

    // ── About dialog ──
    { LocId::ABOUT_TITLE,                    L"About CameraView",                   L"关于 CameraView" },
    { LocId::ABOUT_VERSION,                  L"Version: 1.0.0",                     L"版本：1.0.0" },
    { LocId::ABOUT_AUTHOR,                   L"Author: liyuan.cn@gmail.com",        L"作者：liyuan.cn@gmail.com" },
    { LocId::ABOUT_DESCRIPTION,              L"A professional microscopy camera control and image processing application.", L"专业的显微相机控制与图像处理应用。" },
    { LocId::ABOUT_OK,                       L"OK",                                 L"确定" },

    // Panel cards
    { LocId::PANEL_AI,                       L"AI",                                 L"智能识别" },

    // AI Panel
    { LocId::AI_TASK_TYPE,                   L"Task Type",                          L"任务类型" },
    { LocId::AI_BACKEND,                     L"Algorithm",                          L"算法" },
    { LocId::AI_INPUT_SIZE,                  L"Input Size",                         L"输入尺寸" },
    { LocId::AI_MODEL_NAME,                  L"Model Name",                         L"模型名称" },
    { LocId::AI_LABEL_NAME,                  L"Label Name",                         L"标签名称" },
    { LocId::AI_LABELS,                      L"Labels",                             L"标签列表" },
    { LocId::AI_ADD_LABEL,                   L"Add Label",                          L"添加标签" },
    { LocId::AI_DELETE_LABEL,                L"Delete",                             L"删除" },
    { LocId::AI_CAPTURE_SAMPLE,              L"Capture Sample",                     L"捕获样本" },
    { LocId::AI_SAMPLE_COUNT,                L"Samples: 0",                         L"样本数：0" },
    { LocId::AI_TRAIN_MODEL,                 L"Train Model",                        L"训练模型" },
    { LocId::AI_TRAINING_PROGRESS,           L"",                                   L"" },
    { LocId::AI_MODELS,                      L"Models",                             L"模型列表" },
    { LocId::AI_LOAD_MODEL,                  L"Load",                               L"加载" },
    { LocId::AI_SAVE_MODEL,                  L"Save Model",                         L"保存模型" },
    { LocId::AI_DELETE_MODEL,                L"Delete",                             L"删除" },
    { LocId::AI_RUN_INFERENCE,               L"Run Inference",                      L"运行推理" },
    { LocId::AI_RESULTS,                     L"Results",                            L"结果" },
    { LocId::AI_SHOW_BOXES,                  L"Show Boxes",                         L"显示检测框" },
    { LocId::AI_SEG_OVERLAY,                 L"Seg. Overlay",                       L"分割叠加" },
    { LocId::AI_CLEAR_RESULTS,               L"Clear",                              L"清除" },
    { LocId::AI_STATUS_NO_LABEL_SELECTED,    L"Select a label first.",              L"请先选择标签。" },
    { LocId::AI_STATUS_NO_IMAGE,             L"No image available.",                L"无可用图像。" },
    { LocId::AI_STATUS_NO_TRAINING_SAMPLES,  L"No training samples.",               L"无训练样本。" },
    { LocId::AI_STATUS_NO_LABELS,            L"No labels defined.",                 L"未定义标签。" },
    { LocId::AI_STATUS_MODEL_TRAINED,        L"Model trained. Accuracy: %.1f%%.",   L"模型已训练。准确率：%.1f%%。" },
    { LocId::AI_STATUS_MODEL_SELECTED,       L"Model '%s' selected.",               L"已选择模型 '%s'。" },
    { LocId::AI_STATUS_NO_MODEL,             L"No trained model available.",        L"无可用训练模型。" },
    { LocId::AI_STATUS_MODEL_SAVED,          L"Model saved to '%s'.",               L"模型已保存到 '%s'。" },
    { LocId::AI_STATUS_MODEL_SAVE_FAILED,    L"Failed to save model.",              L"模型保存失败。" },
    { LocId::AI_STATUS_INFERENCE_COMPLETE,   L"Inference complete.",                L"推理完成。" },
    { LocId::AI_STATUS_RESULTS_CLEARED,      L"AI results cleared.",                L"AI结果已清除。" },
    { LocId::AI_STATUS_SAMPLE_CAPTURED,      L"Sample %zu captured for '%s'.",      L"已为 '%2$s' 捕获第 %1$zu 个样本。" },
    { LocId::AI_STATUS_SELECT_MODEL,         L"Select a model first.",              L"请先选择模型。" },

    // ── AI Panel — Section Headers ──
    { LocId::AI_SECTION_MODEL_CONFIG,        L"▸  Model Configuration",              L"▸  模型配置" },
    { LocId::AI_SECTION_YOLO_ADVANCED,       L"▸  YOLO Advanced",                    L"▸  YOLO 高级参数" },
    { LocId::AI_SECTION_DATASET,             L"▸  Dataset & Labels",                 L"▸  数据集与标签" },
    { LocId::AI_SECTION_TRAINING,            L"▸  Training",                         L"▸  训练" },
    { LocId::AI_SECTION_MODEL_CENTER,        L"▸  Model Center",                     L"▸  模型中心" },
    { LocId::AI_SECTION_INFERENCE,           L"▸  Inference",                        L"▸  推理" },

    // ── AI Panel — YOLO Architecture ──
    { LocId::AI_ARCH_TINY_YOLO,              L"TinyYOLO (Detection)",                L"TinyYOLO（目标检测）" },
    { LocId::AI_ARCH_CLASS_CNN,              L"ClassCNN (Classification)",           L"ClassCNN（图像分类）" },
    { LocId::AI_ARCH_SEG_NET,                L"SegNet (Segmentation)",               L"SegNet（图像分割）" },

    // ── AI Panel — YOLO Advanced ──
    { LocId::AI_YOLO_ANCHORS,               L"Anchors",                              L"锚框数" },
    { LocId::AI_YOLO_OBJ_THRESHOLD,          L"Obj Thresh",                          L"目标阈值" },
    { LocId::AI_YOLO_NMS_THRESHOLD,          L"NMS Thresh",                          L"NMS 阈值" },
    { LocId::AI_YOLO_LEARNING_RATE,          L"Learn Rate",                          L"学习率" },
    { LocId::AI_YOLO_BATCH_SIZE,             L"Batch Size",                          L"批大小" },
    { LocId::AI_YOLO_INPUT_SIZE_320,         L"Input: 320×320",                      L"输入：320×320" },
    { LocId::AI_YOLO_INPUT_SIZE_416,         L"Input: 416×416 (Default)",            L"输入：416×416（默认）" },
    { LocId::AI_YOLO_INPUT_SIZE_512,         L"Input: 512×512",                      L"输入：512×512" },
    { LocId::AI_YOLO_INPUT_SIZE_608,         L"Input: 608×608",                      L"输入：608×608" },

    // ── AI Panel — Training Config ──
    { LocId::AI_TRAINING_EPOCHS,             L"Epochs",                              L"训练轮数" },
    { LocId::AI_VALIDATION_SPLIT,            L"Val Split",                           L"验证比例" },
    { LocId::AI_CONF_THRESHOLD,              L"Conf Thresh",                         L"置信度阈值" },
    { LocId::AI_CREATE_MODEL,                L"Create YOLO Model",                   L"创建 YOLO 模型" },

    // ── AI Panel — Dataset ──
    { LocId::AI_DATASET_PATH,                L"Dataset Path",                        L"数据集路径" },
    { LocId::AI_IMPORT_DATASET,              L"Import Dataset",                      L"导入数据集" },
    { LocId::AI_DATASET_STATUS,              L"No dataset loaded.",                  L"未加载数据集。" },
    { LocId::AI_CLEAR_SAMPLES,               L"Clear Samples",                       L"清除样本" },
    { LocId::AI_TRAINING_STATUS,             L"Training ready.",                     L"准备训练。" },
    { LocId::AI_TRAINING_LOSS,               L"Loss: %.4f",                          L"损失：%.4f" },

    // ── AI Panel — Model Center ──
    { LocId::AI_MODEL_ARCHITECTURE,          L"Architecture",                        L"网络架构" },
    { LocId::AI_MODEL_LIST,                  L"Models",                              L"模型列表" },
    { LocId::AI_DEPLOY,                      L"Deploy",                              L"部署" },
    { LocId::AI_EVALUATE,                    L"Evaluate",                            L"评估" },
    { LocId::AI_MODEL_ACCURACY,              L"Accuracy: N/A",                       L"准确率：N/A" },
    { LocId::AI_DEPLOY_STATUS,               L"Not deployed.",                       L"未部署。" },
    { LocId::AI_MODEL_VERSION_LIST,          L"Version History",                     L"版本历史" },

    // ── AI Panel — Inference ──
    { LocId::AI_INFERENCE_TIME,              L"Inference: -- ms",                    L"推理时间：-- ms" },

    // ── AI Status Messages — YOLO specific ──
    { LocId::AI_STATUS_MODEL_CREATED,        L"YOLO model '%s' created successfully.",           L"YOLO 模型 '%s' 创建成功。" },
    { LocId::AI_STATUS_TRAINING_STARTED,      L"YOLO training started: %zu epochs, lr=%.4f.",     L"YOLO 训练已启动：%zu 轮，学习率=%.4f。" },
    { LocId::AI_STATUS_TRAINING_EPOCH,        L"Epoch %zu/%zu — loss: %.4f",                     L"第 %zu/%zu 轮 — 损失：%.4f" },
    { LocId::AI_STATUS_TRAINING_COMPLETE,     L"YOLO training complete! Final loss: %.4f.",       L"YOLO 训练完成！最终损失：%.4f。" },
    { LocId::AI_STATUS_TRAINING_SAVED,        L"YOLO model saved to '%s'.",                       L"YOLO 模型已保存至 '%s'。" },
    { LocId::AI_STATUS_TRAINING_SAVE_FAILED,  L"Failed to save YOLO model.",                      L"YOLO 模型保存失败。" },
    { LocId::AI_STATUS_NO_YOLO_MODEL,         L"No YOLO model loaded. Create or load a model first.", L"未加载 YOLO 模型。请先创建或加载模型。" },
    { LocId::AI_STATUS_YOLO_MODEL_LOADED,     L"YOLO model '%s' loaded successfully.",            L"YOLO 模型 '%s' 加载成功。" },
    { LocId::AI_STATUS_YOLO_MODEL_LOAD_FAILED,L"Failed to load YOLO model.",                      L"YOLO 模型加载失败。" },
    { LocId::AI_STATUS_YOLO_INFERENCE_DONE,   L"YOLO inference complete: %zu detection(s).",      L"YOLO 推理完成：检测到 %zu 个目标。" },
    { LocId::AI_STATUS_YOLO_INFERENCE_TIME,   L"YOLO inference: %.1f ms",                         L"YOLO 推理时间：%.1f ms" },
    { LocId::AI_STATUS_NO_SAMPLES_FOR_TRAINING, L"No training samples captured.",                  L"未捕获训练样本。" },
    { LocId::AI_STATUS_NEED_LABELS_AND_SAMPLES, L"Define labels and capture samples first.",       L"请先定义标签并捕获样本。" },
    { LocId::AI_STATUS_SAMPLES_CLEARED,       L"Training samples cleared.",                        L"训练样本已清除。" },
    { LocId::AI_STATUS_DATASET_IMPORTED,      L"Dataset imported: %zu samples, %zu labels.",       L"数据集已导入：%zu 个样本，%zu 个标签。" },
    { LocId::AI_STATUS_DATASET_IMPORT_FAILED, L"Failed to import dataset.",                        L"数据集导入失败。" },
    { LocId::AI_STATUS_MODEL_EVALUATED,       L"Model evaluated. Accuracy: %.1f%%.",               L"模型已评估。准确率：%.1f%%。" },

    // ── AI Status — additional messages ──
    { LocId::AI_STATUS_SELECT_MODEL_LOAD,   L"Select a model to load.",                      L"请先选择模型加载。" },
    { LocId::AI_STATUS_NO_MODEL_TO_SAVE,    L"No trained model to save. Train a model first.", L"无训练模型可保存。请先训练模型。" },
    { LocId::AI_STATUS_DATASET_PATH_CTRL,   L"Dataset path control not found.",              L"数据集路径控件未找到。" },
    { LocId::AI_STATUS_SELECT_VERSION_DEPLOY, L"Select a version to deploy.",                L"请选择要部署的版本。" },
    { LocId::AI_STATUS_VERSION_DEPLOYED,    L"Version deployed successfully.",               L"版本已成功部署。" },
    { LocId::AI_DEPLOY_STATUS_FORMAT,       L"Deployed: v%d",                                L"已部署：v%d" },
    { LocId::AI_DEPLOY_STATUS_NONE,         L"Deployed: None",                               L"部署：无" },

    // ── AI Task type labels ──
    { LocId::AI_TASK_CLASSIFICATION,        L"Classification",                               L"分类" },
    { LocId::AI_TASK_DETECTION,             L"Detection",                                    L"检测" },
    { LocId::AI_TASK_SEGMENTATION,          L"Segmentation",                                 L"分割" },
    { LocId::AI_TASK_UNKNOWN,               L"Unknown",                                      L"未知" },

    // ── AI Backend labels ──
    { LocId::AI_BACKEND_KNN,                L"k-NN",                                         L"k-NN" },
    { LocId::AI_BACKEND_SVM,                L"SVM",                                          L"SVM" },
    { LocId::AI_BACKEND_KMEANS,             L"k-Means",                                      L"k-Means" },
    { LocId::AI_BACKEND_UNKNOWN,            L"Unknown",                                      L"未知" },
};

// ──── O(1) Lookup (static array indexed by LocId) ────────────────────────

namespace localization_detail {
    // Direct-indexed array for fast lookup during painting / status updates.
    inline const LocEntry* BuildLocById()
    {
        static LocEntry arr[static_cast<std::size_t>(LocId::COUNT)] = {};
        static bool initialized = false;
        if (!initialized) {
            for (const auto& entry : kTranslationTable) {
                arr[static_cast<std::size_t>(entry.id)] = entry;
            }
            initialized = true;
        }
        return arr;
    }
    inline const LocEntry* kLocById = BuildLocById();
} // namespace localization_detail

// ──── Public API ─────────────────────────────────────────────────────────

inline const wchar_t* GetLocStr(LocId id, UILanguage lang = UILanguage::English)
{
    const auto& entry = localization_detail::kLocById[static_cast<std::size_t>(id)];
    return (lang == UILanguage::Chinese) ? entry.zh : entry.en;
}

// ──── Format-string helper ───────────────────────────────────────────────
// Replaces a single {token} placeholder with a replacement string.
// Multiple tokens replaced left-to-right in order.

inline std::wstring FormatLocStr(LocId id, UILanguage lang = UILanguage::English,
                                  const std::vector<std::pair<std::wstring, std::wstring>>& replacements = {})
{
    std::wstring result = GetLocStr(id, lang);
    for (const auto& [key, value] : replacements) {
        std::size_t pos = result.find(key);
        if (pos != std::wstring::npos) {
            result.replace(pos, key.size(), value);
        }
    }
    return result;
}

// ──── Formatter-friendly forward declarations ─────────────────────────────

class CameraControlStatusFormatter;
class CameraDeviceListFormatter;
class CameraTelemetryFormatter;
class ProcessingStatusFormatter;
