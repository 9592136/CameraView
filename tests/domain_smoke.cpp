#include "../src/app/DiagnosticReportActions.h"
#include "../src/app/ExportActions.h"
#include "../src/app/ProjectActions.h"
#include "../src/app/ProjectSessionRestorer.h"
#include "../src/camera/CameraControlStatusFormatter.h"
#include "../src/camera/FrameBuffer.h"
#include "../src/camera/CameraDeviceListFormatter.h"
#include "../src/camera/CameraTelemetryFormatter.h"
#include "../src/domain/CalibrationProfile.h"
#include "../src/domain/Measurement.h"
#include "../src/domain/MeasurementCollection.h"
#include "../src/domain/MeasurementFormatter.h"
#include "../src/domain/MeasurementNameFormatter.h"
#include "../src/imaging/ChannelFusionEngine.h"
#include "../src/imaging/DyeLibrary.h"
#include "../src/imaging/EdfProcessor.h"
#include "../src/imaging/EdfStackListActions.h"
#include "../src/imaging/FluorescenceChannelFactory.h"
#include "../src/imaging/FluorescenceChannelListActions.h"
#include "../src/imaging/FluorescenceChannelSettings.h"
#include "../src/imaging/FluorescenceChannelUpdater.h"
#include "../src/imaging/FluorescenceFormatter.h"
#include "../src/imaging/ImageRegistration.h"
#include "../src/imaging/ImageStitcher.h"
#include "../src/imaging/ImageViewport.h"
#include "../src/imaging/OverlayRenderer.h"
#include "../src/imaging/PreviewDisplayActions.h"
#include "../src/imaging/PreviewFrameComposer.h"
#include "../src/imaging/ProcessingBuildActions.h"
#include "../src/imaging/ProcessingJobExecutor.h"
#include "../src/imaging/ProcessingParameterRules.h"
#include "../src/imaging/ProcessingPanelActions.h"
#include "../src/imaging/ProcessingProgressActions.h"
#include "../src/imaging/ProcessingProgressThrottle.h"
#include "../src/imaging/ProcessingRetryActions.h"
#include "../src/imaging/ProcessingResultActions.h"
#include "../src/imaging/ProcessingJobState.h"
#include "../src/imaging/ProcessingResultFrames.h"
#include "../src/imaging/ProcessingRetryState.h"
#include "../src/imaging/ProcessingStartActions.h"
#include "../src/imaging/ProcessingStatusFormatter.h"
#include "../src/imaging/ProcessingWorkerActions.h"
#include "../src/imaging/PseudoColorMapper.h"
#include "../src/imaging/StitchTileListActions.h"
#include "../src/imaging/StitchTilePlacementPlanner.h"
#include "../src/imaging/ViewTransform.h"
#include "../src/imaging/ViewportInteractionActions.h"
#include "../src/platform/TextInputParser.h"
#include "../src/storage/DiagnosticReportBuilder.h"
#include "../src/storage/ImageExporter.h"
#include "../src/storage/MeasurementCsvExporter.h"
#include "../src/storage/ProjectRepository.h"
#include "../src/storage/ProjectSessionMapper.h"
#include "../src/ui/ControlIds.h"
#include "../src/ui/CameraPanelActions.h"
#include "../src/ui/DyeLibraryActions.h"
#include "../src/ui/DyeProfileFormPresenter.h"
#include "../src/ui/DyeProfileFormParser.h"
#include "../src/ui/FluorescenceDisplayActions.h"
#include "../src/ui/FluorescenceChannelFormPresenter.h"
#include "../src/ui/MeasurementActionApplier.h"
#include "../src/ui/MeasurementDisplayActions.h"
#include "../src/ui/MeasurementEditSession.h"
#include "../src/ui/MeasurementHitTester.h"
#include "../src/ui/MeasurementInteractionActions.h"
#include "../src/ui/MeasurementInteractionState.h"
#include "../src/ui/MeasurementListActions.h"
#include "../src/ui/MeasurementListSelection.h"
#include "../src/ui/MeasurementOverlayModelBuilder.h"
#include "../src/ui/MeasurementToolAvailability.h"
#include "../src/ui/MeasurementToolStartActions.h"
#include "../src/ui/ProcessingBuildInputActions.h"
#include "../src/ui/ProcessingQueueActions.h"
#include "../src/ui/StitchTileDisplayActions.h"
#include "../src/ui/WindowControlDefinitions.h"
#include "../src/ui/WindowControlLayout.h"
#include "../src/ui/WindowLayout.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;

bool Near(double actual, double expected, double tolerance = 0.01)
{
    return std::fabs(actual - expected) <= tolerance;
}

int Fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

class ScopedComInitializer {
public:
    ScopedComInitializer()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized_ = result == S_OK || result == S_FALSE;
        failed_ = FAILED(result) && result != RPC_E_CHANGED_MODE;
    }

    ~ScopedComInitializer()
    {
        if (initialized_) {
            CoUninitialize();
        }
    }

    bool Failed() const
    {
        return failed_;
    }

private:
    bool initialized_ = false;
    bool failed_ = false;
};

ImageFrame MakeSolidImage(int width, int height, unsigned char blue, unsigned char green, unsigned char red)
{
    ImageFrame image;
    image.width = width;
    image.height = height;
    image.stride = (width * 3 + 3) & ~3;
    image.bgr.assign(static_cast<std::size_t>(image.stride) * static_cast<std::size_t>(height), 0);
    for (int y = 0; y < height; ++y) {
        unsigned char* row = image.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.stride);
        for (int x = 0; x < width; ++x) {
            row[x * 3 + 0] = blue;
            row[x * 3 + 1] = green;
            row[x * 3 + 2] = red;
        }
    }
    return image;
}

bool SaveWicTestImage(
    const std::filesystem::path& path,
    REFGUID container_format,
    const ImageFrame& frame)
{
    ScopedComInitializer com;
    if (com.Failed()) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        return false;
    }

    ComPtr<IWICStream> stream;
    result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    }

    ComPtr<IWICBitmapEncoder> encoder;
    if (SUCCEEDED(result)) {
        result = factory->CreateEncoder(container_format, nullptr, &encoder);
    }
    if (SUCCEEDED(result)) {
        result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    }

    ComPtr<IWICBitmapFrameEncode> frame_encoder;
    if (SUCCEEDED(result)) {
        result = encoder->CreateNewFrame(&frame_encoder, nullptr);
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->Initialize(nullptr);
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetSize(frame.width, frame.height);
    }
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetPixelFormat(&pixel_format);
    }
    if (SUCCEEDED(result) && !IsEqualGUID(pixel_format, GUID_WICPixelFormat24bppBGR)) {
        result = E_FAIL;
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->WritePixels(
            frame.height,
            static_cast<UINT>(frame.stride),
            static_cast<UINT>(frame.bgr.size()),
            const_cast<BYTE*>(frame.bgr.data()));
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->Commit();
    }
    if (SUCCEEDED(result)) {
        result = encoder->Commit();
    }

    return SUCCEEDED(result);
}

bool SaveWicGray16TestImage(
    const std::filesystem::path& path,
    REFGUID container_format,
    int width,
    int height,
    const std::vector<std::uint16_t>& gray16)
{
    if (width <= 0 ||
        height <= 0 ||
        gray16.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }

    ScopedComInitializer com;
    if (com.Failed()) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        return false;
    }

    ComPtr<IWICStream> stream;
    result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    }

    ComPtr<IWICBitmapEncoder> encoder;
    if (SUCCEEDED(result)) {
        result = factory->CreateEncoder(container_format, nullptr, &encoder);
    }
    if (SUCCEEDED(result)) {
        result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    }

    ComPtr<IWICBitmapFrameEncode> frame_encoder;
    if (SUCCEEDED(result)) {
        result = encoder->CreateNewFrame(&frame_encoder, nullptr);
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->Initialize(nullptr);
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    }

    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat16bppGray;
    if (SUCCEEDED(result)) {
        result = frame_encoder->SetPixelFormat(&pixel_format);
    }
    if (SUCCEEDED(result) && !IsEqualGUID(pixel_format, GUID_WICPixelFormat16bppGray)) {
        result = E_FAIL;
    }
    if (SUCCEEDED(result)) {
        const UINT stride = static_cast<UINT>(width * static_cast<int>(sizeof(std::uint16_t)));
        result = frame_encoder->WritePixels(
            static_cast<UINT>(height),
            stride,
            static_cast<UINT>(gray16.size() * sizeof(std::uint16_t)),
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(gray16.data())));
    }
    if (SUCCEEDED(result)) {
        result = frame_encoder->Commit();
    }
    if (SUCCEEDED(result)) {
        result = encoder->Commit();
    }

    return SUCCEEDED(result);
}

ImageFrame MakePatternImage(int width, int height)
{
    ImageFrame image;
    image.width = width;
    image.height = height;
    image.stride = (width * 3 + 3) & ~3;
    image.bgr.assign(static_cast<std::size_t>(image.stride) * static_cast<std::size_t>(height), 0);
    for (int y = 0; y < height; ++y) {
        unsigned char* row = image.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.stride);
        for (int x = 0; x < width; ++x) {
            const unsigned char value = static_cast<unsigned char>((x * 17 + y * 29 + (x * y) % 31) & 0xFF);
            row[x * 3 + 0] = value;
            row[x * 3 + 1] = static_cast<unsigned char>((value + x * 3) & 0xFF);
            row[x * 3 + 2] = static_cast<unsigned char>((value + y * 5) & 0xFF);
        }
    }
    return image;
}

ImageFrame CropImage(const ImageFrame& source, int x0, int y0, int width, int height)
{
    ImageFrame image;
    image.width = width;
    image.height = height;
    image.stride = (width * 3 + 3) & ~3;
    image.bgr.assign(static_cast<std::size_t>(image.stride) * static_cast<std::size_t>(height), 0);
    for (int y = 0; y < height; ++y) {
        const unsigned char* src = source.bgr.data() +
            static_cast<std::size_t>(y0 + y) * static_cast<std::size_t>(source.stride) +
            static_cast<std::size_t>(x0) * 3U;
        unsigned char* dst = image.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.stride);
        std::copy(src, src + width * 3, dst);
    }
    return image;
}

ImageFrame AdjustBrightness(ImageFrame image, int delta)
{
    for (int y = 0; y < image.height; ++y) {
        unsigned char* row = image.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.stride);
        for (int x = 0; x < image.width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                row[x * 3 + channel] = static_cast<unsigned char>(
                    std::clamp(static_cast<int>(row[x * 3 + channel]) + delta, 0, 255));
            }
        }
    }
    return image;
}

bool NonDecreasingProgress(const std::vector<int>& values)
{
    return !values.empty() &&
           values.front() == 0 &&
           values.back() == 100 &&
           std::is_sorted(values.begin(), values.end());
}

const WindowControlPlacement* FindPlacement(
    const std::vector<WindowControlPlacement>& placements,
    int control_id)
{
    const auto found = std::find_if(
        placements.begin(),
        placements.end(),
        [&](const WindowControlPlacement& placement) {
            return placement.control_id == control_id;
        });
    return found == placements.end() ? nullptr : &*found;
}

bool RectEquals(const RECT& rect, LONG left, LONG top, LONG right, LONG bottom)
{
    return rect.left == left &&
           rect.top == top &&
           rect.right == right &&
           rect.bottom == bottom;
}

} // namespace

int main()
{
    const RECT wide_client{0, 0, 1200, 800};
    const RECT wide_preview = WindowLayout::PreviewRect(wide_client);
    const RECT wide_panel = WindowLayout::SidePanelRect(wide_client);
    const RECT wide_status = WindowLayout::StatusRect(wide_client);
    const RECT hidden_panel_preview = WindowLayout::PreviewRect(wide_client, false);
    const RECT hidden_panel = WindowLayout::SidePanelRect(wide_client, false);
    const RECT right_docked_preview = WindowLayout::PreviewRect(wide_client, true, false);
    const RECT right_docked_panel = WindowLayout::SidePanelRect(wide_client, true, false);
    if (WindowLayout::ComputeSidePanelWidth(wide_client) != 400 ||
        wide_preview.left != 400 || wide_preview.top != 48 || wide_preview.right != 1200 || wide_preview.bottom != 772 ||
        wide_panel.left != 0 || wide_panel.top != 48 || wide_panel.right != 400 || wide_panel.bottom != 772 ||
        wide_status.left != 0 || wide_status.top != 772 || wide_status.right != 1200 || wide_status.bottom != 800) {
        return Fail("WindowLayout did not produce the expected wide-window regions.");
    }
    if (WindowLayout::ComputeSidePanelWidth(wide_client, false) != 0 ||
        hidden_panel_preview.left != 0 ||
        hidden_panel_preview.top != 48 ||
        hidden_panel_preview.right != 1200 ||
        hidden_panel_preview.bottom != 772 ||
        hidden_panel.left != 0 ||
        hidden_panel.right != 0) {
        return Fail("WindowLayout did not expand the preview when the function panel is hidden.");
    }
    if (right_docked_preview.left != 0 ||
        right_docked_preview.right != 800 ||
        right_docked_preview.top != 48 ||
        right_docked_preview.bottom != 772 ||
        right_docked_panel.left != 800 ||
        right_docked_panel.right != 1200 ||
        right_docked_panel.top != 48 ||
        right_docked_panel.bottom != 772) {
        return Fail("WindowLayout did not dock the function panel on the right.");
    }

    const RECT narrow_client{0, 0, 600, 500};
    const RECT narrow_preview = WindowLayout::PreviewRect(narrow_client);
    const RECT narrow_panel = WindowLayout::SidePanelRect(narrow_client);
    if (WindowLayout::ComputeSidePanelWidth(narrow_client) != 0 ||
        narrow_preview.left != 0 || narrow_preview.top != 48 || narrow_preview.right != 600 || narrow_preview.bottom != 472 ||
        narrow_panel.left != 0 || narrow_panel.right != 0) {
        return Fail("WindowLayout did not collapse the side panel for narrow windows.");
    }

    const std::vector<WindowControlPlacement> wide_control_layout =
        WindowControlLayout::Compute(wide_client);
    const WindowControlPlacement* device_combo = FindPlacement(wide_control_layout, kIdDeviceCombo);
    const WindowControlPlacement* fit_view_button = FindPlacement(wide_control_layout, kIdFitView);
    const WindowControlPlacement* toggle_panel_button =
        FindPlacement(wide_control_layout, kIdToggleFunctionPanel);
    const WindowControlPlacement* toggle_panel_dock_button =
        FindPlacement(wide_control_layout, kIdTogglePanelDock);
    const WindowControlPlacement* camera_card = FindPlacement(wide_control_layout, kIdCameraPanelCard);
    const WindowControlPlacement* image_card = FindPlacement(wide_control_layout, kIdImagePanelCard);
    const WindowControlPlacement* auto_exposure_button = FindPlacement(wide_control_layout, kIdAutoExposure);
    const WindowControlPlacement* pseudo_label = FindPlacement(wide_control_layout, kIdPseudoColorLabel);
    const WindowControlPlacement* results_list = FindPlacement(wide_control_layout, kIdResultsList);
    const std::vector<WindowControlPlacement> image_control_layout =
        WindowControlLayout::Compute(wide_client, 1);
    const WindowControlPlacement* image_pseudo_label =
        FindPlacement(image_control_layout, kIdPseudoColorLabel);
    const std::vector<WindowControlPlacement> measurement_control_layout =
        WindowControlLayout::Compute(wide_client, 4);
    const WindowControlPlacement* measurement_pseudo_label =
        FindPlacement(measurement_control_layout, kIdPseudoColorLabel);
    const WindowControlPlacement* measurement_export_csv =
        FindPlacement(measurement_control_layout, kIdExportCsv);
    const WindowControlPlacement* measurement_results_list =
        FindPlacement(measurement_control_layout, kIdResultsList);
    const std::vector<WindowControlPlacement> processing_control_layout =
        WindowControlLayout::Compute(wide_client, 3);
    const WindowControlPlacement* processing_stitch_tile_label =
        FindPlacement(processing_control_layout, kIdStitchTileLabel);
    const WindowControlPlacement* processing_stitch_tile_list =
        FindPlacement(processing_control_layout, kIdStitchTileList);
    const WindowControlPlacement* processing_delete_stitch_tile =
        FindPlacement(processing_control_layout, kIdDeleteStitchTile);
    const WindowControlPlacement* processing_clear_stitch_tiles =
        FindPlacement(processing_control_layout, kIdClearStitchTiles);
    const std::vector<WindowControlPlacement> right_docked_control_layout =
        WindowControlLayout::Compute(wide_client, 0, 0, true, false);
    const WindowControlPlacement* right_docked_camera_card =
        FindPlacement(right_docked_control_layout, kIdCameraPanelCard);
    const WindowControlPlacement* right_docked_device_combo =
        FindPlacement(right_docked_control_layout, kIdDeviceCombo);
    if (wide_control_layout.size() != 3 + WindowControlLayout::SideControlIds().size()) {
        return Fail("WindowControlLayout did not return the expected number of placements.");
    }
    if (!device_combo || !device_combo->visible || !RectEquals(device_combo->bounds, 12, 149, 388, 289)) {
        return Fail("WindowControlLayout did not compute the expected camera-panel combo placement.");
    }
    if (!fit_view_button || !fit_view_button->visible || !RectEquals(fit_view_button->bounds, 10, 10, 66, 38)) {
        return Fail("WindowControlLayout did not compute the expected fit-view button placement.");
    }
    if (!toggle_panel_button ||
        !toggle_panel_button->visible ||
        !RectEquals(toggle_panel_button->bounds, 74, 10, 170, 38)) {
        return Fail("WindowControlLayout did not compute the expected function-panel toolbar button placement.");
    }
    if (!toggle_panel_dock_button ||
        !toggle_panel_dock_button->visible ||
        !RectEquals(toggle_panel_dock_button->bounds, 178, 10, 266, 38)) {
        return Fail("WindowControlLayout did not compute the expected panel-dock toolbar button placement.");
    }
    if (!camera_card || !camera_card->visible || !RectEquals(camera_card->bounds, 4, 87, 396, 117) ||
        !image_card || !image_card->visible || !RectEquals(image_card->bounds, 4, 365, 396, 395)) {
        return Fail("WindowControlLayout did not compute the expected function header placement.");
    }
    if (!auto_exposure_button ||
        !auto_exposure_button->visible ||
        !RectEquals(auto_exposure_button->bounds, 204, 221, 388, 249)) {
        return Fail("WindowControlLayout did not compute the expected camera parameter placement.");
    }
    if (!right_docked_camera_card ||
        !right_docked_camera_card->visible ||
        !RectEquals(right_docked_camera_card->bounds, 804, 87, 1196, 117) ||
        !right_docked_device_combo ||
        !right_docked_device_combo->visible ||
        !RectEquals(right_docked_device_combo->bounds, 812, 149, 1188, 289)) {
        return Fail("WindowControlLayout did not move function-panel controls when docked right.");
    }
    if (!pseudo_label || pseudo_label->visible || !RectEquals(pseudo_label->bounds, 0, 0, 0, 0)) {
        return Fail("WindowControlLayout should hide image controls in the camera category.");
    }
    if (!image_pseudo_label ||
        !image_pseudo_label->visible ||
        !RectEquals(image_pseudo_label->bounds, 12, 158, 388, 178)) {
        return Fail("WindowControlLayout did not compute the expected pseudo-color label placement.");
    }
    if (!results_list || results_list->visible || !RectEquals(results_list->bounds, 0, 0, 0, 0)) {
        return Fail("WindowControlLayout should hide measurement controls in the camera category.");
    }
    if (!measurement_pseudo_label ||
        measurement_pseudo_label->visible ||
        !measurement_export_csv ||
        !measurement_export_csv->visible ||
        !measurement_results_list ||
        !measurement_results_list->visible ||
        measurement_results_list->bounds.left != 12 ||
        measurement_results_list->bounds.right != 370 ||
        measurement_results_list->bounds.bottom < measurement_results_list->bounds.top) {
        return Fail("WindowControlLayout did not compute the expected measurement category placement.");
    }
    if (!processing_stitch_tile_label ||
        !processing_stitch_tile_label->visible ||
        !processing_stitch_tile_list ||
        !processing_stitch_tile_list->visible ||
        !processing_delete_stitch_tile ||
        !processing_delete_stitch_tile->visible ||
        !processing_clear_stitch_tiles ||
        !processing_clear_stitch_tiles->visible ||
        processing_stitch_tile_list->bounds.bottom <= processing_stitch_tile_list->bounds.top) {
        return Fail("WindowControlLayout did not expose the stitch tile gallery controls.");
    }
    const RECT compact_client{0, 0, 900, 420};
    const int compact_processing_scroll =
        WindowControlLayout::PanelScrollMax(compact_client, 3);
    const std::vector<WindowControlPlacement> compact_processing_layout =
        WindowControlLayout::Compute(compact_client, 3);
    const std::vector<WindowControlPlacement> scrolled_compact_processing_layout =
        WindowControlLayout::Compute(compact_client, 3, std::min(54, compact_processing_scroll));
    const WindowControlPlacement* compact_stitch_search =
        FindPlacement(compact_processing_layout, kIdStitchSearchLabel);
    const WindowControlPlacement* scrolled_compact_stitch_search =
        FindPlacement(scrolled_compact_processing_layout, kIdStitchSearchLabel);
    const WindowControlPlacement* compact_panel_scroll_bar =
        FindPlacement(compact_processing_layout, kIdPanelScrollBar);
    const RECT compact_panel = WindowLayout::SidePanelRect(compact_client);
    if (compact_processing_scroll <= 0 ||
        !compact_stitch_search ||
        !compact_stitch_search->visible ||
        !scrolled_compact_stitch_search ||
        !scrolled_compact_stitch_search->visible ||
        scrolled_compact_stitch_search->bounds.top >= compact_stitch_search->bounds.top) {
        return Fail("WindowControlLayout did not scroll compact side-panel content.");
    }
    if (!compact_panel_scroll_bar ||
        !compact_panel_scroll_bar->visible ||
        compact_panel_scroll_bar->bounds.right != compact_panel.right ||
        compact_panel_scroll_bar->bounds.left > compact_panel_scroll_bar->bounds.right - 16 ||
        compact_panel_scroll_bar->bounds.top <= compact_panel.top ||
        compact_panel_scroll_bar->bounds.bottom != compact_panel.bottom) {
        return Fail("WindowControlLayout did not expose a full-height side-panel scrollbar.");
    }
    const std::vector<std::wstring>& panel_categories = WindowControlLayout::PanelCategoryLabels();
    if (panel_categories.size() != 6 ||
        panel_categories[0] != L"Camera" ||
        panel_categories[1] != L"Image" ||
        panel_categories[4] != L"Measurement" ||
        WindowControlLayout::NormalizePanelCategory(-1) != 0 ||
        WindowControlLayout::NormalizePanelCategory(99) != 0 ||
        WindowControlLayout::NormalizePanelCategory(3) != 3 ||
        WindowControlLayout::PanelCategoryFromCardControl(kIdProcessingPanelCard) != 3 ||
        WindowControlLayout::PanelCategoryFromCardControl(9999) != -1) {
        return Fail("WindowControlLayout did not expose stable panel categories.");
    }

    const std::vector<WindowControlPlacement> narrow_control_layout =
        WindowControlLayout::Compute(narrow_client);
    const WindowControlPlacement* narrow_device_combo = FindPlacement(narrow_control_layout, kIdDeviceCombo);
    const WindowControlPlacement* narrow_fit_view = FindPlacement(narrow_control_layout, kIdFitView);
    const WindowControlPlacement* narrow_pseudo_label = FindPlacement(narrow_control_layout, kIdPseudoColorLabel);
    if (!narrow_fit_view ||
        !narrow_fit_view->visible ||
        !narrow_device_combo ||
        narrow_device_combo->visible ||
        !narrow_pseudo_label ||
        narrow_pseudo_label->visible ||
        !RectEquals(narrow_pseudo_label->bounds, 0, 0, 0, 0)) {
        return Fail("WindowControlLayout did not hide side-panel controls for narrow windows.");
    }
    const std::vector<WindowControlPlacement> hidden_panel_layout =
        WindowControlLayout::Compute(wide_client, 0, 0, false);
    const WindowControlPlacement* hidden_toggle_panel_button =
        FindPlacement(hidden_panel_layout, kIdToggleFunctionPanel);
    const WindowControlPlacement* hidden_toggle_panel_dock_button =
        FindPlacement(hidden_panel_layout, kIdTogglePanelDock);
    const WindowControlPlacement* hidden_camera_card =
        FindPlacement(hidden_panel_layout, kIdCameraPanelCard);
    const WindowControlPlacement* hidden_auto_exposure =
        FindPlacement(hidden_panel_layout, kIdAutoExposure);
    if (WindowControlLayout::PanelScrollMax(wide_client, 3, false) != 0 ||
        WindowControlLayout::PanelScrollPage(wide_client, false) != 0 ||
        !hidden_toggle_panel_button ||
        !hidden_toggle_panel_button->visible ||
        !hidden_toggle_panel_dock_button ||
        !hidden_toggle_panel_dock_button->visible ||
        !hidden_camera_card ||
        hidden_camera_card->visible ||
        !hidden_auto_exposure ||
        hidden_auto_exposure->visible) {
        return Fail("WindowControlLayout did not hide function-panel controls while keeping the toolbar visible.");
    }

    const std::vector<WindowControlDefinition>& control_definitions = WindowControlDefinitions::All();
    std::set<int> control_ids;
    for (const WindowControlDefinition& definition : control_definitions) {
        control_ids.insert(definition.control_id);
    }
    const WindowControlDefinition* open_button_definition = WindowControlDefinitions::Find(kIdOpen);
    const WindowControlDefinition* fit_button_definition = WindowControlDefinitions::Find(kIdFitView);
    const WindowControlDefinition* toggle_panel_definition =
        WindowControlDefinitions::Find(kIdToggleFunctionPanel);
    const WindowControlDefinition* toggle_panel_dock_definition =
        WindowControlDefinitions::Find(kIdTogglePanelDock);
    const WindowControlDefinition* fusion_checkbox_definition = WindowControlDefinitions::Find(kIdFusionPreview);
    const WindowControlDefinition* auto_exposure_definition = WindowControlDefinitions::Find(kIdAutoExposure);
    const WindowControlDefinition* camera_gain_definition = WindowControlDefinitions::Find(kIdCameraGainEdit);
    const WindowControlDefinition* dye_red_definition = WindowControlDefinitions::Find(kIdDyeRedEdit);
    const WindowControlDefinition* panel_scroll_definition = WindowControlDefinitions::Find(kIdPanelScrollBar);
    if (control_definitions.size() != wide_control_layout.size() ||
        control_ids.size() != control_definitions.size() ||
        !open_button_definition ||
        std::wstring(open_button_definition->class_name) != L"BUTTON" ||
        std::wstring(open_button_definition->text) != L"Open" ||
        !fit_button_definition ||
        std::wstring(fit_button_definition->text) != L"Fit" ||
        !toggle_panel_definition ||
        std::wstring(toggle_panel_definition->text) != L"Hide Panel" ||
        !toggle_panel_dock_definition ||
        std::wstring(toggle_panel_dock_definition->text) != L"Dock Right" ||
        !auto_exposure_definition ||
        std::wstring(auto_exposure_definition->text) != L"Auto Exposure" ||
        !camera_gain_definition ||
        std::wstring(camera_gain_definition->class_name) != L"EDIT" ||
        !fusion_checkbox_definition ||
        (fusion_checkbox_definition->style & BS_AUTOCHECKBOX) == 0 ||
        !dye_red_definition ||
        (dye_red_definition->style & ES_NUMBER) == 0 ||
        !panel_scroll_definition ||
        std::wstring(panel_scroll_definition->class_name) != L"SCROLLBAR" ||
        WindowControlDefinitions::Find(9999) != nullptr) {
        return Fail("WindowControlDefinitions did not describe the expected controls.");
    }

    const CameraDeviceListPresentation sdk_unavailable =
        CameraDeviceListFormatter::SdkUnavailable();
    const CameraDeviceListPresentation no_camera =
        CameraDeviceListFormatter::NoCameraFound();
    const std::vector<CameraDevice> device_options{
        CameraDevice{0, 10, L"MUCam A"},
        CameraDevice{1, 11, L""},
    };
    const CameraDeviceListPresentation cameras =
        CameraDeviceListFormatter::Devices(device_options);
    if (sdk_unavailable.selection_enabled ||
        sdk_unavailable.items.size() != 1 ||
        sdk_unavailable.items[0] != L"SDK DLL not loaded" ||
        sdk_unavailable.selected_item != 0 ||
        sdk_unavailable.default_device_index != -1 ||
        no_camera.selection_enabled ||
        no_camera.items.size() != 1 ||
        no_camera.items[0] != L"No camera found" ||
        !cameras.selection_enabled ||
        cameras.default_device_index != 0 ||
        cameras.selected_item != 0 ||
        cameras.items.size() != 2 ||
        cameras.items[0] != L"MUCam A" ||
        cameras.items[1] != L"Device 2" ||
        CameraDeviceListFormatter::SelectionToDeviceIndex(1, cameras.items.size()) != 1 ||
        CameraDeviceListFormatter::SelectionToDeviceIndex(-1, cameras.items.size()) ||
        CameraDeviceListFormatter::SelectionToDeviceIndex(2, cameras.items.size())) {
        return Fail("CameraDeviceListFormatter did not describe expected device combo options.");
    }

    const CameraListRefreshActionResult sdk_unavailable_action =
        CameraPanelActions::SdkUnavailable(L"DLL missing");
    const CameraListRefreshActionResult no_camera_action =
        CameraPanelActions::DevicesEnumerated({}, L"SDK diagnostics");
    const CameraListRefreshActionResult cameras_action =
        CameraPanelActions::DevicesEnumerated(device_options, L"SDK diagnostics");
    const CameraSelectionActionResult no_count_selection =
        CameraPanelActions::SelectDevice(0, 0);
    const CameraSelectionActionResult invalid_selection =
        CameraPanelActions::SelectDevice(2, 4);
    const CameraSelectionActionResult valid_selection =
        CameraPanelActions::SelectDevice(2, 1);
    const CameraExposureParseResult invalid_exposure =
        CameraPanelActions::ParseExposureText(L"bad exposure");
    const CameraExposureParseResult valid_exposure =
        CameraPanelActions::ParseExposureText(L"12.5 ms");
    if (sdk_unavailable_action.succeeded ||
        sdk_unavailable_action.open_enabled ||
        sdk_unavailable_action.combo_enabled ||
        sdk_unavailable_action.preview_telemetry != L"SDK not loaded" ||
        sdk_unavailable_action.status != L"DLL missing" ||
        no_camera_action.succeeded ||
        no_camera_action.presentation.items[0] != L"No camera found" ||
        no_camera_action.preview_telemetry != L"SDK diagnostics" ||
        no_camera_action.status != L"No MUCam camera found." ||
        !cameras_action.succeeded ||
        !cameras_action.open_enabled ||
        !cameras_action.combo_enabled ||
        cameras_action.camera_count != 2 ||
        cameras_action.selected_camera_index != 0 ||
        cameras_action.presentation.items[1] != L"Device 2" ||
        cameras_action.status != L"Found 2 camera(s). Select a device and click Open." ||
        no_count_selection.has_status ||
        invalid_selection.selected_camera_index != -1 ||
        !invalid_selection.has_status ||
        invalid_selection.status != L"No camera selected." ||
        valid_selection.selected_camera_index != 1 ||
        valid_selection.status != L"Selected device 2. Click Open to preview." ||
        invalid_exposure.valid ||
        invalid_exposure.status != L"Exposure must be a positive number." ||
        !valid_exposure.valid ||
        !Near(valid_exposure.requested_exposure_ms, 12.5) ||
        !Near(CameraPanelActions::ClampExposure(20.0f, true, 1.0f, 10.0f), 10.0) ||
        !Near(CameraPanelActions::ClampExposure(20.0f, false, 1.0f, 10.0f), 20.0) ||
        CameraPanelActions::ExposureApplyStatus(true, 7.5f) != L"Exposure set to 7.50 ms." ||
        CameraPanelActions::ExposureApplyStatus(false, 7.5f) != L"Failed to set exposure." ||
        CameraPanelActions::ExposurePendingStatus() != L"Exposure will be applied when the camera opens.") {
        return Fail("CameraPanelActions did not describe camera panel actions.");
    }

    const ImageFrame invalid_tool_frame;
    const MeasurementToolStartResult invalid_calibration_tool =
        MeasurementToolAvailability::ForCalibration(invalid_tool_frame);
    const MeasurementToolStartResult invalid_measurement_tool =
        MeasurementToolAvailability::ForMeasurement(invalid_tool_frame);
    const MeasurementToolStartResult valid_measurement_tool =
        MeasurementToolAvailability::ForMeasurement(MakeSolidImage(1, 1, 10, 20, 30));
    if (invalid_calibration_tool.can_start ||
        invalid_calibration_tool.status != L"Open a camera frame before calibration." ||
        invalid_measurement_tool.can_start ||
        invalid_measurement_tool.status != L"Open a camera frame before measuring." ||
        !valid_measurement_tool.can_start ||
        !valid_measurement_tool.status.empty()) {
        return Fail("MeasurementToolAvailability did not gate measurement tool startup by frame availability.");
    }

    const MeasurementToolStartResult valid_calibration_tool =
        MeasurementToolAvailability::ForCalibration(MakeSolidImage(1, 1, 10, 20, 30));
    MeasurementInteractionState tool_start_interaction;
    const MeasurementToolStartActionResult invalid_length_start =
        MeasurementToolStartActions::BeginCalibration(
            tool_start_interaction,
            valid_calibration_tool,
            false,
            0.0,
            MeasurementUnit::Micrometers);
    if (invalid_length_start.started ||
        invalid_length_start.kind != MeasurementToolStartActionKind::Calibration ||
        invalid_length_start.status != MeasurementToolStartActionStatus::InvalidCalibrationLength ||
        invalid_length_start.message != L"Enter a positive calibration length." ||
        !tool_start_interaction.IsIdle()) {
        return Fail("MeasurementToolStartActions did not reject an invalid calibration length.");
    }
    const MeasurementToolStartActionResult unavailable_calibration_start =
        MeasurementToolStartActions::BeginCalibration(
            tool_start_interaction,
            invalid_calibration_tool,
            true,
            25.0,
            MeasurementUnit::Micrometers);
    if (unavailable_calibration_start.started ||
        unavailable_calibration_start.status != MeasurementToolStartActionStatus::FrameUnavailable ||
        unavailable_calibration_start.message != L"Open a camera frame before calibration." ||
        !tool_start_interaction.IsIdle()) {
        return Fail("MeasurementToolStartActions did not reject calibration without a frame.");
    }
    const MeasurementToolStartActionResult calibration_tool_start =
        MeasurementToolStartActions::BeginCalibration(
            tool_start_interaction,
            valid_calibration_tool,
            true,
            250.0,
            MeasurementUnit::Millimeters);
    if (!calibration_tool_start.started ||
        calibration_tool_start.status != MeasurementToolStartActionStatus::Started ||
        calibration_tool_start.kind != MeasurementToolStartActionKind::Calibration ||
        calibration_tool_start.message != L"Calibration: click the first point." ||
        !Near(calibration_tool_start.calibration_length, 250.0) ||
        calibration_tool_start.calibration_unit != MeasurementUnit::Millimeters ||
        tool_start_interaction.Mode() != MeasurementInteractionMode::CalibrationFirstPoint) {
        return Fail("MeasurementToolStartActions did not start calibration and return pending calibration settings.");
    }
    tool_start_interaction.AddPoint(ImagePoint{0.0, 0.0});
    MeasurementInteractionAction calibration_tool_action =
        tool_start_interaction.AddPoint(ImagePoint{100.0, 0.0});
    if (calibration_tool_action.kind != MeasurementInteractionActionKind::CalibrationCompleted ||
        !Near(calibration_tool_action.calibration_length, 250.0) ||
        calibration_tool_action.calibration_unit != MeasurementUnit::Millimeters) {
        return Fail("MeasurementInteractionState did not carry calibration settings into the completed action.");
    }

    MeasurementInteractionState unavailable_measurement_start_interaction;
    const MeasurementToolStartActionResult unavailable_length_start =
        MeasurementToolStartActions::BeginLength(
            unavailable_measurement_start_interaction,
            invalid_measurement_tool);
    if (unavailable_length_start.started ||
        unavailable_length_start.kind != MeasurementToolStartActionKind::Length ||
        unavailable_length_start.status != MeasurementToolStartActionStatus::FrameUnavailable ||
        unavailable_length_start.message != L"Open a camera frame before measuring." ||
        !unavailable_measurement_start_interaction.IsIdle()) {
        return Fail("MeasurementToolStartActions did not reject measurement without a frame.");
    }

    MeasurementInteractionState measurement_start_interaction;
    const MeasurementToolStartActionResult length_start =
        MeasurementToolStartActions::BeginLength(measurement_start_interaction, valid_measurement_tool);
    const MeasurementToolStartActionResult angle_start =
        MeasurementToolStartActions::BeginAngle(measurement_start_interaction, valid_measurement_tool);
    const MeasurementToolStartActionResult rectangle_start =
        MeasurementToolStartActions::BeginRectangleArea(measurement_start_interaction, valid_measurement_tool);
    const MeasurementToolStartActionResult polygon_start =
        MeasurementToolStartActions::BeginPolygonArea(measurement_start_interaction, valid_measurement_tool);
    if (!length_start.started ||
        length_start.message != L"Length: click the first point." ||
        !angle_start.started ||
        angle_start.message != L"Angle: click the first ray point." ||
        !rectangle_start.started ||
        rectangle_start.message != L"Area: click the first rectangle corner." ||
        !polygon_start.started ||
        polygon_start.message != L"Polygon: click vertices, then Finish Poly." ||
        measurement_start_interaction.Mode() != MeasurementInteractionMode::PolygonAreaCollecting) {
        return Fail("MeasurementToolStartActions did not start measurement tools.");
    }

    MeasurementInteractionState interaction;
    if (!interaction.IsIdle() ||
        interaction.BeginLength() != L"Length: click the first point.") {
        return Fail("MeasurementInteractionState did not start length measurement.");
    }
    MeasurementInteractionAction interaction_action = interaction.AddPoint(ImagePoint{1.0, 2.0});
    MeasurementInteractionPending pending_overlay = interaction.PendingOverlay();
    if (interaction_action.kind != MeasurementInteractionActionKind::Prompt ||
        interaction_action.status != L"Length: click the second point." ||
        pending_overlay.kind != MeasurementInteractionPendingKind::Point ||
        !Near(pending_overlay.first.x, 1.0) ||
        !Near(pending_overlay.first.y, 2.0)) {
        return Fail("MeasurementInteractionState did not advance to a pending second length point.");
    }
    interaction_action = interaction.AddPoint(ImagePoint{3.0, 4.0});
    if (interaction_action.kind != MeasurementInteractionActionKind::LengthCompleted ||
        !interaction.IsIdle() ||
        !Near(interaction_action.first.x, 1.0) ||
        !Near(interaction_action.second.y, 4.0)) {
        return Fail("MeasurementInteractionState did not complete length measurement.");
    }

    MeasurementInteractionState point_action_interaction;
    MeasurementCollection point_action_measurements;
    CalibrationProfile point_action_calibration = CalibrationProfile::Uncalibrated();
    MeasurementInteractionActionResult point_action_result =
        MeasurementInteractionActions::AddPoint(
            point_action_interaction,
            point_action_measurements,
            point_action_calibration,
            true,
            true,
            ImagePoint{1.0, 1.0},
            100.0,
            MeasurementUnit::Micrometers,
            MeasurementUnit::Pixels);
    if (point_action_result.handled ||
        point_action_result.status != MeasurementInteractionActionStatus::Ignored) {
        return Fail("MeasurementInteractionActions should ignore clicks while idle.");
    }
    point_action_interaction.BeginLength();
    point_action_result = MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        false,
        true,
        ImagePoint{1.0, 1.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (point_action_result.handled ||
        point_action_result.status != MeasurementInteractionActionStatus::OutsidePreview ||
        point_action_interaction.Mode() != MeasurementInteractionMode::LengthFirstPoint) {
        return Fail("MeasurementInteractionActions should ignore clicks outside the preview.");
    }
    point_action_result = MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        false,
        std::nullopt,
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (!point_action_result.handled ||
        point_action_result.status != MeasurementInteractionActionStatus::NoFrame ||
        point_action_result.message != L"No frame available." ||
        point_action_interaction.Mode() != MeasurementInteractionMode::LengthFirstPoint) {
        return Fail("MeasurementInteractionActions did not report missing frames.");
    }
    point_action_result = MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        std::nullopt,
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (!point_action_result.handled ||
        point_action_result.status != MeasurementInteractionActionStatus::NoImagePoint ||
        point_action_result.message != L"Click inside the image area." ||
        point_action_interaction.Mode() != MeasurementInteractionMode::LengthFirstPoint) {
        return Fail("MeasurementInteractionActions did not report failed image coordinate conversion.");
    }
    point_action_result = MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        ImagePoint{0.0, 0.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (!point_action_result.handled ||
        point_action_result.status != MeasurementInteractionActionStatus::Applied ||
        !point_action_result.preview_changed ||
        point_action_result.measurement_list_changed ||
        point_action_result.message != L"Length: click the second point." ||
        point_action_interaction.Mode() != MeasurementInteractionMode::LengthSecondPoint) {
        return Fail("MeasurementInteractionActions did not apply the first length point.");
    }
    point_action_result = MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        ImagePoint{3.0, 4.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (!point_action_result.handled ||
        !point_action_result.measurement_list_changed ||
        !point_action_result.preview_changed ||
        point_action_measurements.LengthCount() != 1 ||
        !point_action_interaction.IsIdle() ||
        point_action_result.message.empty()) {
        return Fail("MeasurementInteractionActions did not complete a length measurement.");
    }

    MeasurementInteractionState calibration_retry_interaction;
    calibration_retry_interaction.BeginCalibration();
    MeasurementInteractionAction calibration_retry_action =
        calibration_retry_interaction.AddPoint(ImagePoint{5.0, 6.0});
    calibration_retry_action = calibration_retry_interaction.AddPoint(ImagePoint{5.0, 6.0});
    if (calibration_retry_action.kind != MeasurementInteractionActionKind::Prompt ||
        calibration_retry_action.status != L"Calibration: click a different second point." ||
        calibration_retry_interaction.Mode() != MeasurementInteractionMode::CalibrationSecondPoint) {
        return Fail("MeasurementInteractionState should keep waiting after a repeated calibration point.");
    }
    calibration_retry_action = calibration_retry_interaction.AddPoint(ImagePoint{9.0, 6.0});
    if (calibration_retry_action.kind != MeasurementInteractionActionKind::CalibrationCompleted ||
        !calibration_retry_interaction.IsIdle()) {
        return Fail("MeasurementInteractionState did not complete calibration after a different second point.");
    }

    MeasurementInteractionState calibration_action_interaction;
    MeasurementCollection calibration_action_measurements;
    CalibrationProfile calibration_action_profile = CalibrationProfile::Uncalibrated();
    calibration_action_interaction.BeginCalibration(100.0, MeasurementUnit::Micrometers);
    MeasurementInteractionActionResult calibration_action_result =
        MeasurementInteractionActions::AddPoint(
            calibration_action_interaction,
            calibration_action_measurements,
            calibration_action_profile,
            true,
            true,
            ImagePoint{5.0, 6.0},
            0.0,
            MeasurementUnit::Pixels,
            MeasurementUnit::Pixels);
    calibration_action_result = MeasurementInteractionActions::AddPoint(
        calibration_action_interaction,
        calibration_action_measurements,
        calibration_action_profile,
        true,
        true,
        ImagePoint{5.5, 6.0},
        0.0,
        MeasurementUnit::Pixels,
        MeasurementUnit::Pixels);
    if (!calibration_action_result.handled ||
        calibration_action_result.measurement_list_changed ||
        !calibration_action_result.preview_changed ||
        calibration_action_result.message != L"Calibration: click a second point farther away." ||
        calibration_action_interaction.Mode() != MeasurementInteractionMode::CalibrationSecondPoint ||
        calibration_action_profile.IsCalibrated()) {
        return Fail("MeasurementInteractionActions should keep calibration active when the second point is too close.");
    }
    calibration_action_result = MeasurementInteractionActions::AddPoint(
        calibration_action_interaction,
        calibration_action_measurements,
        calibration_action_profile,
        true,
        true,
        ImagePoint{15.0, 6.0},
        0.0,
        MeasurementUnit::Pixels,
        MeasurementUnit::Pixels);
    if (!calibration_action_result.handled ||
        !calibration_action_result.measurement_list_changed ||
        !calibration_action_result.preview_changed ||
        !calibration_action_interaction.IsIdle() ||
        !calibration_action_profile.IsCalibrated()) {
        return Fail("MeasurementInteractionActions did not complete calibration after a far enough second point.");
    }
    MeasurementInteractionActionResult finish_polygon_result =
        MeasurementInteractionActions::FinishPolygon(
            point_action_interaction,
            point_action_measurements,
            point_action_calibration,
            100.0,
            MeasurementUnit::Micrometers,
            MeasurementUnit::Pixels);
    if (!finish_polygon_result.handled ||
        finish_polygon_result.measurement_list_changed ||
        finish_polygon_result.preview_changed ||
        finish_polygon_result.message != L"Start Poly Area before finishing a polygon.") {
        return Fail("MeasurementInteractionActions did not report inactive polygon completion.");
    }
    point_action_interaction.BeginPolygonArea();
    MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        ImagePoint{0.0, 0.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        ImagePoint{4.0, 0.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    MeasurementInteractionActions::AddPoint(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        true,
        true,
        ImagePoint{0.0, 3.0},
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    finish_polygon_result = MeasurementInteractionActions::FinishPolygon(
        point_action_interaction,
        point_action_measurements,
        point_action_calibration,
        100.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (!finish_polygon_result.handled ||
        !finish_polygon_result.measurement_list_changed ||
        !finish_polygon_result.preview_changed ||
        point_action_measurements.PolygonCount() != 1 ||
        !point_action_interaction.IsIdle() ||
        finish_polygon_result.message.empty()) {
        return Fail("MeasurementInteractionActions did not complete a polygon measurement.");
    }

    MeasurementCollection applied_measurements;
    CalibrationProfile applied_calibration = CalibrationProfile::Uncalibrated();
    MeasurementInteractionAction prompt_action;
    prompt_action.kind = MeasurementInteractionActionKind::Prompt;
    prompt_action.status = L"Pick another point.";
    MeasurementActionApplyResult applied_result = MeasurementActionApplier::Apply(
        prompt_action,
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Pixels);
    if (applied_result.status != L"Pick another point." ||
        applied_result.measurement_list_changed ||
        applied_result.preview_changed ||
        applied_measurements.Count() != 0) {
        return Fail("MeasurementActionApplier did not preserve prompt status.");
    }
    MeasurementInteractionAction calibration_action;
    calibration_action.kind = MeasurementInteractionActionKind::CalibrationCompleted;
    calibration_action.first = ImagePoint{0.0, 0.0};
    calibration_action.second = ImagePoint{10.0, 0.0};
    calibration_action.calibration_length = 50.0;
    calibration_action.calibration_unit = MeasurementUnit::Micrometers;
    applied_result = MeasurementActionApplier::Apply(
        calibration_action,
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Pixels,
        MeasurementUnit::Micrometers);
    if (!applied_result.measurement_list_changed ||
        !applied_result.preview_changed ||
        !applied_calibration.IsCalibrated() ||
        !Near(applied_calibration.MicronsPerPixel(), 5.0) ||
        applied_result.status.find(L"5.0000 um/px") == std::wstring::npos) {
        return Fail("MeasurementActionApplier did not apply calibration.");
    }
    MeasurementInteractionAction apply_length;
    apply_length.kind = MeasurementInteractionActionKind::LengthCompleted;
    apply_length.first = ImagePoint{0.0, 0.0};
    apply_length.second = ImagePoint{3.0, 4.0};
    applied_result = MeasurementActionApplier::Apply(
        apply_length,
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Micrometers);
    if (!applied_result.measurement_list_changed ||
        !applied_result.preview_changed ||
        applied_measurements.LengthCount() != 1 ||
        applied_measurements.Lengths()[0].Name() != L"Length 1" ||
        applied_result.status.find(L"25.00 um") == std::wstring::npos) {
        return Fail("MeasurementActionApplier did not add a length measurement.");
    }
    MeasurementInteractionAction apply_angle;
    apply_angle.kind = MeasurementInteractionActionKind::AngleCompleted;
    apply_angle.first = ImagePoint{1.0, 0.0};
    apply_angle.second = ImagePoint{0.0, 0.0};
    apply_angle.third = ImagePoint{0.0, 1.0};
    applied_result = MeasurementActionApplier::Apply(
        apply_angle,
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Micrometers);
    if (!applied_result.measurement_list_changed ||
        applied_measurements.AngleCount() != 1 ||
        applied_result.status.find(L"90.00 deg") == std::wstring::npos) {
        return Fail("MeasurementActionApplier did not add an angle measurement.");
    }
    MeasurementInteractionAction apply_rectangle;
    apply_rectangle.kind = MeasurementInteractionActionKind::RectangleCompleted;
    apply_rectangle.first = ImagePoint{0.0, 0.0};
    apply_rectangle.second = ImagePoint{2.0, 3.0};
    applied_result = MeasurementActionApplier::Apply(
        apply_rectangle,
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Micrometers);
    if (!applied_result.measurement_list_changed ||
        applied_measurements.RectangleCount() != 1 ||
        applied_result.status.find(L"150.00 um^2") == std::wstring::npos) {
        return Fail("MeasurementActionApplier did not add a rectangle measurement.");
    }
    MeasurementInteractionAction apply_polygon;
    apply_polygon.kind = MeasurementInteractionActionKind::PolygonCompleted;
    apply_polygon.polygon_points = {
        ImagePoint{0.0, 0.0},
        ImagePoint{2.0, 0.0},
        ImagePoint{2.0, 3.0}
    };
    applied_result = MeasurementActionApplier::Apply(
        std::move(apply_polygon),
        applied_measurements,
        applied_calibration,
        0.0,
        MeasurementUnit::Micrometers,
        MeasurementUnit::Micrometers);
    if (!applied_result.measurement_list_changed ||
        applied_measurements.PolygonCount() != 1 ||
        applied_measurements.Polygons()[0].Name() != L"Polygon 1" ||
        applied_result.status.find(L"75.00 um^2") == std::wstring::npos) {
        return Fail("MeasurementActionApplier did not add a polygon measurement.");
    }

    if (interaction.BeginAngle() != L"Angle: click the first ray point.") {
        return Fail("MeasurementInteractionState did not start angle measurement.");
    }
    interaction.AddPoint(ImagePoint{1.0, 0.0});
    interaction.AddPoint(ImagePoint{0.0, 0.0});
    pending_overlay = interaction.PendingOverlay();
    interaction_action = interaction.AddPoint(ImagePoint{0.0, 1.0});
    if (pending_overlay.kind != MeasurementInteractionPendingKind::Angle ||
        interaction_action.kind != MeasurementInteractionActionKind::AngleCompleted ||
        !Near(interaction_action.second.x, 0.0) ||
        !Near(interaction_action.third.y, 1.0)) {
        return Fail("MeasurementInteractionState did not complete angle measurement.");
    }

    if (interaction.BeginPolygonArea() != L"Polygon: click vertices, then Finish Poly.") {
        return Fail("MeasurementInteractionState did not start polygon measurement.");
    }
    interaction.AddPoint(ImagePoint{0.0, 0.0});
    interaction.AddPoint(ImagePoint{5.0, 0.0});
    interaction_action = interaction.FinishPolygon();
    if (interaction_action.kind != MeasurementInteractionActionKind::PolygonTooFew ||
        interaction_action.status != L"Polygon needs at least three vertices.") {
        return Fail("MeasurementInteractionState did not enforce the polygon point count.");
    }
    interaction_action = interaction.AddPoint(ImagePoint{5.0, 5.0});
    pending_overlay = interaction.PendingOverlay();
    if (interaction_action.kind != MeasurementInteractionActionKind::PolygonPointAdded ||
        interaction_action.status != L"Polygon: 3 vertices. Click more or Finish Poly." ||
        pending_overlay.kind != MeasurementInteractionPendingKind::Polygon ||
        !pending_overlay.polygon_points ||
        pending_overlay.polygon_points->size() != 3) {
        return Fail("MeasurementInteractionState did not expose pending polygon points.");
    }

    MeasurementCollection overlay_measurements;
    overlay_measurements.AddLength(L"Overlay Length", ImagePoint{0.0, 0.0}, ImagePoint{3.0, 4.0});
    overlay_measurements.AddRectangleArea(L"Overlay Rect", ImagePoint{1.0, 2.0}, ImagePoint{6.0, 8.0});
    const CalibrationProfile overlay_calibration = CalibrationProfile::FromMicronsPerPixel(0.5);
    MeasurementOverlayModel overlay_model = MeasurementOverlayModelBuilder::Build(
        overlay_measurements,
        overlay_calibration,
        MeasurementUnit::Micrometers,
        pending_overlay);
    if (overlay_model.calibration != &overlay_calibration ||
        overlay_model.display_unit != MeasurementUnit::Micrometers ||
        overlay_model.lengths != &overlay_measurements.Lengths() ||
        overlay_model.angles != &overlay_measurements.Angles() ||
        overlay_model.rectangles != &overlay_measurements.Rectangles() ||
        overlay_model.polygons != &overlay_measurements.Polygons() ||
        overlay_model.pending.kind != OverlayPendingKind::Polygon ||
        overlay_model.pending.polygon_points != pending_overlay.polygon_points) {
        return Fail("MeasurementOverlayModelBuilder did not expose measurement collections and pending polygon state.");
    }
    MeasurementInteractionPending angle_pending;
    angle_pending.kind = MeasurementInteractionPendingKind::Angle;
    angle_pending.first = ImagePoint{2.0, 3.0};
    angle_pending.second = ImagePoint{4.0, 5.0};
    overlay_model = MeasurementOverlayModelBuilder::Build(
        overlay_measurements,
        overlay_calibration,
        MeasurementUnit::Pixels,
        angle_pending);
    if (overlay_model.pending.kind != OverlayPendingKind::Angle ||
        !Near(overlay_model.pending.first.x, 2.0) ||
        !Near(overlay_model.pending.second.y, 5.0)) {
        return Fail("MeasurementOverlayModelBuilder did not map pending angle state.");
    }

    interaction_action = interaction.FinishPolygon();
    if (interaction_action.kind != MeasurementInteractionActionKind::PolygonCompleted ||
        !interaction.IsIdle() ||
        interaction_action.polygon_points.size() != 3) {
        return Fail("MeasurementInteractionState did not complete polygon measurement.");
    }

    MeasurementCollection hit_measurements;
    hit_measurements.AddLength(L"Hit Length", ImagePoint{10.0, 10.0}, ImagePoint{80.0, 80.0});
    hit_measurements.AddAngle(L"Hit Angle", ImagePoint{20.0, 10.0}, ImagePoint{20.0, 20.0}, ImagePoint{30.0, 20.0});
    hit_measurements.AddPolygonArea(
        L"Hit Polygon",
        std::vector<ImagePoint>{ImagePoint{20.0, 70.0}, ImagePoint{40.0, 70.0}, ImagePoint{40.0, 90.0}});
    ImageViewport hit_viewport;
    const RECT hit_preview{0, 0, 100, 100};
    const ImageFrame hit_frame = MakeSolidImage(100, 100, 0, 0, 0);
    auto hit = MeasurementHitTester::FindEditableHandle(
        hit_measurements,
        hit_viewport,
        hit_preview,
        hit_frame,
        POINT{79, 82},
        5);
    if (!hit ||
        hit->measurement.kind != MeasurementKind::Length ||
        hit->point != EditablePoint::Second ||
        hit->flat_index != 0 ||
        hit->point_index != 1) {
        return Fail("MeasurementHitTester did not find the expected length endpoint.");
    }
    hit = MeasurementHitTester::FindEditableHandle(
        hit_measurements,
        hit_viewport,
        hit_preview,
        hit_frame,
        POINT{41, 89},
        5);
    if (!hit ||
        hit->measurement.kind != MeasurementKind::PolygonArea ||
        hit->flat_index != 2 ||
        hit->point_index != 2) {
        return Fail("MeasurementHitTester did not find the expected polygon vertex.");
    }
    hit = MeasurementHitTester::FindEditableHandle(
        hit_measurements,
        hit_viewport,
        hit_preview,
        hit_frame,
        POINT{95, 5},
        4);
    if (hit) {
        return Fail("MeasurementHitTester should ignore clicks outside the hit radius.");
    }

    MeasurementEditSession edit_session;
    if (edit_session.ApplyTo(hit_measurements, ImagePoint{1.0, 1.0}) ||
        edit_session.IsActive()) {
        return Fail("MeasurementEditSession should ignore updates before editing begins.");
    }
    MeasurementHitTestResult length_edit;
    length_edit.measurement = MeasurementReference{MeasurementKind::Length, 0};
    length_edit.point = EditablePoint::Second;
    length_edit.point_index = 1;
    edit_session.Begin(length_edit);
    const std::optional<MeasurementReference> edited_length =
        edit_session.ApplyTo(hit_measurements, ImagePoint{90.0, 91.0});
    if (!edit_session.IsActive() ||
        !edited_length ||
        edited_length->kind != MeasurementKind::Length ||
        !Near(hit_measurements.Lengths()[0].Second().x, 90.0) ||
        !Near(hit_measurements.Lengths()[0].Second().y, 91.0)) {
        return Fail("MeasurementEditSession did not update the active length endpoint.");
    }
    edit_session.Clear();
    if (edit_session.IsActive() ||
        edit_session.ApplyTo(hit_measurements, ImagePoint{10.0, 10.0})) {
        return Fail("MeasurementEditSession did not clear the active edit.");
    }
    MeasurementHitTestResult polygon_edit;
    polygon_edit.measurement = MeasurementReference{MeasurementKind::PolygonArea, 0};
    polygon_edit.point = EditablePoint::First;
    polygon_edit.point_index = 2;
    edit_session.Begin(polygon_edit);
    const std::optional<MeasurementReference> edited_polygon =
        edit_session.ApplyTo(hit_measurements, ImagePoint{44.0, 94.0});
    if (!edited_polygon ||
        edited_polygon->kind != MeasurementKind::PolygonArea ||
        !Near(hit_measurements.Polygons()[0].Points()[2].x, 44.0) ||
        !Near(hit_measurements.Polygons()[0].Points()[2].y, 94.0)) {
        return Fail("MeasurementEditSession did not update the active polygon vertex.");
    }
    edit_session.Clear();

    FrameBuffer frame_buffer;
    if (frame_buffer.HasFrame() || frame_buffer.Snapshot().IsValid()) {
        return Fail("FrameBuffer should start empty.");
    }
    ImageFrame buffered_frame = MakeSolidImage(3, 2, 10, 20, 30);
    frame_buffer.Publish(buffered_frame);
    buffered_frame.bgr[0] = 99;
    const ImageFrame frame_snapshot = frame_buffer.Snapshot();
    const std::shared_ptr<const ImageFrame> shared_frame_snapshot = frame_buffer.SnapshotShared();
    if (!frame_buffer.HasFrame() ||
        !frame_snapshot.IsValid() ||
        !shared_frame_snapshot ||
        !shared_frame_snapshot->IsValid() ||
        frame_snapshot.width != 3 ||
        frame_snapshot.height != 2 ||
        frame_snapshot.bgr[0] != 10 ||
        shared_frame_snapshot->bgr[0] != 10) {
        return Fail("FrameBuffer did not publish an isolated latest-frame snapshot.");
    }
    frame_buffer.Clear();
    if (frame_buffer.HasFrame() || frame_buffer.Snapshot().IsValid() || frame_buffer.SnapshotShared()) {
        return Fail("FrameBuffer did not clear the latest frame.");
    }

    CameraOpenInfo open_info;
    open_info.type = 7;
    open_info.width = 1280;
    open_info.height = 720;
    if (CameraTelemetryFormatter::FormatPreviewStarted(1, open_info) !=
            L"Previewing device 2, camera type 7, 1280x720." ||
        CameraTelemetryFormatter::FormatPendingTelemetry(1, open_info) !=
            L"Device 2 | type 7 | 1280x720 | -- fps" ||
        CameraTelemetryFormatter::FormatFrameTelemetry(1, open_info, 23.456, 9876) !=
            L"Device 2 | type 7 | 1280x720 | 23.5 fps | ts 9876") {
        return Fail("CameraTelemetryFormatter did not format preview telemetry.");
    }

    if (CameraControlStatusFormatter::FormatNoCameraSelectedForStart() !=
            L"No camera selected. Click Refresh and choose a device." ||
        CameraControlStatusFormatter::FormatCamerasFound(2) !=
            L"Found 2 camera(s). Select a device and click Open." ||
        CameraControlStatusFormatter::FormatSelectedDevice(1) !=
            L"Selected device 2. Click Open to preview." ||
        CameraControlStatusFormatter::FormatCameraDisconnected() !=
            L"Camera disconnected." ||
        CameraControlStatusFormatter::FormatPreviewStopped() !=
            L"Preview stopped." ||
        CameraControlStatusFormatter::FormatExposureSet(12.5f) !=
            L"Exposure set to 12.50 ms." ||
        CameraControlStatusFormatter::FormatExposurePending() !=
            L"Exposure will be applied when the camera opens.") {
        return Fail("CameraControlStatusFormatter did not format camera panel status text.");
    }

    if (TextInputParser::Trim(L" \t Dye Name \r\n") != L"Dye Name") {
        return Fail("TextInputParser did not trim surrounding whitespace.");
    }
    double parsed_double = 0.0;
    if (!TextInputParser::TryParsePositiveDouble(L"12.5 ms", parsed_double) || !Near(parsed_double, 12.5)) {
        return Fail("TextInputParser did not parse a positive double prefix.");
    }
    if (TextInputParser::TryParsePositiveDouble(L"0", parsed_double) ||
        TextInputParser::TryParseNonNegativeDouble(L"-0.1", parsed_double) ||
        TextInputParser::TryParseNonNegativeDouble(L"1e9999", parsed_double)) {
        return Fail("TextInputParser accepted an invalid double value.");
    }
    float parsed_float = 0.0f;
    if (!TextInputParser::TryParsePositiveFloat(L"18.25 ms", parsed_float) || !Near(parsed_float, 18.25) ||
        TextInputParser::TryParsePositiveFloat(L"1e40", parsed_float) || parsed_float != 0.0f) {
        return Fail("TextInputParser did not enforce positive float limits.");
    }
    int parsed_int = -1;
    if (!TextInputParser::TryParseByte(L"255", parsed_int) || parsed_int != 255 ||
        TextInputParser::TryParseByte(L"256", parsed_int) || parsed_int != 0 ||
        !TextInputParser::TryParseIntegerRange(L"75%", 5, 100, parsed_int) || parsed_int != 75 ||
        TextInputParser::TryParseIntegerRange(L"4", 5, 100, parsed_int) || parsed_int != 5) {
        return Fail("TextInputParser did not enforce byte and integer ranges.");
    }

    const ImagePoint calibration_start{0.0, 0.0};
    const ImagePoint calibration_end{100.0, 0.0};
    const std::vector<MeasurementUnit>& calibration_unit_options =
        CalibrationProfile::CalibrationUnitOptions();
    if (calibration_unit_options.size() != 2 ||
        calibration_unit_options[0] != MeasurementUnit::Micrometers ||
        calibration_unit_options[1] != MeasurementUnit::Millimeters ||
        CalibrationProfile::CalibrationUnitAtIndex(1) != MeasurementUnit::Millimeters ||
        CalibrationProfile::CalibrationUnitAtIndex(-1) != MeasurementUnit::Micrometers ||
        CalibrationProfile::CalibrationUnitAtIndex(99) != MeasurementUnit::Micrometers) {
        return Fail("CalibrationProfile did not expose stable calibration unit options.");
    }
    const CalibrationProfile calibration = CalibrationProfile::FromTwoPointCalibration(
        calibration_start,
        calibration_end,
        50.0,
        MeasurementUnit::Micrometers);
    if (!calibration.IsCalibrated() || !Near(calibration.MicronsPerPixel(), 0.5)) {
        return Fail("Two point calibration did not produce the expected scale.");
    }

    const CalibrationProfile millimeter_calibration = CalibrationProfile::FromTwoPointCalibration(
        calibration_start,
        calibration_end,
        1.0,
        MeasurementUnit::Millimeters);
    if (!millimeter_calibration.IsCalibrated() || !Near(millimeter_calibration.MicronsPerPixel(), 10.0)) {
        return Fail("Millimeter calibration did not convert to microns per pixel.");
    }

    const CalibrationProfile invalid_calibration = CalibrationProfile::FromTwoPointCalibration(
        calibration_start,
        calibration_start,
        50.0,
        MeasurementUnit::Micrometers);
    if (invalid_calibration.IsCalibrated()) {
        return Fail("Zero-distance calibration should be rejected.");
    }

    const LengthMeasurement measurement(L"Length 1", ImagePoint{0.0, 0.0}, ImagePoint{0.0, 200.0});
    const MeasurementResult result = measurement.Evaluate(calibration, MeasurementUnit::Micrometers);
    if (!Near(result.pixel_value, 200.0) || !Near(result.calibrated_value, 100.0)) {
        return Fail("Length measurement did not convert pixels to calibrated units.");
    }

    LengthMeasurement editable_length(L"Editable Length", ImagePoint{0.0, 0.0}, ImagePoint{10.0, 0.0});
    editable_length.SetName(L"Renamed Length");
    editable_length.SetSecond(ImagePoint{20.0, 0.0});
    if (editable_length.Name() != L"Renamed Length" || !Near(editable_length.PixelLength(), 20.0)) {
        return Fail("Editable length measurement did not update name or endpoint.");
    }

    const MeasurementResult uncalibrated_result = measurement.Evaluate(
        CalibrationProfile::Uncalibrated(),
        MeasurementUnit::Micrometers);
    if (uncalibrated_result.unit != MeasurementUnit::Pixels || !Near(uncalibrated_result.calibrated_value, 200.0)) {
        return Fail("Uncalibrated length measurement did not fall back to pixels.");
    }

    const AngleMeasurement angle_measurement(
        L"Angle 1",
        ImagePoint{1.0, 0.0},
        ImagePoint{0.0, 0.0},
        ImagePoint{0.0, 1.0});
    const MeasurementResult angle_result = angle_measurement.Evaluate();
    if (!Near(angle_result.calibrated_value, 90.0) || angle_result.unit_label != L"deg") {
        return Fail("Angle measurement did not produce the expected degree value.");
    }

    AngleMeasurement editable_angle(L"Editable Angle", ImagePoint{1.0, 0.0}, ImagePoint{0.0, 0.0}, ImagePoint{0.0, 1.0});
    editable_angle.SetName(L"Renamed Angle");
    editable_angle.SetSecond(ImagePoint{-1.0, 0.0});
    if (editable_angle.Name() != L"Renamed Angle" || !Near(editable_angle.Degrees(), 180.0)) {
        return Fail("Editable angle measurement did not update name or point.");
    }

    const RectangleAreaMeasurement area_measurement(
        L"Area 1",
        ImagePoint{0.0, 0.0},
        ImagePoint{20.0, 10.0});
    const MeasurementResult area_result = area_measurement.Evaluate(calibration, MeasurementUnit::Micrometers);
    if (!Near(area_result.pixel_value, 200.0) || !Near(area_result.calibrated_value, 50.0) || area_result.unit_label != L"um^2") {
        return Fail("Rectangle area measurement did not convert to calibrated square units.");
    }

    RectangleAreaMeasurement editable_area(L"Editable Area", ImagePoint{0.0, 0.0}, ImagePoint{5.0, 5.0});
    editable_area.SetName(L"Renamed Area");
    editable_area.SetSecond(ImagePoint{8.0, 5.0});
    if (editable_area.Name() != L"Renamed Area" || !Near(editable_area.PixelArea(), 40.0)) {
        return Fail("Editable area measurement did not update name or corner.");
    }

    PolygonAreaMeasurement polygon_measurement(
        L"Polygon 1",
        {ImagePoint{0.0, 0.0}, ImagePoint{10.0, 0.0}, ImagePoint{10.0, 10.0}, ImagePoint{0.0, 10.0}});
    const MeasurementResult polygon_result = polygon_measurement.Evaluate(calibration, MeasurementUnit::Micrometers);
    if (!Near(polygon_result.pixel_value, 100.0) || !Near(polygon_result.calibrated_value, 25.0) || polygon_result.unit_label != L"um^2") {
        return Fail("Polygon area measurement did not convert to calibrated square units.");
    }
    polygon_measurement.SetName(L"Renamed Polygon");
    polygon_measurement.SetPoint(2, ImagePoint{20.0, 10.0});
    if (polygon_measurement.Name() != L"Renamed Polygon" || !Near(polygon_measurement.PixelArea(), 150.0)) {
        return Fail("Editable polygon measurement did not update name or vertex.");
    }

    const std::wstring overlay_length_text =
        OverlayRenderer::FormatLine(measurement, calibration, MeasurementUnit::Micrometers);
    if (overlay_length_text.find(L"Length 1: 100.00 um") == std::wstring::npos ||
        overlay_length_text.find(L"(200.0 raw)") == std::wstring::npos) {
        return Fail("OverlayRenderer did not preserve formatted length measurement text.");
    }
    if (MeasurementFormatter::FormatLine(measurement, calibration, MeasurementUnit::Micrometers) != overlay_length_text) {
        return Fail("MeasurementFormatter and OverlayRenderer formatted length text differently.");
    }

    MeasurementCollection measurement_collection;
    measurement_collection.AddLength(L"Stored Length", ImagePoint{0.0, 0.0}, ImagePoint{3.0, 4.0});
    measurement_collection.AddAngle(L"Stored Angle", ImagePoint{1.0, 0.0}, ImagePoint{0.0, 0.0}, ImagePoint{0.0, 1.0});
    measurement_collection.AddRectangleArea(L"Stored Area", ImagePoint{0.0, 0.0}, ImagePoint{4.0, 5.0});
    measurement_collection.AddPolygonArea(
        L"Stored Polygon",
        std::vector<ImagePoint>{ImagePoint{0.0, 0.0}, ImagePoint{4.0, 0.0}, ImagePoint{4.0, 4.0}});
    if (measurement_collection.Count() != 4 || measurement_collection.PolygonCount() != 1) {
        return Fail("MeasurementCollection did not track measurement counts.");
    }
    if (MeasurementNameFormatter::FormatDefaultName(MeasurementKind::Length, 3) != L"Length 3" ||
        MeasurementNameFormatter::FormatDefaultName(MeasurementKind::None, 2) != L"Measurement 2" ||
        MeasurementNameFormatter::NextDefaultName(MeasurementKind::Length, measurement_collection) != L"Length 2" ||
        MeasurementNameFormatter::NextDefaultName(MeasurementKind::Angle, measurement_collection) != L"Angle 2" ||
        MeasurementNameFormatter::NextDefaultName(MeasurementKind::RectangleArea, measurement_collection) != L"Area 2" ||
        MeasurementNameFormatter::NextDefaultName(MeasurementKind::PolygonArea, measurement_collection) != L"Polygon 2") {
        return Fail("MeasurementNameFormatter did not produce default measurement names.");
    }
    if (!MeasurementListSelection::IndexAtSelection(0, measurement_collection.Count()) ||
        MeasurementListSelection::IndexAtSelection(-1, measurement_collection.Count()) ||
        MeasurementListSelection::IndexAtSelection(static_cast<int>(measurement_collection.Count()), measurement_collection.Count()) ||
        MeasurementListSelection::NextAfterDelete(0, 0) ||
        MeasurementListSelection::NextAfterDelete(1, 3) != 1 ||
        MeasurementListSelection::NextAfterDelete(3, 3) != 2) {
        return Fail("MeasurementListSelection did not map list selection indexes.");
    }
    if (MeasurementDisplayActions::DisplayUnit(CalibrationProfile::Uncalibrated()) != MeasurementUnit::Pixels ||
        MeasurementDisplayActions::DisplayUnit(calibration) != MeasurementUnit::Micrometers ||
        MeasurementDisplayActions::MeasurementCount(measurement_collection) != 4) {
        return Fail("MeasurementDisplayActions did not choose display units or count measurements.");
    }
    const std::vector<std::wstring> display_lines =
        MeasurementDisplayActions::ListLines(measurement_collection, calibration);
    if (display_lines.size() != 4 ||
        display_lines[0].find(L"Stored Length: 2.50 um") == std::wstring::npos ||
        display_lines[3].find(L"Stored Polygon: 2.00 um^2") == std::wstring::npos) {
        return Fail("MeasurementDisplayActions did not format measurement list lines.");
    }
    const auto selected_measurement = MeasurementDisplayActions::SelectedMeasurement(measurement_collection, 2);
    if (!selected_measurement ||
        selected_measurement->kind != MeasurementKind::RectangleArea ||
        selected_measurement->index != 0 ||
        MeasurementDisplayActions::SelectedMeasurement(measurement_collection, -1) ||
        MeasurementDisplayActions::SelectedMeasurement(
            measurement_collection,
            static_cast<int>(measurement_collection.Count()))) {
        return Fail("MeasurementDisplayActions did not map selected measurements.");
    }
    MeasurementInteractionState display_interaction;
    display_interaction.BeginLength();
    display_interaction.AddPoint(ImagePoint{7.0, 8.0});
    const MeasurementOverlayModel display_overlay =
        MeasurementDisplayActions::BuildOverlayModel(measurement_collection, calibration, display_interaction);
    if (display_overlay.calibration != &calibration ||
        display_overlay.display_unit != MeasurementUnit::Micrometers ||
        display_overlay.lengths != &measurement_collection.Lengths() ||
        display_overlay.pending.kind != OverlayPendingKind::Point ||
        !Near(display_overlay.pending.first.x, 7.0) ||
        !Near(display_overlay.pending.first.y, 8.0)) {
        return Fail("MeasurementDisplayActions did not build the overlay model.");
    }
    MeasurementCollection list_action_measurements;
    MeasurementListActionResult list_action_result =
        MeasurementListActions::DeleteSelected(list_action_measurements, 0);
    if (list_action_result.status != MeasurementListActionStatus::NoMeasurement ||
        list_action_result.changed) {
        return Fail("MeasurementListActions did not reject delete on an empty collection.");
    }
    list_action_measurements.AddLength(L"Action Length 1", ImagePoint{0.0, 0.0}, ImagePoint{1.0, 0.0});
    list_action_measurements.AddLength(L"Action Length 2", ImagePoint{0.0, 0.0}, ImagePoint{2.0, 0.0});
    list_action_result = MeasurementListActions::DeleteSelected(list_action_measurements, -1);
    if (list_action_result.status != MeasurementListActionStatus::NoSelection ||
        list_action_result.changed) {
        return Fail("MeasurementListActions did not reject delete without a selection.");
    }
    list_action_result = MeasurementListActions::DeleteSelected(list_action_measurements, 0);
    if (list_action_result.status != MeasurementListActionStatus::Deleted ||
        !list_action_result.changed ||
        list_action_result.next_selection != 0 ||
        list_action_measurements.Count() != 1 ||
        list_action_measurements.Lengths()[0].Name() != L"Action Length 2") {
        return Fail("MeasurementListActions did not delete and select the next measurement.");
    }
    list_action_result = MeasurementListActions::RenameSelected(list_action_measurements, 0, L"   ");
    if (list_action_result.status != MeasurementListActionStatus::EmptyName ||
        list_action_result.changed) {
        return Fail("MeasurementListActions accepted an empty rename.");
    }
    list_action_result = MeasurementListActions::RenameSelected(list_action_measurements, 0, L"  Trimmed Name  ");
    if (list_action_result.status != MeasurementListActionStatus::Renamed ||
        !list_action_result.changed ||
        list_action_result.next_selection != 0 ||
        list_action_result.applied_name != L"Trimmed Name" ||
        list_action_measurements.Lengths()[0].Name() != L"Trimmed Name") {
        return Fail("MeasurementListActions did not trim and apply a rename.");
    }
    const auto rectangle_reference = measurement_collection.AtFlatIndex(2);
    if (!rectangle_reference || rectangle_reference->kind != MeasurementKind::RectangleArea || rectangle_reference->index != 0) {
        return Fail("MeasurementCollection flat index lookup returned the wrong measurement.");
    }
    if (!measurement_collection.SetName(*rectangle_reference, L"Renamed Stored Area") ||
        measurement_collection.Name(*rectangle_reference) != L"Renamed Stored Area") {
        return Fail("MeasurementCollection did not rename the selected measurement.");
    }
    if (!measurement_collection.SetPoint(*rectangle_reference, EditablePoint::Second, 0, ImagePoint{8.0, 5.0}) ||
        !Near(measurement_collection.Rectangles()[0].PixelArea(), 40.0)) {
        return Fail("MeasurementCollection did not update an editable measurement point.");
    }
    const std::vector<std::wstring> measurement_lines =
        MeasurementFormatter::FormatCollection(measurement_collection, calibration, MeasurementUnit::Micrometers);
    if (measurement_lines.size() != 4 ||
        measurement_lines[0].find(L"Stored Length: 2.50 um") == std::wstring::npos ||
        measurement_lines[1].find(L"Stored Angle: 90.00 deg") == std::wstring::npos ||
        measurement_lines[2].find(L"Renamed Stored Area: 10.00 um^2") == std::wstring::npos ||
        measurement_lines[3].find(L"Stored Polygon: 2.00 um^2") == std::wstring::npos) {
        return Fail("MeasurementFormatter did not format the measurement collection in list order.");
    }
    if (!measurement_collection.EraseAtFlatIndex(1) || measurement_collection.Count() != 3 ||
        measurement_collection.FlatIndexOf(*rectangle_reference) != 1) {
        return Fail("MeasurementCollection did not erase a flat-indexed measurement correctly.");
    }

    ProcessingJobState processing_state;
    const ProcessingJobLaunch first_job = processing_state.Begin();
    if (!processing_state.IsRunning() || first_job.job_id != 1 || !first_job.cancel_token || first_job.cancel_token->load()) {
        return Fail("ProcessingJobState did not start the first job correctly.");
    }
    processing_state.RequestCancel();
    if (!first_job.cancel_token->load()) {
        return Fail("ProcessingJobState did not propagate cancellation.");
    }
    ProcessingJobResult stale_result;
    stale_result.job_id = first_job.job_id;
    stale_result.kind = ProcessingJobKind::Stitch;
    stale_result.succeeded = true;
    processing_state.InvalidateActiveJob();
    processing_state.Publish(std::move(stale_result));
    const ProcessingJobResult ignored_result = processing_state.TakePending();
    if (processing_state.IsCurrentResult(ignored_result)) {
        return Fail("ProcessingJobState did not invalidate stale job results.");
    }
    const ProcessingJobLaunch second_job = processing_state.Begin();
    ProcessingJobResult current_result;
    current_result.job_id = second_job.job_id;
    current_result.kind = ProcessingJobKind::Edf;
    current_result.succeeded = true;
    current_result.image = MakeSolidImage(1, 1, 1, 2, 3);
    processing_state.Publish(std::move(current_result));
    const ProcessingJobResult published_result = processing_state.TakePending();
    if (processing_state.IsRunning() ||
        !processing_state.IsCurrentResult(published_result) ||
        !published_result.has_result ||
        published_result.kind != ProcessingJobKind::Edf ||
        !published_result.image.IsValid()) {
        return Fail("ProcessingJobState did not publish the current job result.");
    }

    ProcessingResultFrames processing_frames;
    ProcessingJobResult stitch_frame_result;
    stitch_frame_result.kind = ProcessingJobKind::Stitch;
    stitch_frame_result.succeeded = true;
    stitch_frame_result.image = MakeSolidImage(2, 2, 10, 20, 30);
    if (!processing_frames.Apply(std::move(stitch_frame_result)) ||
        !processing_frames.IsProcessingResultVisible() ||
        processing_frames.ProcessingResult().width != 2 ||
        processing_frames.DisplaySource() != ProcessingResultDisplaySource::Stitch ||
        processing_frames.DisplayKindLabel() != L"Stitch" ||
        processing_frames.DisplaySourceLabel() != L"Stitch result" ||
        processing_frames.EdfCompositeFrame().IsValid() ||
        processing_frames.EdfFocusMap().IsValid()) {
        return Fail("ProcessingResultFrames did not apply a stitch result.");
    }
    ProcessingJobResult edf_frame_result;
    edf_frame_result.kind = ProcessingJobKind::Edf;
    edf_frame_result.succeeded = true;
    edf_frame_result.image = MakeSolidImage(3, 2, 1, 2, 3);
    edf_frame_result.focus_map = MakeSolidImage(3, 2, 9, 8, 7);
    if (!processing_frames.Apply(std::move(edf_frame_result)) ||
        !processing_frames.IsProcessingResultVisible() ||
        processing_frames.EdfCompositeFrame().width != 3 ||
        !processing_frames.EdfFocusMap().IsValid() ||
        processing_frames.ProcessingResult().bgr[0] != 1 ||
        processing_frames.DisplaySource() != ProcessingResultDisplaySource::EdfComposite ||
        processing_frames.DisplayKindLabel() != L"EDF" ||
        processing_frames.DisplaySourceLabel() != L"EDF composite" ||
        !processing_frames.ShowEdfFocusMap() ||
        processing_frames.ProcessingResult().bgr[0] != 9 ||
        processing_frames.DisplaySource() != ProcessingResultDisplaySource::EdfFocusMap ||
        processing_frames.DisplaySourceLabel() != L"EDF focus map" ||
        !processing_frames.ShowEdfCompositeFrame() ||
        processing_frames.ProcessingResult().bgr[0] != 1 ||
        processing_frames.DisplaySource() != ProcessingResultDisplaySource::EdfComposite) {
        return Fail("ProcessingResultFrames did not apply EDF result and display switching state.");
    }
    ProcessingJobResult failed_frame_result;
    failed_frame_result.kind = ProcessingJobKind::Edf;
    failed_frame_result.succeeded = false;
    failed_frame_result.image = MakeSolidImage(1, 1, 50, 60, 70);
    if (processing_frames.Apply(std::move(failed_frame_result)) ||
        processing_frames.ProcessingResult().bgr[0] != 1) {
        return Fail("ProcessingResultFrames should ignore failed results.");
    }
    if (!processing_frames.Clear()) {
        return Fail("ProcessingResultFrames did not report clearing a visible result.");
    }
    if (processing_frames.IsProcessingResultVisible() ||
        processing_frames.ProcessingResult().IsValid() ||
        processing_frames.DisplaySource() != ProcessingResultDisplaySource::None ||
        processing_frames.DisplayKindLabel() != L"" ||
        processing_frames.DisplaySourceLabel() != L"(none)" ||
        processing_frames.ShowEdfCompositeFrame() ||
        processing_frames.ShowEdfFocusMap()) {
        return Fail("ProcessingResultFrames did not clear display state.");
    }
    if (processing_frames.Clear()) {
        return Fail("ProcessingResultFrames should not report a visible result when already clear.");
    }

    const ProcessingBuildActionResult no_stitch_build =
        ProcessingBuildActions::PrepareStitch({}, true, 50);
    if (no_stitch_build.can_start ||
        no_stitch_build.status != ProcessingBuildActionStatus::NoStitchTiles ||
        no_stitch_build.message != L"Add stitch tiles before building a stitched image.") {
        return Fail("ProcessingBuildActions did not reject empty stitch input.");
    }
    std::vector<StitchTile> build_tiles(1);
    build_tiles[0].frame = MakeSolidImage(2, 2, 1, 2, 3);
    const ProcessingBuildActionResult invalid_stitch_build =
        ProcessingBuildActions::PrepareStitch(build_tiles, false, 4);
    if (invalid_stitch_build.can_start ||
        invalid_stitch_build.status != ProcessingBuildActionStatus::InvalidStitchSearch ||
        invalid_stitch_build.message != L"Stitch search must be 5-100 percent.") {
        return Fail("ProcessingBuildActions did not reject an invalid stitch search.");
    }
    const ProcessingBuildActionResult ready_stitch_build =
        ProcessingBuildActions::PrepareStitch(build_tiles, true, 45);
    if (!ready_stitch_build.can_start ||
        ready_stitch_build.kind != ProcessingJobKind::Stitch ||
        ready_stitch_build.status != ProcessingBuildActionStatus::StitchReady ||
        ready_stitch_build.stitch_tiles.size() != 1 ||
        ready_stitch_build.stitch_search_percent != 45) {
        return Fail("ProcessingBuildActions did not prepare a stitch build request.");
    }
    const ProcessingIntegerInputResult skipped_stitch_search_input =
        ProcessingBuildInputActions::StitchSearchForNextTile(false, L"bad", 33);
    const ProcessingIntegerInputResult invalid_stitch_search_input =
        ProcessingBuildInputActions::StitchSearchForNextTile(true, L"101", 33);
    const ProcessingIntegerInputResult valid_stitch_search_input =
        ProcessingBuildInputActions::StitchSearchForNextTile(true, L"45%", 33);
    const ProcessingBuildActionResult text_stitch_build =
        ProcessingBuildInputActions::PrepareStitch(build_tiles, L"44");
    const ProcessingBuildActionResult text_invalid_stitch_build =
        ProcessingBuildInputActions::PrepareStitch(build_tiles, L"bad");
    if (!skipped_stitch_search_input.accepted ||
        skipped_stitch_search_input.value != 33 ||
        invalid_stitch_search_input.accepted ||
        invalid_stitch_search_input.message != L"Stitch search must be 5-100 percent." ||
        !valid_stitch_search_input.accepted ||
        valid_stitch_search_input.value != 45 ||
        !text_stitch_build.can_start ||
        text_stitch_build.stitch_search_percent != 44 ||
        text_invalid_stitch_build.can_start ||
        text_invalid_stitch_build.status != ProcessingBuildActionStatus::InvalidStitchSearch) {
        return Fail("ProcessingBuildInputActions did not prepare stitch inputs from text.");
    }
    const ProcessingBuildActionResult no_edf_build =
        ProcessingBuildActions::PrepareEdf({MakeSolidImage(1, 1, 1, 1, 1)}, true, 2);
    if (no_edf_build.can_start ||
        no_edf_build.status != ProcessingBuildActionStatus::NotEnoughEdfFrames ||
        no_edf_build.message != L"Add at least two EDF frames before building EDF.") {
        return Fail("ProcessingBuildActions did not reject a short EDF stack.");
    }
    std::vector<ImageFrame> build_edf_stack{
        MakeSolidImage(1, 1, 1, 1, 1),
        MakeSolidImage(1, 1, 2, 2, 2),
    };
    const ProcessingBuildActionResult invalid_edf_build =
        ProcessingBuildActions::PrepareEdf(build_edf_stack, false, 99);
    if (invalid_edf_build.can_start ||
        invalid_edf_build.status != ProcessingBuildActionStatus::InvalidEdfRadius ||
        invalid_edf_build.message != L"EDF radius must be 1-16.") {
        return Fail("ProcessingBuildActions did not reject an invalid EDF radius.");
    }
    const ProcessingBuildActionResult ready_edf_build =
        ProcessingBuildActions::PrepareEdf(build_edf_stack, true, 3);
    if (!ready_edf_build.can_start ||
        ready_edf_build.kind != ProcessingJobKind::Edf ||
        ready_edf_build.status != ProcessingBuildActionStatus::EdfReady ||
        ready_edf_build.edf_stack.size() != 2 ||
        ready_edf_build.edf_options.focus_radius != 3) {
        return Fail("ProcessingBuildActions did not prepare an EDF build request.");
    }
    const ProcessingBuildActionResult text_edf_build =
        ProcessingBuildInputActions::PrepareEdf(build_edf_stack, L"4");
    const ProcessingBuildActionResult text_invalid_edf_build =
        ProcessingBuildInputActions::PrepareEdf(build_edf_stack, L"17");
    if (!text_edf_build.can_start ||
        text_edf_build.edf_options.focus_radius != 4 ||
        text_invalid_edf_build.can_start ||
        text_invalid_edf_build.status != ProcessingBuildActionStatus::InvalidEdfRadius) {
        return Fail("ProcessingBuildInputActions did not prepare EDF inputs from text.");
    }

    ProcessingResultFrames queue_frames;
    ProcessingJobResult visible_queue_result;
    visible_queue_result.kind = ProcessingJobKind::Stitch;
    visible_queue_result.succeeded = true;
    visible_queue_result.image = MakeSolidImage(2, 2, 1, 2, 3);
    queue_frames.Apply(std::move(visible_queue_result));
    std::vector<StitchTile> queue_tiles;
    const ProcessingQueueActionResult missing_queue_stitch =
        ProcessingQueueActions::AddStitchTile(queue_tiles, queue_frames, ImageFrame{}, L"bad", 33);
    if (missing_queue_stitch.changed ||
        missing_queue_stitch.preview_changed ||
        missing_queue_stitch.status != ProcessingQueueActionStatus::StitchNotAdded ||
        missing_queue_stitch.stitch_search_percent != 33 ||
        !queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not reject an empty stitch frame without clearing results.");
    }
    const ProcessingQueueActionResult first_queue_stitch =
        ProcessingQueueActions::AddStitchTile(
            queue_tiles,
            queue_frames,
            MakeSolidImage(2, 2, 1, 2, 3),
            L"bad",
            33);
    if (!first_queue_stitch.changed ||
        !first_queue_stitch.preview_changed ||
        first_queue_stitch.status != ProcessingQueueActionStatus::StitchAdded ||
        first_queue_stitch.stitch_search_percent != 33 ||
        first_queue_stitch.stitch_tile_count != 1 ||
        queue_tiles.size() != 1 ||
        queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not add the first stitch tile and clear old results.");
    }
    ProcessingResultFrames idle_queue_frames;
    std::vector<StitchTile> idle_queue_tiles;
    const ProcessingQueueActionResult idle_queue_stitch =
        ProcessingQueueActions::AddStitchTile(
            idle_queue_tiles,
            idle_queue_frames,
            MakeSolidImage(2, 2, 3, 3, 3),
            L"50",
            50);
    if (!idle_queue_stitch.changed ||
        idle_queue_stitch.preview_changed ||
        idle_queue_stitch.status != ProcessingQueueActionStatus::StitchAdded ||
        idle_queue_stitch.stitch_tile_count != 1 ||
        idle_queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions should not request preview redraw when no result was visible.");
    }
    ProcessingJobResult visible_queue_result_again;
    visible_queue_result_again.kind = ProcessingJobKind::Stitch;
    visible_queue_result_again.succeeded = true;
    visible_queue_result_again.image = MakeSolidImage(2, 2, 4, 5, 6);
    queue_frames.Apply(std::move(visible_queue_result_again));
    const ProcessingQueueActionResult invalid_queue_stitch =
        ProcessingQueueActions::AddStitchTile(
            queue_tiles,
            queue_frames,
            MakeSolidImage(2, 2, 7, 8, 9),
            L"101",
            33);
    if (invalid_queue_stitch.changed ||
        invalid_queue_stitch.preview_changed ||
        invalid_queue_stitch.status != ProcessingQueueActionStatus::InvalidStitchSearch ||
        invalid_queue_stitch.stitch_search_percent != 33 ||
        queue_tiles.size() != 1 ||
        !queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not reject invalid stitch search without clearing results.");
    }
    const ProcessingQueueActionResult second_queue_stitch =
        ProcessingQueueActions::AddStitchTile(
            queue_tiles,
            queue_frames,
            MakeSolidImage(2, 2, 10, 11, 12),
            L"45",
            33);
    if (!second_queue_stitch.changed ||
        !second_queue_stitch.preview_changed ||
        second_queue_stitch.status != ProcessingQueueActionStatus::StitchAdded ||
        second_queue_stitch.stitch_search_percent != 45 ||
        second_queue_stitch.stitch_tile_count != 2 ||
        queue_tiles.size() != 2 ||
        queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not add a later stitch tile with parsed search.");
    }
    ProcessingJobResult visible_edf_queue_result;
    visible_edf_queue_result.kind = ProcessingJobKind::Edf;
    visible_edf_queue_result.succeeded = true;
    visible_edf_queue_result.image = MakeSolidImage(2, 2, 1, 1, 1);
    queue_frames.Apply(std::move(visible_edf_queue_result));
    std::vector<ImageFrame> queue_edf_stack;
    const ProcessingQueueActionResult missing_queue_edf =
        ProcessingQueueActions::AddEdfFrame(queue_edf_stack, queue_frames, ImageFrame{});
    if (missing_queue_edf.changed ||
        missing_queue_edf.preview_changed ||
        missing_queue_edf.status != ProcessingQueueActionStatus::EdfNotAdded ||
        !queue_edf_stack.empty() ||
        !queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not reject an empty EDF frame without clearing results.");
    }
    const ProcessingQueueActionResult first_queue_edf =
        ProcessingQueueActions::AddEdfFrame(queue_edf_stack, queue_frames, MakeSolidImage(2, 2, 2, 2, 2));
    if (!first_queue_edf.changed ||
        !first_queue_edf.preview_changed ||
        first_queue_edf.status != ProcessingQueueActionStatus::EdfAdded ||
        first_queue_edf.edf_frame_count != 1 ||
        queue_edf_stack.size() != 1 ||
        queue_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingQueueActions did not add an EDF frame and clear old results.");
    }

    ProcessingJobState start_action_state;
    ProcessingRetryState start_action_retry;
    int start_join_count = 0;
    const ProcessingStartActionResult empty_stitch_start = ProcessingStartActions::StartStitch(
        start_action_state,
        start_action_retry,
        {},
        50,
        true,
        [&start_join_count]() { ++start_join_count; });
    if (empty_stitch_start.can_start ||
        empty_stitch_start.status != ProcessingStartActionStatus::NoStitchTiles ||
        empty_stitch_start.message != L"Add stitch tiles before building a stitched image." ||
        start_join_count != 0 ||
        start_action_state.IsRunning()) {
        return Fail("ProcessingStartActions did not reject empty stitch startup.");
    }
    ProcessingJobState busy_start_state;
    ProcessingRetryState busy_start_retry;
    const ProcessingJobLaunch busy_launch = busy_start_state.Begin();
    if (busy_launch.job_id == 0) {
        return Fail("ProcessingJobState did not create a busy launch for startup tests.");
    }
    int busy_join_count = 0;
    const ProcessingStartActionResult busy_start = ProcessingStartActions::StartStitch(
        busy_start_state,
        busy_start_retry,
        build_tiles,
        45,
        true,
        [&busy_join_count]() { ++busy_join_count; });
    if (busy_start.can_start ||
        busy_start.status != ProcessingStartActionStatus::AlreadyRunning ||
        busy_start.message != ProcessingStatusFormatter::FormatAlreadyRunning() ||
        busy_join_count != 0 ||
        !busy_start_state.IsRunning()) {
        return Fail("ProcessingStartActions did not reject a running processing job.");
    }
    busy_start_state.MarkIdle();
    ProcessingJobState stitch_start_state;
    ProcessingRetryState stitch_start_retry;
    int stitch_join_count = 0;
    const ProcessingStartActionResult stitch_start = ProcessingStartActions::StartStitch(
        stitch_start_state,
        stitch_start_retry,
        build_tiles,
        45,
        true,
        [&stitch_join_count]() { ++stitch_join_count; });
    const ProcessingRetryRequest stitch_start_request = stitch_start_retry.Request();
    if (!stitch_start.can_start ||
        stitch_start.status != ProcessingStartActionStatus::Started ||
        stitch_start.kind != ProcessingJobKind::Stitch ||
        stitch_start.message != ProcessingStatusFormatter::FormatStarted(ProcessingJobKind::Stitch) ||
        stitch_start.launch.job_id == 0 ||
        !stitch_start.launch.cancel_token ||
        !stitch_start_state.IsRunning() ||
        stitch_join_count != 1 ||
        stitch_start_request.kind != ProcessingJobKind::Stitch ||
        stitch_start_request.stitch_tiles.size() != 1 ||
        stitch_start_request.stitch_search_percent != 45) {
        return Fail("ProcessingStartActions did not start a stitch job and remember its snapshot.");
    }
    stitch_start_state.MarkIdle();
    ProcessingJobState edf_short_start_state;
    ProcessingRetryState edf_short_start_retry;
    int edf_short_join_count = 0;
    const ProcessingStartActionResult edf_short_start = ProcessingStartActions::StartEdf(
        edf_short_start_state,
        edf_short_start_retry,
        {MakeSolidImage(1, 1, 1, 1, 1)},
        EdfOptions{},
        true,
        [&edf_short_join_count]() { ++edf_short_join_count; });
    if (edf_short_start.can_start ||
        edf_short_start.status != ProcessingStartActionStatus::NotEnoughEdfFrames ||
        edf_short_start.message != L"Add at least two EDF frames before building EDF." ||
        edf_short_join_count != 0 ||
        edf_short_start_state.IsRunning()) {
        return Fail("ProcessingStartActions did not reject a short EDF startup stack.");
    }
    ProcessingJobState edf_start_state;
    ProcessingRetryState edf_start_retry;
    EdfOptions edf_start_options;
    edf_start_options.focus_radius = 4;
    int edf_join_count = 0;
    const ProcessingStartActionResult edf_start = ProcessingStartActions::StartEdf(
        edf_start_state,
        edf_start_retry,
        build_edf_stack,
        edf_start_options,
        true,
        [&edf_join_count]() { ++edf_join_count; });
    const ProcessingRetryRequest edf_start_request = edf_start_retry.Request();
    if (!edf_start.can_start ||
        edf_start.status != ProcessingStartActionStatus::Started ||
        edf_start.kind != ProcessingJobKind::Edf ||
        edf_start.message != ProcessingStatusFormatter::FormatStarted(ProcessingJobKind::Edf) ||
        edf_start.launch.job_id == 0 ||
        !edf_start.launch.cancel_token ||
        !edf_start_state.IsRunning() ||
        edf_join_count != 1 ||
        edf_start_request.kind != ProcessingJobKind::Edf ||
        edf_start_request.edf_stack.size() != 2 ||
        edf_start_request.edf_options.focus_radius != 4) {
        return Fail("ProcessingStartActions did not start an EDF job and remember its snapshot.");
    }
    edf_start_state.MarkIdle();
    ProcessingJobState no_remember_start_state;
    ProcessingRetryState no_remember_start_retry;
    int no_remember_join_count = 0;
    const ProcessingStartActionResult no_remember_start = ProcessingStartActions::StartStitch(
        no_remember_start_state,
        no_remember_start_retry,
        build_tiles,
        45,
        false,
        [&no_remember_join_count]() { ++no_remember_join_count; });
    if (!no_remember_start.can_start ||
        no_remember_start_retry.Request().kind != ProcessingJobKind::None ||
        no_remember_join_count != 1) {
        return Fail("ProcessingStartActions should not remember a snapshot when disabled.");
    }
    no_remember_start_state.MarkIdle();

    ProcessingJobState result_action_state;
    ProcessingResultFrames result_action_frames;
    const ProcessingResultActionResult no_pending_result_action =
        ProcessingResultActions::ApplyPending(result_action_state, result_action_frames);
    if (no_pending_result_action.handled ||
        no_pending_result_action.changed ||
        no_pending_result_action.status != ProcessingResultActionStatus::NoResult) {
        return Fail("ProcessingResultActions should ignore missing pending results.");
    }
    const ProcessingJobLaunch stale_action_job = result_action_state.Begin();
    ProcessingJobResult stale_action_result;
    stale_action_result.job_id = stale_action_job.job_id;
    stale_action_result.kind = ProcessingJobKind::Stitch;
    stale_action_result.succeeded = true;
    stale_action_result.image = MakeSolidImage(2, 2, 1, 2, 3);
    result_action_state.InvalidateActiveJob();
    result_action_state.Publish(std::move(stale_action_result));
    const ProcessingResultActionResult stale_result_action =
        ProcessingResultActions::ApplyPending(result_action_state, result_action_frames);
    if (stale_result_action.handled ||
        stale_result_action.changed ||
        stale_result_action.status != ProcessingResultActionStatus::StaleResult ||
        result_action_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingResultActions should ignore stale processing results.");
    }
    const ProcessingJobLaunch canceled_action_job = result_action_state.Begin();
    result_action_state.RequestCancel();
    result_action_state.InvalidateActiveJob();
    if (!canceled_action_job.cancel_token || !canceled_action_job.cancel_token->load()) {
        return Fail("ProcessingJobState did not cancel the active job token.");
    }
    ProcessingJobResult canceled_action_result;
    canceled_action_result.job_id = canceled_action_job.job_id;
    canceled_action_result.kind = ProcessingJobKind::Edf;
    canceled_action_result.succeeded = true;
    canceled_action_result.image = MakeSolidImage(2, 2, 8, 9, 10);
    result_action_state.Publish(std::move(canceled_action_result));
    const ProcessingResultActionResult canceled_stale_result_action =
        ProcessingResultActions::ApplyPending(result_action_state, result_action_frames);
    if (canceled_stale_result_action.handled ||
        canceled_stale_result_action.changed ||
        canceled_stale_result_action.status != ProcessingResultActionStatus::StaleResult ||
        result_action_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingResultActions should ignore canceled stale processing results.");
    }
    const ProcessingJobLaunch failed_action_job = result_action_state.Begin();
    ProcessingJobResult failed_action_result;
    failed_action_result.job_id = failed_action_job.job_id;
    failed_action_result.kind = ProcessingJobKind::Edf;
    failed_action_result.succeeded = false;
    failed_action_result.status = L"EDF failed.";
    result_action_state.Publish(std::move(failed_action_result));
    const ProcessingResultActionResult failed_result_action =
        ProcessingResultActions::ApplyPending(result_action_state, result_action_frames);
    if (!failed_result_action.handled ||
        failed_result_action.changed ||
        failed_result_action.status != ProcessingResultActionStatus::Failed ||
        failed_result_action.message != L"EDF failed." ||
        result_action_frames.IsProcessingResultVisible()) {
        return Fail("ProcessingResultActions did not report a failed processing result.");
    }
    const ProcessingJobLaunch applied_action_job = result_action_state.Begin();
    ProcessingJobResult applied_action_result;
    applied_action_result.job_id = applied_action_job.job_id;
    applied_action_result.kind = ProcessingJobKind::Edf;
    applied_action_result.succeeded = true;
    applied_action_result.image = MakeSolidImage(3, 2, 4, 5, 6);
    applied_action_result.focus_map = MakeSolidImage(3, 2, 7, 8, 9);
    applied_action_result.status = L"EDF image ready: 3x2.";
    result_action_state.Publish(std::move(applied_action_result));
    const ProcessingResultActionResult applied_result_action =
        ProcessingResultActions::ApplyPending(result_action_state, result_action_frames);
    if (!applied_result_action.handled ||
        !applied_result_action.changed ||
        applied_result_action.status != ProcessingResultActionStatus::Applied ||
        applied_result_action.message != L"EDF image ready: 3x2." ||
        !result_action_frames.IsProcessingResultVisible() ||
        !result_action_frames.EdfFocusMap().IsValid() ||
        result_action_frames.DisplaySource() != ProcessingResultDisplaySource::EdfComposite ||
        result_action_frames.DisplaySourceLabel() != L"EDF composite") {
        return Fail("ProcessingResultActions did not apply a successful processing result.");
    }

    ProcessingRetryState retry_state;
    ProcessingRetryRequest retry_request = retry_state.Request();
    if (retry_request.kind != ProcessingJobKind::None ||
        retry_request.CanRetry() ||
        retry_request.stitch_search_percent != ProcessingParameterRules::DefaultStitchSearchPercent() ||
        retry_request.edf_options.focus_radius != ProcessingParameterRules::DefaultEdfOptions().focus_radius) {
        return Fail("ProcessingRetryState did not expose default retry state.");
    }
    std::vector<StitchTile> retry_tiles(1);
    retry_tiles[0].frame = MakeSolidImage(2, 2, 1, 2, 3);
    retry_state.RememberStitch(retry_tiles, 35);
    retry_request = retry_state.Request();
    if (retry_request.kind != ProcessingJobKind::Stitch ||
        !retry_request.CanRetry() ||
        retry_request.stitch_tiles.size() != 1 ||
        retry_request.stitch_search_percent != 35 ||
        !retry_request.edf_stack.empty()) {
        return Fail("ProcessingRetryState did not remember stitch retry input.");
    }
    std::vector<ImageFrame> retry_stack{
        MakeSolidImage(2, 2, 1, 1, 1),
        MakeSolidImage(2, 2, 2, 2, 2),
    };
    EdfOptions retry_edf_options;
    retry_edf_options.focus_radius = 4;
    retry_state.RememberEdf(retry_stack, retry_edf_options);
    retry_request = retry_state.Request();
    if (retry_request.kind != ProcessingJobKind::Edf ||
        !retry_request.CanRetry() ||
        retry_request.edf_stack.size() != 2 ||
        retry_request.edf_options.focus_radius != 4 ||
        !retry_request.stitch_tiles.empty()) {
        return Fail("ProcessingRetryState did not remember EDF retry input.");
    }
    retry_state.RememberEdf(std::vector<ImageFrame>{MakeSolidImage(1, 1, 3, 3, 3)}, retry_edf_options);
    if (retry_state.Request().CanRetry()) {
        return Fail("ProcessingRetryState should reject EDF retry with fewer than two frames.");
    }
    retry_state.Clear();
    if (retry_state.Request().kind != ProcessingJobKind::None ||
        retry_state.Request().CanRetry()) {
        return Fail("ProcessingRetryState did not clear retry state.");
    }

    const ProcessingRetryActionResult no_retry_action =
        ProcessingRetryActions::Prepare(retry_state);
    if (no_retry_action.can_start ||
        no_retry_action.status != ProcessingRetryActionStatus::NoRetry ||
        no_retry_action.kind != ProcessingJobKind::None ||
        no_retry_action.message != ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind::None)) {
        return Fail("ProcessingRetryActions did not reject an empty retry state.");
    }
    retry_state.RememberStitch({}, 45);
    const ProcessingRetryActionResult empty_stitch_retry_action =
        ProcessingRetryActions::Prepare(retry_state);
    if (empty_stitch_retry_action.can_start ||
        empty_stitch_retry_action.kind != ProcessingJobKind::Stitch ||
        empty_stitch_retry_action.message != ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind::Stitch)) {
        return Fail("ProcessingRetryActions did not reject an empty stitch retry.");
    }
    retry_state.RememberStitch(retry_tiles, 35);
    const ProcessingRetryActionResult stitch_retry_action =
        ProcessingRetryActions::Prepare(retry_state);
    if (!stitch_retry_action.can_start ||
        stitch_retry_action.status != ProcessingRetryActionStatus::Ready ||
        stitch_retry_action.kind != ProcessingJobKind::Stitch ||
        stitch_retry_action.request.stitch_tiles.size() != 1 ||
        stitch_retry_action.request.stitch_search_percent != 35 ||
        stitch_retry_action.message != ProcessingStatusFormatter::FormatRetryStarted(ProcessingJobKind::Stitch)) {
        return Fail("ProcessingRetryActions did not prepare a stitch retry.");
    }
    retry_state.RememberEdf(std::vector<ImageFrame>{MakeSolidImage(1, 1, 3, 3, 3)}, retry_edf_options);
    const ProcessingRetryActionResult invalid_edf_retry_action =
        ProcessingRetryActions::Prepare(retry_state);
    if (invalid_edf_retry_action.can_start ||
        invalid_edf_retry_action.kind != ProcessingJobKind::Edf ||
        invalid_edf_retry_action.message != ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind::Edf)) {
        return Fail("ProcessingRetryActions did not reject an invalid EDF retry.");
    }
    retry_state.RememberEdf(retry_stack, retry_edf_options);
    const ProcessingRetryActionResult edf_retry_action =
        ProcessingRetryActions::Prepare(retry_state);
    if (!edf_retry_action.can_start ||
        edf_retry_action.status != ProcessingRetryActionStatus::Ready ||
        edf_retry_action.kind != ProcessingJobKind::Edf ||
        edf_retry_action.request.edf_stack.size() != 2 ||
        edf_retry_action.request.edf_options.focus_radius != 4 ||
        edf_retry_action.message != ProcessingStatusFormatter::FormatRetryStarted(ProcessingJobKind::Edf)) {
        return Fail("ProcessingRetryActions did not prepare an EDF retry.");
    }

    ProcessingResultFrames panel_focus_frames;
    const ProcessingPanelActionResult missing_composite_action =
        ProcessingPanelActions::ShowEdfCompositeFrame(panel_focus_frames);
    if (missing_composite_action.changed ||
        missing_composite_action.status != ProcessingPanelActionStatus::NoEdfCompositeFrame ||
        missing_composite_action.message != L"Build an EDF image before viewing the EDF image.") {
        return Fail("ProcessingPanelActions did not reject a missing EDF composite frame.");
    }
    const ProcessingPanelActionResult missing_focus_action =
        ProcessingPanelActions::ShowEdfFocusMap(panel_focus_frames);
    if (missing_focus_action.changed ||
        missing_focus_action.status != ProcessingPanelActionStatus::NoEdfFocusMap ||
        missing_focus_action.message != L"Build an EDF image before viewing the focus map.") {
        return Fail("ProcessingPanelActions did not reject a missing EDF focus map.");
    }
    ProcessingJobResult panel_edf_result;
    panel_edf_result.kind = ProcessingJobKind::Edf;
    panel_edf_result.succeeded = true;
    panel_edf_result.image = MakeSolidImage(2, 2, 1, 2, 3);
    panel_edf_result.focus_map = MakeSolidImage(2, 2, 9, 8, 7);
    panel_focus_frames.Apply(std::move(panel_edf_result));
    const ProcessingPanelActionResult shown_focus_action =
        ProcessingPanelActions::ShowEdfFocusMap(panel_focus_frames);
    if (!shown_focus_action.changed ||
        shown_focus_action.status != ProcessingPanelActionStatus::EdfFocusMapShown ||
        shown_focus_action.message != L"EDF focus map ready." ||
        panel_focus_frames.ProcessingResult().bgr[0] != 9 ||
        panel_focus_frames.DisplaySource() != ProcessingResultDisplaySource::EdfFocusMap ||
        panel_focus_frames.DisplaySourceLabel() != L"EDF focus map") {
        return Fail("ProcessingPanelActions did not show an EDF focus map.");
    }
    const ProcessingPanelActionResult shown_composite_action =
        ProcessingPanelActions::ShowEdfCompositeFrame(panel_focus_frames);
    if (!shown_composite_action.changed ||
        shown_composite_action.status != ProcessingPanelActionStatus::EdfCompositeFrameShown ||
        shown_composite_action.message != L"EDF image ready." ||
        panel_focus_frames.ProcessingResult().bgr[0] != 1 ||
        panel_focus_frames.DisplaySource() != ProcessingResultDisplaySource::EdfComposite ||
        panel_focus_frames.DisplaySourceLabel() != L"EDF composite") {
        return Fail("ProcessingPanelActions did not show an EDF composite frame.");
    }

    ProcessingJobState panel_state;
    const ProcessingJobLaunch panel_launch = panel_state.Begin();
    std::vector<StitchTile> panel_tiles(1);
    panel_tiles[0].frame = MakeSolidImage(1, 1, 1, 2, 3);
    std::vector<ImageFrame> panel_stack{
        MakeSolidImage(1, 1, 4, 5, 6),
        MakeSolidImage(1, 1, 7, 8, 9),
    };
    ProcessingRetryState panel_retry;
    panel_retry.RememberEdf(panel_stack, retry_edf_options);
    ProcessingResultFrames panel_clear_frames;
    ProcessingJobResult panel_stitch_result;
    panel_stitch_result.kind = ProcessingJobKind::Stitch;
    panel_stitch_result.succeeded = true;
    panel_stitch_result.image = MakeSolidImage(1, 1, 10, 11, 12);
    panel_clear_frames.Apply(std::move(panel_stitch_result));
    const ProcessingPanelActionResult clear_action = ProcessingPanelActions::Clear(
        panel_state,
        panel_tiles,
        panel_stack,
        panel_retry,
        panel_clear_frames);
    ProcessingJobResult stale_panel_result;
    stale_panel_result.has_result = true;
    stale_panel_result.job_id = panel_launch.job_id;
    if (!clear_action.changed ||
        clear_action.status != ProcessingPanelActionStatus::Cleared ||
        clear_action.message != ProcessingStatusFormatter::FormatCleared(true) ||
        !panel_tiles.empty() ||
        !panel_stack.empty() ||
        panel_retry.Request().CanRetry() ||
        panel_clear_frames.IsProcessingResultVisible() ||
        panel_state.IsCurrentResult(stale_panel_result)) {
        return Fail("ProcessingPanelActions did not clear processing queues safely.");
    }

    auto progress_cancel = std::make_shared<std::atomic_bool>(false);
    ProcessingProgressActions progress_action(ProcessingJobKind::Stitch, progress_cancel);
    const ProcessingProgressActionResult first_progress = progress_action.Report(0);
    if (!first_progress.should_report ||
        first_progress.status != ProcessingProgressActionStatus::Report ||
        first_progress.message != L"Stitch processing 0%.") {
        return Fail("ProcessingProgressActions did not report the initial stitch progress.");
    }
    const ProcessingProgressActionResult suppressed_progress = progress_action.Report(5);
    if (suppressed_progress.should_report ||
        suppressed_progress.status != ProcessingProgressActionStatus::Suppressed ||
        !suppressed_progress.message.empty()) {
        return Fail("ProcessingProgressActions did not suppress throttled progress.");
    }
    const ProcessingProgressActionResult next_progress = progress_action.Report(10);
    if (!next_progress.should_report ||
        next_progress.status != ProcessingProgressActionStatus::Report ||
        next_progress.message != L"Stitch processing 10%.") {
        return Fail("ProcessingProgressActions did not report the next throttled progress.");
    }
    progress_cancel->store(true);
    const ProcessingProgressActionResult canceled_progress = progress_action.Report(20);
    if (canceled_progress.should_report ||
        canceled_progress.status != ProcessingProgressActionStatus::Canceled ||
        !canceled_progress.message.empty()) {
        return Fail("ProcessingProgressActions should ignore progress after cancel.");
    }
    ProcessingProgressActions edf_progress_action(ProcessingJobKind::Edf, nullptr, 5);
    if (edf_progress_action.Report(0).message != L"EDF processing 0%." ||
        edf_progress_action.Report(3).should_report ||
        edf_progress_action.Report(5).message != L"EDF processing 5%.") {
        return Fail("ProcessingProgressActions did not honor custom EDF progress intervals.");
    }

    ProcessingProgressThrottle progress_throttle;
    if (!progress_throttle.ShouldReport(0) ||
        progress_throttle.ShouldReport(5) ||
        !progress_throttle.ShouldReport(10) ||
        progress_throttle.ShouldReport(19) ||
        !progress_throttle.ShouldReport(20) ||
        !progress_throttle.ShouldReport(100)) {
        return Fail("ProcessingProgressThrottle did not report expected 10 percent intervals.");
    }
    ProcessingProgressThrottle five_percent_throttle(5);
    if (!five_percent_throttle.ShouldReport(0) ||
        five_percent_throttle.ShouldReport(4) ||
        !five_percent_throttle.ShouldReport(5)) {
        return Fail("ProcessingProgressThrottle did not honor custom report intervals.");
    }
    ProcessingProgressThrottle minimum_throttle(0);
    if (!minimum_throttle.ShouldReport(0) ||
        minimum_throttle.ShouldReport(0) ||
        !minimum_throttle.ShouldReport(1)) {
        return Fail("ProcessingProgressThrottle did not normalize invalid report intervals.");
    }

    ImageFrame ready_image = MakeSolidImage(12, 8, 1, 2, 3);
    if (ProcessingStatusFormatter::FormatCleared(false) != L"Processing stacks cleared." ||
        ProcessingStatusFormatter::FormatCleared(true) != L"Processing stacks cleared. Running job result will be ignored." ||
        ProcessingStatusFormatter::FormatAlreadyRunning() != L"Processing job is already running." ||
        ProcessingStatusFormatter::FormatNoRetry(ProcessingJobKind::Stitch) != L"No stitch processing job to retry." ||
        ProcessingStatusFormatter::FormatRetryStarted(ProcessingJobKind::Edf) != L"Retrying EDF processing in background." ||
        ProcessingStatusFormatter::FormatStarted(ProcessingJobKind::Stitch) != L"Stitch processing started in background." ||
        ProcessingStatusFormatter::FormatProgress(ProcessingJobKind::Edf, 70) != L"EDF processing 70%." ||
        ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch) != L"Stitch processing canceled." ||
        ProcessingStatusFormatter::FormatFailed(ProcessingJobKind::Edf) != L"Failed to build EDF image." ||
        ProcessingStatusFormatter::FormatReady(ProcessingJobKind::Stitch, ready_image, 3) != L"Stitched image ready: 12x8. Optimized 3 relation(s)." ||
        ProcessingStatusFormatter::FormatReady(ProcessingJobKind::Edf, ready_image) != L"EDF image ready: 12x8.") {
        return Fail("ProcessingStatusFormatter did not format processing status text.");
    }

    DiagnosticReportInput diagnostic_input;
    diagnostic_input.generated = DiagnosticReportTimestamp{2026, 6, 29, 9, 8, 7};
    diagnostic_input.status = L"Preview running.";
    diagnostic_input.viewport_zoom = L"110%";
    diagnostic_input.preview_running = true;
    diagnostic_input.processing_running = false;
    diagnostic_input.sdk.loaded = true;
    diagnostic_input.sdk.loaded_path = L"C:\\MUCam\\MUCam32Ex.dll";
    diagnostic_input.sdk.uses_ex_api = true;
    diagnostic_input.sdk.has_exposure_control = true;
    diagnostic_input.sdk.has_auto_exposure_control = true;
    diagnostic_input.sdk.has_gain_control = true;
    diagnostic_input.sdk.has_white_balance_control = true;
    diagnostic_input.sdk.has_bayer_readout = true;
    diagnostic_input.sdk.has_bayer_to_rgb = true;
    diagnostic_input.sdk.has_bit_depth_control = true;
    diagnostic_input.enumerated_cameras = 3;
    diagnostic_input.selected_camera_index = 1;
    diagnostic_input.enumerated_devices = {
        CameraDevice{0, 10, L"MUCam A"},
        CameraDevice{1, 11, L""},
        CameraDevice{2, 12, L"MUCam C"}};
    diagnostic_input.latest_frame_source = L"Image file: C:\\Samples\\field.bmp";
    diagnostic_input.latest_frame = MakeSolidImage(4, 3, 10, 20, 30);
    diagnostic_input.latest_frame.sequence = 42;
    diagnostic_input.latest_frame.timestamp = 1234;
    diagnostic_input.measurement.calibrated = true;
    diagnostic_input.measurement.microns_per_pixel = 0.5;
    diagnostic_input.measurement.display_unit = MeasurementUnit::Micrometers;
    diagnostic_input.measurement.total_measurements = 4;
    diagnostic_input.measurement.length_measurements = 1;
    diagnostic_input.measurement.angle_measurements = 1;
    diagnostic_input.measurement.rectangle_area_measurements = 1;
    diagnostic_input.measurement.polygon_area_measurements = 1;
    diagnostic_input.image_processing.preview_display_mode = L"Pseudo color: Hot";
    diagnostic_input.image_processing.pseudo_color = PseudoColorPalette::Hot;
    diagnostic_input.image_processing.dye_profiles = 5;
    diagnostic_input.image_processing.fluorescence_channels = 2;
    diagnostic_input.image_processing.stitch_tiles = 6;
    diagnostic_input.image_processing.stitch_search_percent = 12;
    diagnostic_input.image_processing.edf_frames = 7;
    diagnostic_input.image_processing.edf_focus_radius = 3;
    diagnostic_input.image_processing.processing_result_visible = true;
    diagnostic_input.image_processing.processing_result_kind = L"Stitch";
    diagnostic_input.image_processing.processing_result_source = L"Stitch result";
    diagnostic_input.image_processing.processing_result_width = 12;
    diagnostic_input.image_processing.processing_result_height = 8;
    const std::wstring diagnostic_report = DiagnosticReportBuilder::Build(diagnostic_input);
    if (diagnostic_report.find(L"Generated: 2026-06-29 09:08:07") == std::wstring::npos ||
        diagnostic_report.find(L"Preview telemetry: (none)") == std::wstring::npos ||
        diagnostic_report.find(L"Viewport zoom: 110%") == std::wstring::npos ||
        diagnostic_report.find(L"Telemetry: SDK MUCam32Ex.dll | Ex | Exposure | AutoExp | Gain | WB | Bayer | RGB | 8-bit") == std::wstring::npos ||
        diagnostic_report.find(L"Auto exposure: Yes") == std::wstring::npos ||
        diagnostic_report.find(L"Gain control: Yes") == std::wstring::npos ||
        diagnostic_report.find(L"White balance: Yes") == std::wstring::npos ||
        diagnostic_report.find(L"Selected camera index: 2") == std::wstring::npos ||
        diagnostic_report.find(L"Selected camera: Device 2 | type 11") == std::wstring::npos ||
        diagnostic_report.find(L"  3. MUCam C | type 12") == std::wstring::npos ||
        diagnostic_report.find(L"Latest frame source: Image file: C:\\Samples\\field.bmp") == std::wstring::npos ||
        diagnostic_report.find(L"Latest frame size: 4x3") == std::wstring::npos ||
        diagnostic_report.find(L"Microns per pixel: 0.50000000") == std::wstring::npos ||
        diagnostic_report.find(L"Preview display mode: Pseudo color: Hot") == std::wstring::npos ||
        diagnostic_report.find(L"Pseudo color: Hot") == std::wstring::npos ||
        diagnostic_report.find(L"Processing result visible: Yes") == std::wstring::npos ||
        diagnostic_report.find(L"Processing result kind: Stitch") == std::wstring::npos ||
        diagnostic_report.find(L"Processing result source: Stitch result") == std::wstring::npos ||
        diagnostic_report.find(L"Processing result size: 12x8") == std::wstring::npos) {
        return Fail("DiagnosticReportBuilder did not include the expected diagnostic fields.");
    }
    if (DiagnosticReportBuilder::BuildSdkTelemetry(CameraSdkDiagnostics{}) != L"SDK not loaded") {
        return Fail("DiagnosticReportBuilder did not summarize an unloaded SDK correctly.");
    }

    MeasurementCollection diagnostic_measurements;
    diagnostic_measurements.AddLength(L"Diagnostic Length", ImagePoint{0.0, 0.0}, ImagePoint{8.0, 0.0});
    diagnostic_measurements.AddRectangleArea(L"Diagnostic Area", ImagePoint{0.0, 0.0}, ImagePoint{5.0, 4.0});
    DiagnosticReportActionInput action_diagnostic_input;
    action_diagnostic_input.generated = DiagnosticReportTimestamp{2026, 7, 13, 10, 11, 12};
    action_diagnostic_input.status = L"Action report ready.";
    action_diagnostic_input.preview_telemetry = L"Preview telemetry text.";
    action_diagnostic_input.viewport_zoom = L"150%";
    action_diagnostic_input.preview_running = true;
    action_diagnostic_input.processing_running = true;
    action_diagnostic_input.sdk = diagnostic_input.sdk;
    action_diagnostic_input.enumerated_cameras = 2;
    action_diagnostic_input.selected_camera_index = 0;
    action_diagnostic_input.enumerated_devices = {
        CameraDevice{0, 21, L"Inspection Camera"},
        CameraDevice{1, 22, L"Aux Camera"}};
    action_diagnostic_input.latest_frame_source = L"Camera device 1 | type 21";
    action_diagnostic_input.latest_frame = MakeSolidImage(6, 5, 1, 2, 3);
    action_diagnostic_input.calibration = calibration;
    action_diagnostic_input.display_unit = MeasurementUnit::Micrometers;
    action_diagnostic_input.preview_display_mode = L"Processing result: EDF focus map";
    action_diagnostic_input.pseudo_color = PseudoColorPalette::Cyan;
    action_diagnostic_input.dye_profiles = 4;
    action_diagnostic_input.fluorescence_channels = 3;
    action_diagnostic_input.stitch_tiles = 2;
    action_diagnostic_input.stitch_search_percent = 55;
    action_diagnostic_input.edf_frames = 6;
    action_diagnostic_input.edf_focus_radius = 7;
    action_diagnostic_input.processing_result_visible = true;
    action_diagnostic_input.processing_result_kind = L"EDF";
    action_diagnostic_input.processing_result_source = L"EDF focus map";
    action_diagnostic_input.processing_result_width = 9;
    action_diagnostic_input.processing_result_height = 8;
    action_diagnostic_input.edf_composite_available = true;
    action_diagnostic_input.edf_focus_map_available = true;
    const std::wstring action_diagnostic_report =
        DiagnosticReportActions::BuildReport(std::move(action_diagnostic_input), diagnostic_measurements);
    if (action_diagnostic_report.find(L"Generated: 2026-07-13 10:11:12") == std::wstring::npos ||
        action_diagnostic_report.find(L"Preview telemetry: Preview telemetry text.") == std::wstring::npos ||
        action_diagnostic_report.find(L"Viewport zoom: 150%") == std::wstring::npos ||
        action_diagnostic_report.find(L"Preview running: Yes") == std::wstring::npos ||
        action_diagnostic_report.find(L"Processing running: Yes") == std::wstring::npos ||
        action_diagnostic_report.find(L"Selected camera index: 1") == std::wstring::npos ||
        action_diagnostic_report.find(L"Selected camera: Inspection Camera | type 21") == std::wstring::npos ||
        action_diagnostic_report.find(L"  2. Aux Camera | type 22") == std::wstring::npos ||
        action_diagnostic_report.find(L"Latest frame source: Camera device 1 | type 21") == std::wstring::npos ||
        action_diagnostic_report.find(L"Latest frame size: 6x5") == std::wstring::npos ||
        action_diagnostic_report.find(L"Total measurements: 2") == std::wstring::npos ||
        action_diagnostic_report.find(L"Length measurements: 1") == std::wstring::npos ||
        action_diagnostic_report.find(L"Rectangle area measurements: 1") == std::wstring::npos ||
        action_diagnostic_report.find(L"Preview display mode: Processing result: EDF focus map") == std::wstring::npos ||
        action_diagnostic_report.find(L"Pseudo color: Cyan") == std::wstring::npos ||
        action_diagnostic_report.find(L"Dye profiles: 4") == std::wstring::npos ||
        action_diagnostic_report.find(L"EDF focus radius: 7") == std::wstring::npos ||
        action_diagnostic_report.find(L"Processing result kind: EDF") == std::wstring::npos ||
        action_diagnostic_report.find(L"Processing result source: EDF focus map") == std::wstring::npos ||
        action_diagnostic_report.find(L"Processing result size: 9x8") == std::wstring::npos ||
        action_diagnostic_report.find(L"EDF composite available: Yes") == std::wstring::npos ||
        action_diagnostic_report.find(L"EDF focus map available: Yes") == std::wstring::npos) {
        return Fail("DiagnosticReportActions did not build a report from application state.");
    }
    if (DiagnosticReportActions::BuildSdkTelemetry(CameraSdkDiagnostics{}) != L"SDK not loaded") {
        return Fail("DiagnosticReportActions did not summarize SDK telemetry.");
    }

    ViewTransform transform;
    RECT viewport = {0, 0, 400, 300};
    POINT anchor = {200, 150};
    const auto before = transform.ScreenToImage(viewport, 200, 100, anchor);
    if (!before || !Near(before->x, 100.0) || !Near(before->y, 50.0)) {
        return Fail("Initial screen to image conversion is incorrect.");
    }
    const auto outside_drawn_image = transform.ScreenToImage(viewport, 200, 100, POINT{200, 20});
    if (outside_drawn_image) {
        return Fail("Screen to image conversion should reject clicks outside the drawn image.");
    }

    transform.ZoomAt(viewport, 200, 100, anchor, WHEEL_DELTA);
    const auto after_zoom = transform.ScreenToImage(viewport, 200, 100, anchor);
    if (!after_zoom || !Near(after_zoom->x, before->x) || !Near(after_zoom->y, before->y)) {
        return Fail("Zoom did not keep the cursor anchored to the same image point.");
    }

    const POINT before_pan_screen = transform.ImageToScreen(viewport, 200, 100, *before);
    transform.PanBy(viewport, 200, 100, 20, 0);
    const POINT after_pan_screen = transform.ImageToScreen(viewport, 200, 100, *before);
    if (after_pan_screen.x <= before_pan_screen.x) {
        return Fail("Pan did not move the image in the drag direction.");
    }

    ImageFrame viewport_frame;
    viewport_frame.width = 200;
    viewport_frame.height = 100;
    viewport_frame.stride = viewport_frame.width * 3;
    viewport_frame.bgr.assign(static_cast<std::size_t>(viewport_frame.stride * viewport_frame.height), 128);
    ImageViewport viewport_model;
    const auto viewport_before = viewport_model.ScreenToImage(viewport, viewport_frame, anchor);
    if (!viewport_before || !Near(viewport_before->x, 100.0) || !Near(viewport_before->y, 50.0)) {
        return Fail("ImageViewport screen to image conversion is incorrect.");
    }
    viewport_model.ZoomAt(viewport, viewport_frame, anchor, WHEEL_DELTA);
    const auto viewport_after_zoom = viewport_model.ScreenToImage(viewport, viewport_frame, anchor);
    if (!viewport_after_zoom || !Near(viewport_after_zoom->x, viewport_before->x) || !Near(viewport_after_zoom->y, viewport_before->y)) {
        return Fail("ImageViewport zoom did not keep the cursor anchored.");
    }

    ImageViewport interaction_viewport;
    ViewportPanState pan_state;
    POINT outside_anchor = {-1, -1};
    if (ViewportInteractionActions::ZoomAt(interaction_viewport, viewport, ImageFrame{}, anchor, WHEEL_DELTA) ||
        ViewportInteractionActions::ZoomAt(interaction_viewport, viewport, viewport_frame, outside_anchor, WHEEL_DELTA) ||
        ViewportInteractionActions::ZoomAt(interaction_viewport, viewport, viewport_frame, anchor, 0) ||
        !ViewportInteractionActions::ZoomAt(interaction_viewport, viewport, viewport_frame, anchor, WHEEL_DELTA) ||
        interaction_viewport.Zoom() <= 1.0 ||
        ViewportInteractionActions::FormatZoomStatus(1.0) != L"Zoom: 100%." ||
        ViewportInteractionActions::FormatZoomStatus(interaction_viewport.Zoom()) != L"Zoom: 110%." ||
        ViewportInteractionActions::FormatZoomStatus(0.5) != L"Zoom: 50%." ||
        ViewportInteractionActions::FormatZoomValue(1.5) != L"150%") {
        return Fail("ViewportInteractionActions did not gate zoom input correctly.");
    }

    const ViewportPanBeginResult edit_blocked_pan =
        ViewportInteractionActions::BeginPan(pan_state, viewport, viewport_frame, anchor, true);
    const ViewportPanBeginResult outside_pan =
        ViewportInteractionActions::BeginPan(pan_state, viewport, viewport_frame, outside_anchor, false);
    const ViewportPanBeginResult invalid_frame_pan =
        ViewportInteractionActions::BeginPan(pan_state, viewport, ImageFrame{}, anchor, false);
    const ViewportPanBeginResult begun_pan =
        ViewportInteractionActions::BeginPan(pan_state, viewport, viewport_frame, anchor, false);
    POINT invalid_frame_pan_point = {anchor.x + 8, anchor.y};
    const ViewportPanContinueResult invalid_frame_continue =
        ViewportInteractionActions::ContinuePan(pan_state, interaction_viewport, viewport, ImageFrame{}, invalid_frame_pan_point);
    const bool invalid_continue_updated_point = pan_state.last_point.x == invalid_frame_pan_point.x;
    POINT moved_pan_point = {invalid_frame_pan_point.x + 24, invalid_frame_pan_point.y};
    const ViewportPanContinueResult moved_pan =
        ViewportInteractionActions::ContinuePan(pan_state, interaction_viewport, viewport, viewport_frame, moved_pan_point);
    const bool moved_continue_updated_point = pan_state.last_point.x == moved_pan_point.x;
    const bool active_after_continue = pan_state.active;
    const bool ended_pan = ViewportInteractionActions::EndPan(pan_state);
    const ViewportPanContinueResult inactive_continue =
        ViewportInteractionActions::ContinuePan(pan_state, interaction_viewport, viewport, viewport_frame, moved_pan_point);
    if (edit_blocked_pan.started ||
        outside_pan.started ||
        invalid_frame_pan.started ||
        !begun_pan.started ||
        !begun_pan.capture_mouse ||
        !active_after_continue ||
        !invalid_frame_continue.handled ||
        invalid_frame_continue.preview_changed ||
        !invalid_continue_updated_point ||
        !moved_pan.handled ||
        !moved_pan.preview_changed ||
        !moved_continue_updated_point ||
        !ended_pan ||
        pan_state.active ||
        inactive_continue.handled ||
        ViewportInteractionActions::EndPan(pan_state)) {
        return Fail("ViewportInteractionActions did not maintain pan state correctly.");
    }

    ProjectDocument document;
    document.calibration = calibration;
    document.measurements.emplace_back(L"Length, \"A\"", ImagePoint{1.0, 2.0}, ImagePoint{3.0, 4.0});
    document.measurements.emplace_back(L"Length 2", ImagePoint{10.0, 20.0}, ImagePoint{30.0, 40.0});
    document.angle_measurements.emplace_back(L"Angle 1", ImagePoint{1.0, 0.0}, ImagePoint{0.0, 0.0}, ImagePoint{0.0, 1.0});
    document.rectangle_measurements.emplace_back(L"Area 1", ImagePoint{0.0, 0.0}, ImagePoint{20.0, 10.0});
    document.polygon_measurements.emplace_back(
        L"Polygon 1",
        std::vector<ImagePoint>{ImagePoint{0.0, 0.0}, ImagePoint{10.0, 0.0}, ImagePoint{10.0, 10.0}, ImagePoint{0.0, 10.0}});
    document.dye_profiles.push_back(DyeProfile{L"Custom Green", 488.0, 520.0, RgbColor{20, 240, 90}});
    document.processing_settings.edf_focus_radius = 4;
    document.processing_settings.stitch_search_percent = 35;
    FluorescenceChannelRecipe channel_recipe;
    channel_recipe.name = L"FITC 1";
    channel_recipe.color = RgbColor{80, 255, 80};
    channel_recipe.visible = false;
    channel_recipe.black_level = 12;
    channel_recipe.white_level = 220;
    document.fluorescence_channels.push_back(channel_recipe);

    const std::filesystem::path project_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.cvproj";
    std::wstring error;
    if (!ProjectRepository::Save(project_path, document, error)) {
        return Fail("Project save failed.");
    }

    ProjectDocument loaded_document;
    if (!ProjectRepository::Load(project_path, loaded_document, error)) {
        return Fail("Project load failed.");
    }
    std::filesystem::remove(project_path);

    if (!loaded_document.calibration.IsCalibrated() ||
        !Near(loaded_document.calibration.MicronsPerPixel(), calibration.MicronsPerPixel(), 0.0001) ||
        loaded_document.measurements.size() != 2 ||
        loaded_document.angle_measurements.size() != 1 ||
        loaded_document.rectangle_measurements.size() != 1 ||
        loaded_document.polygon_measurements.size() != 1 ||
        loaded_document.dye_profiles.size() != 1 ||
        loaded_document.fluorescence_channels.size() != 1 ||
        loaded_document.measurements[0].Name() != document.measurements[0].Name() ||
        !Near(loaded_document.measurements[1].Second().y, 40.0) ||
        !Near(loaded_document.angle_measurements[0].Degrees(), 90.0) ||
        !Near(loaded_document.rectangle_measurements[0].PixelArea(), 200.0) ||
        !Near(loaded_document.polygon_measurements[0].PixelArea(), 100.0) ||
        loaded_document.polygon_measurements[0].Points().size() != 4 ||
        loaded_document.dye_profiles[0].name != L"Custom Green" ||
        !Near(loaded_document.dye_profiles[0].excitation_nm, 488.0) ||
        !Near(loaded_document.dye_profiles[0].emission_nm, 520.0) ||
        loaded_document.dye_profiles[0].color.r != 20 ||
        loaded_document.dye_profiles[0].color.g != 240 ||
        loaded_document.dye_profiles[0].color.b != 90 ||
        loaded_document.processing_settings.edf_focus_radius != 4 ||
        loaded_document.processing_settings.stitch_search_percent != 35 ||
        loaded_document.fluorescence_channels[0].name != L"FITC 1" ||
        loaded_document.fluorescence_channels[0].visible ||
        loaded_document.fluorescence_channels[0].black_level != 12 ||
        loaded_document.fluorescence_channels[0].white_level != 220 ||
        loaded_document.fluorescence_channels[0].color.r != 80 ||
        loaded_document.fluorescence_channels[0].color.g != 255 ||
        loaded_document.fluorescence_channels[0].color.b != 80) {
        return Fail("Project round trip did not preserve calibration, measurements, and fluorescence settings.");
    }

    MeasurementCollection project_measurements;
    project_measurements.AddLength(L"Mapped Length", ImagePoint{0.0, 0.0}, ImagePoint{8.0, 0.0});
    project_measurements.AddAngle(L"Mapped Angle", ImagePoint{1.0, 0.0}, ImagePoint{0.0, 0.0}, ImagePoint{0.0, 1.0});
    FluorescenceChannel mapped_channel;
    mapped_channel.name = L"Mapped Channel";
    mapped_channel.color = RgbColor{9, 8, 7};
    mapped_channel.visible = false;
    mapped_channel.black_level = 11;
    mapped_channel.white_level = 222;
    mapped_channel.frame = MakeSolidImage(2, 2, 1, 2, 3);
    EdfOptions mapped_edf_options;
    mapped_edf_options.focus_radius = 5;
    const ProjectDocument mapped_document = ProjectSessionMapper::ToDocument(
        calibration,
        project_measurements,
        document.dye_profiles,
        std::vector<FluorescenceChannel>{mapped_channel},
        mapped_edf_options,
        44);
    if (mapped_document.measurements.size() != 1 ||
        mapped_document.angle_measurements.size() != 1 ||
        mapped_document.fluorescence_channels.size() != 1 ||
        mapped_document.fluorescence_channels[0].name != L"Mapped Channel" ||
        mapped_document.fluorescence_channels[0].visible ||
        mapped_document.fluorescence_channels[0].black_level != 11 ||
        mapped_document.processing_settings.edf_focus_radius != 5 ||
        mapped_document.processing_settings.stitch_search_percent != 44) {
        return Fail("ProjectSessionMapper did not build a document from session state.");
    }

    const std::filesystem::path action_project_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.cvproj";
    const ProjectActionResult project_save = ProjectActions::SaveProject(
        action_project_path,
        calibration,
        project_measurements,
        document.dye_profiles,
        std::vector<FluorescenceChannel>{mapped_channel},
        mapped_edf_options,
        44);
    if (!project_save.succeeded ||
        project_save.status != ProjectActionStatus::Saved ||
        project_save.message != L"Project saved." ||
        !std::filesystem::exists(action_project_path)) {
        return Fail("ProjectActions did not save a project file.");
    }
    ProjectActionResult project_load = ProjectActions::LoadProject(action_project_path);
    std::filesystem::remove(action_project_path);
    if (!project_load.succeeded ||
        project_load.status != ProjectActionStatus::Loaded ||
        project_load.session_state.measurements.LengthCount() != 1 ||
        project_load.session_state.measurements.AngleCount() != 1 ||
        project_load.session_state.dye_profiles.size() != 1 ||
        project_load.session_state.fluorescence_channels.size() != 1 ||
        project_load.session_state.fluorescence_channels[0].name != L"Mapped Channel" ||
        project_load.session_state.fluorescence_channels[0].visible ||
        project_load.session_state.edf_options.focus_radius != 5 ||
        project_load.session_state.stitch_search_percent != 44 ||
        !project_load.session_state.restored_channel_settings) {
        return Fail("ProjectActions did not load a project into session state.");
    }

    ProjectDocument sparse_document;
    sparse_document.measurements.emplace_back(L"Open Length", ImagePoint{0.0, 0.0}, ImagePoint{5.0, 0.0});
    sparse_document.processing_settings.edf_focus_radius = 99;
    sparse_document.processing_settings.stitch_search_percent = 1;
    sparse_document.fluorescence_channels.push_back(channel_recipe);
    ProjectSessionState mapped_state = ProjectSessionMapper::FromDocument(std::move(sparse_document));
    if (mapped_state.measurements.LengthCount() != 1 ||
        mapped_state.dye_profiles.empty() ||
        mapped_state.edf_options.focus_radius != 16 ||
        mapped_state.stitch_search_percent != 5 ||
        !mapped_state.restored_channel_settings ||
        mapped_state.fluorescence_channels.size() != 1 ||
        mapped_state.fluorescence_channels[0].frame.IsValid() ||
        mapped_state.fluorescence_channels[0].black_level != 12 ||
        mapped_state.fluorescence_channels[0].white_level != 220) {
        return Fail("ProjectSessionMapper did not restore normalized session state from a document.");
    }

    CalibrationProfile runtime_calibration = CalibrationProfile::Uncalibrated();
    MeasurementCollection runtime_measurements;
    std::vector<DyeProfile> runtime_dyes = DyeLibrary::DefaultDyes();
    std::vector<FluorescenceChannel> runtime_channels;
    std::vector<StitchTile> runtime_stitch_tiles(1);
    runtime_stitch_tiles[0].frame = MakeSolidImage(1, 1, 1, 1, 1);
    std::vector<ImageFrame> runtime_edf_stack = {
        MakeSolidImage(1, 1, 1, 1, 1),
        MakeSolidImage(1, 1, 2, 2, 2)
    };
    EdfOptions runtime_edf_options;
    runtime_edf_options.focus_radius = 2;
    int runtime_stitch_search_percent = 77;
    bool runtime_show_fusion_preview = true;
    ProcessingRetryState runtime_retry;
    runtime_retry.RememberEdf(runtime_edf_stack, runtime_edf_options);
    ProcessingResultFrames runtime_processing_frames;
    ProcessingJobResult runtime_processing_result;
    runtime_processing_result.kind = ProcessingJobKind::Stitch;
    runtime_processing_result.succeeded = true;
    runtime_processing_result.image = MakeSolidImage(1, 1, 3, 3, 3);
    runtime_processing_frames.Apply(std::move(runtime_processing_result));

    ProjectSessionState restore_state = std::move(mapped_state);
    const ProjectSessionRestoreResult restore_result = ProjectSessionRestorer::Restore(
        ProjectRuntimeState{
            runtime_calibration,
            runtime_measurements,
            runtime_dyes,
            runtime_channels,
            runtime_stitch_tiles,
            runtime_edf_stack,
            runtime_edf_options,
            runtime_stitch_search_percent,
            runtime_show_fusion_preview,
            runtime_retry,
            runtime_processing_frames
        },
        std::move(restore_state));
    if (!restore_result.restored_channel_settings ||
        restore_result.status.find(L"Fluorescence channel settings restored") == std::wstring::npos ||
        runtime_measurements.LengthCount() != 1 ||
        runtime_dyes.empty() ||
        runtime_channels.size() != 1 ||
        runtime_edf_options.focus_radius != 16 ||
        runtime_stitch_search_percent != 5 ||
        runtime_show_fusion_preview ||
        !runtime_stitch_tiles.empty() ||
        !runtime_edf_stack.empty() ||
        runtime_retry.Request().CanRetry() ||
        runtime_processing_frames.IsProcessingResultVisible()) {
        return Fail("ProjectSessionRestorer did not restore project state and clear runtime processing state.");
    }

    EdfOptions invalid_edf_options;
    invalid_edf_options.focus_radius = 99;
    const EdfOptions normalized_edf_options =
        ProcessingParameterRules::NormalizeEdfOptions(invalid_edf_options);
    const StitchSearchRadius registration_radius =
        ProcessingParameterRules::RegistrationSearchRadius(200, 50, 10);
    const StitchSearchRadius clamped_registration_radius =
        ProcessingParameterRules::RegistrationSearchRadius(200, 50, 1);
    std::vector<StitchTile> optimization_tiles(2);
    optimization_tiles[0].frame = MakeSolidImage(2000, 100, 0, 0, 0);
    optimization_tiles[1].frame = MakeSolidImage(20, 20, 0, 0, 0);
    const StitchOptimizationOptions rule_optimization_options =
        ProcessingParameterRules::StitchOptimizationOptionsFor(optimization_tiles, 100);
    const StitchOptimizationOptions small_rule_optimization_options =
        ProcessingParameterRules::StitchOptimizationOptionsFor(optimization_tiles, 1);
    if (ProcessingParameterRules::MinStitchSearchPercent() != 5 ||
        ProcessingParameterRules::MaxStitchSearchPercent() != 100 ||
        ProcessingParameterRules::DefaultStitchSearchPercent() != 85 ||
        !ProcessingParameterRules::IsValidStitchSearchPercent(5) ||
        !ProcessingParameterRules::IsValidStitchSearchPercent(100) ||
        ProcessingParameterRules::IsValidStitchSearchPercent(4) ||
        ProcessingParameterRules::ClampStitchSearchPercent(1) != 5 ||
        ProcessingParameterRules::ClampStitchSearchPercent(200) != 100 ||
        ProcessingParameterRules::MinEdfFocusRadius() != 1 ||
        ProcessingParameterRules::MaxEdfFocusRadius() != 16 ||
        !ProcessingParameterRules::IsValidEdfFocusRadius(16) ||
        ProcessingParameterRules::IsValidEdfFocusRadius(17) ||
        normalized_edf_options.focus_radius != 16 ||
        registration_radius.x != 20 ||
        registration_radius.y != 8 ||
        clamped_registration_radius.x != 10 ||
        clamped_registration_radius.y != 8 ||
        rule_optimization_options.search_radius_x != 128 ||
        rule_optimization_options.search_radius_y != 100 ||
        rule_optimization_options.iterations != 30 ||
        small_rule_optimization_options.search_radius_x != 100 ||
        small_rule_optimization_options.search_radius_y != 5) {
        return Fail("ProcessingParameterRules did not normalize processing parameters.");
    }

    ImageFrame image;
    image.width = 16;
    image.height = 16;
    image.stride = (image.width * 3 + 3) & ~3;
    image.bgr.assign(static_cast<std::size_t>(image.stride) * static_cast<std::size_t>(image.height), 0);
    image.bgr[0] = 64;
    image.bgr[1] = 64;
    image.bgr[2] = 64;
    const std::vector<PseudoColorPalette>& palette_options = PseudoColorMapper::PaletteOptions();
    if (palette_options.size() != 6 ||
        palette_options[0] != PseudoColorPalette::Original ||
        PseudoColorMapper::PaletteAtIndex(2) != PseudoColorPalette::Hot ||
        PseudoColorMapper::PaletteAtIndex(-1) != PseudoColorPalette::Original ||
        PseudoColorMapper::PaletteAtIndex(99) != PseudoColorPalette::Original) {
        return Fail("PseudoColorMapper did not expose stable palette options.");
    }
    const ImageFrame hot_image = PseudoColorMapper::Apply(image, PseudoColorPalette::Hot);
    if (!hot_image.IsValid() || hot_image.bgr.size() != image.bgr.size() || image.bgr[2] != 64 || hot_image.bgr[2] <= hot_image.bgr[0]) {
        return Fail("Pseudo color mapping did not produce the expected hot palette output.");
    }
    const std::vector<std::wstring> pseudo_labels = PreviewDisplayActions::PseudoColorLabels();
    const PseudoColorSelectionResult pseudo_selection = PreviewDisplayActions::SelectPseudoColor(2);
    const PseudoColorSelectionResult invalid_pseudo_selection = PreviewDisplayActions::SelectPseudoColor(99);
    if (pseudo_labels.size() != palette_options.size() ||
        pseudo_labels[2] != L"Hot" ||
        pseudo_selection.palette != PseudoColorPalette::Hot ||
        pseudo_selection.message != L"Pseudo color: Hot" ||
        invalid_pseudo_selection.palette != PseudoColorPalette::Original ||
        invalid_pseudo_selection.message != L"Pseudo color: Original") {
        return Fail("PreviewDisplayActions did not expose pseudo color selection state.");
    }

    const std::vector<DyeProfile> dyes = DyeLibrary::DefaultDyes();
    if (dyes.size() < 4 || dyes[0].name != L"DAPI" || DyeLibrary::FindByName(dyes, L"FITC") == nullptr) {
        return Fail("Default fluorescence dye library is incomplete.");
    }
    if (DyeLibrary::FallbackDye().name != L"Channel" ||
        !DyeLibrary::IndexAtSelection(0, dyes.size()) ||
        DyeLibrary::IndexAtSelection(-1, dyes.size()) ||
        DyeLibrary::IndexAtSelection(static_cast<int>(dyes.size()), dyes.size())) {
        return Fail("DyeLibrary did not expose stable fallback dye and selection mapping.");
    }
    std::vector<DyeProfile> editable_dyes = dyes;
    const DyeLibraryUpdateResult updated_dye = DyeLibrary::UpsertByName(
        editable_dyes,
        DyeProfile{L"FITC", 500.0, 530.0, RgbColor{1, 2, 3}});
    const DyeLibraryUpdateResult added_dye = DyeLibrary::UpsertByName(
        editable_dyes,
        DyeProfile{L"Custom", 610.0, 650.0, RgbColor{4, 5, 6}});
    if (!updated_dye.updated_existing ||
        updated_dye.index != 1 ||
        editable_dyes[1].emission_nm != 530.0 ||
        added_dye.updated_existing ||
        added_dye.index != editable_dyes.size() - 1 ||
        editable_dyes.back().name != L"Custom") {
        return Fail("DyeLibrary did not upsert dyes by name.");
    }
    const DyeLibraryDeleteResult deleted_dye = DyeLibrary::DeleteAt(editable_dyes, 1);
    const DyeLibraryDeleteResult invalid_delete = DyeLibrary::DeleteAt(editable_dyes, editable_dyes.size());
    if (!deleted_dye.deleted ||
        deleted_dye.removed.name != L"FITC" ||
        !deleted_dye.next_index ||
        *deleted_dye.next_index != 1 ||
        invalid_delete.deleted) {
        return Fail("DyeLibrary did not delete dyes with the expected next selection.");
    }
    const DyeProfileInputResult missing_dye_name = DyeProfileFormParser::Parse(
        DyeProfileInput{L"  ", L"358", L"461", L"80", L"120", L"255"});
    const DyeProfileInputResult invalid_dye_wavelength = DyeProfileFormParser::Parse(
        DyeProfileInput{L"DAPI", L"-1", L"461", L"80", L"120", L"255"});
    const DyeProfileInputResult invalid_dye_color = DyeProfileFormParser::Parse(
        DyeProfileInput{L"DAPI", L"358", L"461", L"80", L"256", L"255"});
    const DyeProfileInputResult valid_dye_input = DyeProfileFormParser::Parse(
        DyeProfileInput{L"  Custom Dye  ", L"405.5 nm", L"520.25", L"1", L"2", L"3"});
    if (missing_dye_name.status != DyeProfileInputStatus::MissingName ||
        missing_dye_name.IsValid() ||
        missing_dye_name.message != L"Dye name is required." ||
        invalid_dye_wavelength.status != DyeProfileInputStatus::InvalidWavelengths ||
        invalid_dye_wavelength.IsValid() ||
        invalid_dye_color.status != DyeProfileInputStatus::InvalidColor ||
        invalid_dye_color.IsValid() ||
        !valid_dye_input.IsValid() ||
        valid_dye_input.status != DyeProfileInputStatus::Valid ||
        valid_dye_input.dye->name != L"Custom Dye" ||
        !Near(valid_dye_input.dye->excitation_nm, 405.5) ||
        !Near(valid_dye_input.dye->emission_nm, 520.25) ||
        valid_dye_input.dye->color.r != 1 ||
        valid_dye_input.dye->color.g != 2 ||
        valid_dye_input.dye->color.b != 3) {
        return Fail("DyeProfileFormParser did not validate dye input fields.");
    }

    const DyeProfileFormValues empty_dye_form = DyeProfileFormPresenter::Empty();
    const DyeProfileFormValues dye_form = DyeProfileFormPresenter::FromDye(dyes[0]);
    if (empty_dye_form.name != L"" ||
        empty_dye_form.excitation_nm != L"0" ||
        empty_dye_form.emission_nm != L"0" ||
        empty_dye_form.red != L"255" ||
        dye_form.name != L"DAPI" ||
        dye_form.excitation_nm != L"358" ||
        dye_form.emission_nm != L"461" ||
        dye_form.red != L"80" ||
        dye_form.green != L"120" ||
        dye_form.blue != L"255") {
        return Fail("DyeProfileFormPresenter did not prepare stable dye form values.");
    }

    std::vector<DyeProfile> dye_action_dyes = DyeLibrary::DefaultDyes();
    const DyeLibraryActionResult invalid_dye_save =
        DyeLibraryActions::Save(dye_action_dyes, DyeProfileInput{L" ", L"358", L"461", L"80", L"120", L"255"});
    const DyeLibraryActionResult saved_dye_action =
        DyeLibraryActions::Save(dye_action_dyes, DyeProfileInput{L"  Action Dye  ", L"410", L"520", L"9", L"8", L"7"});
    if (invalid_dye_save.changed ||
        invalid_dye_save.status != DyeLibraryActionStatus::InvalidInput ||
        invalid_dye_save.message != L"Dye name is required." ||
        !saved_dye_action.changed ||
        saved_dye_action.status != DyeLibraryActionStatus::Saved ||
        !saved_dye_action.selected_index ||
        *saved_dye_action.selected_index != dye_action_dyes.size() - 1 ||
        dye_action_dyes[*saved_dye_action.selected_index].name != L"Action Dye" ||
        dye_action_dyes[*saved_dye_action.selected_index].color.r != 9 ||
        saved_dye_action.message != L"Dye saved: Action Dye.") {
        return Fail("DyeLibraryActions did not save dye profiles safely.");
    }

    const std::size_t action_dye_index = *saved_dye_action.selected_index;
    const DyeLibraryActionResult updated_dye_action =
        DyeLibraryActions::Save(dye_action_dyes, DyeProfileInput{L"Action Dye", L"411", L"521", L"1", L"2", L"3"});
    const DyeLibraryActionResult no_selection_dye_delete =
        DyeLibraryActions::DeleteSelected(dye_action_dyes, -1);
    const DyeLibraryActionResult deleted_dye_action =
        DyeLibraryActions::DeleteSelected(dye_action_dyes, static_cast<int>(action_dye_index));
    if (!updated_dye_action.changed ||
        updated_dye_action.status != DyeLibraryActionStatus::Saved ||
        !updated_dye_action.selected_index ||
        *updated_dye_action.selected_index != action_dye_index ||
        dye_action_dyes[action_dye_index].color.r != 1 ||
        no_selection_dye_delete.changed ||
        no_selection_dye_delete.status != DyeLibraryActionStatus::NoSelection ||
        no_selection_dye_delete.message != L"Select a dye first." ||
        !deleted_dye_action.changed ||
        deleted_dye_action.status != DyeLibraryActionStatus::Deleted ||
        !deleted_dye_action.dye ||
        deleted_dye_action.dye->name != L"Action Dye" ||
        !deleted_dye_action.selected_index ||
        *deleted_dye_action.selected_index != dye_action_dyes.size() - 1 ||
        deleted_dye_action.message != L"Dye deleted: Action Dye.") {
        return Fail("DyeLibraryActions did not update or delete dye profiles safely.");
    }

    if (FluorescenceFormatter::FormatDyeLabel(dyes[0]) != L"DAPI  358/461 nm") {
        return Fail("FluorescenceFormatter did not format the dye label.");
    }
    const std::vector<std::wstring> dye_display_labels =
        FluorescenceDisplayActions::DyeLabels(dyes);
    if (dye_display_labels.size() != dyes.size() ||
        dye_display_labels[0] != L"DAPI  358/461 nm" ||
        FluorescenceDisplayActions::SelectedDye(dyes, 1).name != L"FITC" ||
        FluorescenceDisplayActions::SelectedDye(dyes, -1).name != L"DAPI" ||
        FluorescenceDisplayActions::SelectedDye(std::vector<DyeProfile>{}, 0).name != L"Channel" ||
        !FluorescenceDisplayActions::SelectedDyeIndex(dyes, 0) ||
        FluorescenceDisplayActions::SelectedDyeIndex(dyes, -1) ||
        FluorescenceDisplayActions::SelectedDyeIndex(dyes, static_cast<int>(dyes.size()))) {
        return Fail("FluorescenceDisplayActions did not expose dye display selections.");
    }
    if (FluorescenceFormatter::FormatDefaultChannelName(dyes[1], 4) != L"FITC 4") {
        return Fail("FluorescenceFormatter did not format a default channel name.");
    }
    const FluorescenceChannel factory_channel =
        FluorescenceChannelFactory::CreateFromFrame(dyes[1], MakeSolidImage(2, 1, 10, 20, 30), 4);
    if (factory_channel.name != L"FITC 4" ||
        factory_channel.color.r != dyes[1].color.r ||
        factory_channel.color.g != dyes[1].color.g ||
        factory_channel.color.b != dyes[1].color.b ||
        !factory_channel.visible ||
        factory_channel.black_level != 0 ||
        factory_channel.white_level != 255 ||
        !factory_channel.frame.IsValid() ||
        factory_channel.frame.width != 2 ||
        factory_channel.frame.height != 1) {
        return Fail("FluorescenceChannelFactory did not create a default visible channel from a frame.");
    }

    std::vector<FluorescenceChannel> channel_action_channels;
    const FluorescenceChannelListActionResult missing_frame_channel_action =
        FluorescenceChannelListActions::AddCurrentFrame(channel_action_channels, ImageFrame{}, dyes[0]);
    if (missing_frame_channel_action.changed ||
        missing_frame_channel_action.status != FluorescenceChannelListActionStatus::NoFrame ||
        !channel_action_channels.empty() ||
        missing_frame_channel_action.message != L"No image frame to add as a fluorescence channel.") {
        return Fail("FluorescenceChannelListActions accepted an empty frame.");
    }
    const FluorescenceChannelListActionResult added_channel_action =
        FluorescenceChannelListActions::AddCurrentFrame(
            channel_action_channels,
            MakeSolidImage(2, 1, 10, 20, 30),
            dyes[0]);
    if (!added_channel_action.changed ||
        added_channel_action.status != FluorescenceChannelListActionStatus::Added ||
        !added_channel_action.show_fusion_preview ||
        !added_channel_action.selected_index ||
        *added_channel_action.selected_index != 0 ||
        channel_action_channels.size() != 1 ||
        channel_action_channels[0].name != L"DAPI 1" ||
        added_channel_action.message != L"Added fluorescence channel: DAPI.") {
        return Fail("FluorescenceChannelListActions did not add a channel safely.");
    }
    const FluorescenceChannelListActionResult cleared_channel_action =
        FluorescenceChannelListActions::Clear(channel_action_channels);
    if (!cleared_channel_action.changed ||
        cleared_channel_action.status != FluorescenceChannelListActionStatus::Cleared ||
        cleared_channel_action.show_fusion_preview ||
        !channel_action_channels.empty() ||
        cleared_channel_action.message != L"Fluorescence channels cleared.") {
        return Fail("FluorescenceChannelListActions did not clear channels safely.");
    }

    const FluorescenceChannelDisplaySettings default_channel_settings =
        FluorescenceChannelSettings::Defaults();
    const FluorescenceChannelDisplaySettings empty_channel_settings =
        FluorescenceChannelSettings::Defaults(false);
    const std::optional<FluorescenceChannelDisplaySettings> valid_channel_settings =
        FluorescenceChannelSettings::FromLevels(false, 12, 220);
    if (!default_channel_settings.visible ||
        default_channel_settings.black_level != 0 ||
        default_channel_settings.white_level != 255 ||
        empty_channel_settings.visible ||
        !FluorescenceChannelSettings::IndexAtSelection(0, 2) ||
        FluorescenceChannelSettings::IndexAtSelection(-1, 2) ||
        FluorescenceChannelSettings::IndexAtSelection(2, 2) ||
        !valid_channel_settings ||
        valid_channel_settings->visible ||
        valid_channel_settings->black_level != 12 ||
        valid_channel_settings->white_level != 220 ||
        FluorescenceChannelSettings::FromLevels(true, -1, 220) ||
        FluorescenceChannelSettings::FromLevels(true, 12, 256) ||
        FluorescenceChannelSettings::FromLevels(true, 220, 220) ||
        FluorescenceChannelSettings::FromLevels(true, 221, 220)) {
        return Fail("FluorescenceChannelSettings did not validate display settings.");
    }

    ImageFrame channel_image;
    channel_image.width = 2;
    channel_image.height = 1;
    channel_image.stride = (channel_image.width * 3 + 3) & ~3;
    channel_image.bgr.assign(static_cast<std::size_t>(channel_image.stride) * static_cast<std::size_t>(channel_image.height), 0);
    channel_image.bgr[0] = 100;
    channel_image.bgr[1] = 100;
    channel_image.bgr[2] = 100;
    channel_image.bgr[3] = 220;
    channel_image.bgr[4] = 220;
    channel_image.bgr[5] = 220;

    FluorescenceChannel green_channel;
    green_channel.name = L"FITC";
    green_channel.frame = channel_image;
    green_channel.color = RgbColor{0, 255, 0};
    FluorescenceChannelSettings::Apply(green_channel, *valid_channel_settings);
    const FluorescenceChannelDisplaySettings copied_channel_settings =
        FluorescenceChannelSettings::FromChannel(green_channel);
    if (green_channel.visible ||
        green_channel.black_level != 12 ||
        green_channel.white_level != 220 ||
        copied_channel_settings.visible ||
        copied_channel_settings.black_level != 12 ||
        copied_channel_settings.white_level != 220) {
        return Fail("FluorescenceChannelSettings did not apply or read channel settings.");
    }
    FluorescenceChannelSettings::Apply(green_channel, FluorescenceChannelSettings::Defaults());
    if (FluorescenceFormatter::FormatChannelLine(green_channel) != L"[on] FITC  2x1  0-255") {
        return Fail("FluorescenceFormatter did not format a visible channel with a frame.");
    }
    const std::vector<FluorescenceChannel> display_channels{green_channel};
    const std::vector<std::wstring> channel_display_lines =
        FluorescenceDisplayActions::ChannelLines(display_channels);
    if (channel_display_lines.size() != 1 ||
        channel_display_lines[0] != L"[on] FITC  2x1  0-255" ||
        !FluorescenceDisplayActions::SelectedChannelIndex(display_channels, 0) ||
        FluorescenceDisplayActions::SelectedChannelIndex(display_channels, -1) ||
        FluorescenceDisplayActions::SelectedChannelIndex(display_channels, 1)) {
        return Fail("FluorescenceDisplayActions did not expose channel display selections.");
    }
    const FluorescenceChannelFormValues empty_channel_form = FluorescenceChannelFormPresenter::Empty();
    const FluorescenceChannelFormValues green_channel_form = FluorescenceChannelFormPresenter::FromChannel(green_channel);
    if (empty_channel_form.visible ||
        empty_channel_form.black_level != L"0" ||
        empty_channel_form.white_level != L"255" ||
        !green_channel_form.visible ||
        green_channel_form.black_level != L"0" ||
        green_channel_form.white_level != L"255") {
        return Fail("FluorescenceChannelFormPresenter did not prepare stable channel form values.");
    }

    std::vector<FluorescenceChannel> editable_channels{green_channel};
    const FluorescenceChannelUpdateResult no_channel_update =
        FluorescenceChannelUpdater::Apply(editable_channels, -1, false, 10, 200);
    const FluorescenceChannelUpdateResult out_of_bounds_update =
        FluorescenceChannelUpdater::Apply(editable_channels, 0, false, -1, 200);
    const FluorescenceChannelUpdateResult invalid_order_update =
        FluorescenceChannelUpdater::Apply(editable_channels, 0, false, 200, 200);
    const FluorescenceChannelUpdateResult applied_channel_update =
        FluorescenceChannelUpdater::Apply(editable_channels, 0, false, 20, 180);
    if (no_channel_update.applied ||
        no_channel_update.status != FluorescenceChannelUpdateStatus::NoSelection ||
        out_of_bounds_update.applied ||
        out_of_bounds_update.status != FluorescenceChannelUpdateStatus::RangeOutOfBounds ||
        invalid_order_update.applied ||
        invalid_order_update.status != FluorescenceChannelUpdateStatus::InvalidRangeOrder ||
        !applied_channel_update.applied ||
        applied_channel_update.status != FluorescenceChannelUpdateStatus::Applied ||
        !applied_channel_update.index ||
        *applied_channel_update.index != 0 ||
        editable_channels[0].visible ||
        editable_channels[0].black_level != 20 ||
        editable_channels[0].white_level != 180 ||
        applied_channel_update.message != L"Fluorescence channel updated.") {
        return Fail("FluorescenceChannelUpdater did not apply channel display settings safely.");
    }

    FluorescenceChannel magenta_channel;
    magenta_channel.name = L"Cy5";
    magenta_channel.frame = channel_image;
    magenta_channel.color = RgbColor{255, 0, 255};
    magenta_channel.black_level = 50;
    magenta_channel.white_level = 200;
    magenta_channel.visible = false;
    if (FluorescenceFormatter::FormatChannelLine(magenta_channel) != L"[off] Cy5  2x1  50-200") {
        return Fail("FluorescenceFormatter did not format a hidden channel with a frame.");
    }
    magenta_channel.visible = true;

    const ImageFrame fused_image = ChannelFusionEngine::Fuse({green_channel, magenta_channel});
    if (!fused_image.IsValid() || fused_image.width != 2 || fused_image.height != 1) {
        return Fail("Fluorescence fusion did not produce a valid image.");
    }
    if (fused_image.bgr[0] == 0 || fused_image.bgr[1] == 0 || fused_image.bgr[2] == 0) {
        return Fail("Fluorescence fusion did not combine channel colors.");
    }
    if (channel_image.bgr[0] != 100 || channel_image.bgr[3] != 220) {
        return Fail("Fluorescence fusion modified the source image.");
    }

    magenta_channel.visible = false;
    const ImageFrame visible_only_image = ChannelFusionEngine::Fuse({green_channel, magenta_channel});
    if (!visible_only_image.IsValid() ||
        visible_only_image.bgr[0] != 0 ||
        visible_only_image.bgr[1] == 0 ||
        visible_only_image.bgr[2] != 0) {
        return Fail("Invisible fluorescence channels should not contribute to fusion.");
    }

    FluorescenceChannel ranged_channel;
    ranged_channel.name = L"Range";
    ranged_channel.frame = channel_image;
    ranged_channel.color = RgbColor{255, 0, 0};
    ranged_channel.black_level = 50;
    ranged_channel.white_level = 150;
    FluorescenceChannel empty_channel;
    empty_channel.name = L"Empty";
    empty_channel.visible = false;
    empty_channel.black_level = 12;
    empty_channel.white_level = 220;
    if (FluorescenceFormatter::FormatChannelLine(empty_channel) != L"[off] Empty  no frame  12-220") {
        return Fail("FluorescenceFormatter did not format a channel without a frame.");
    }
    const ImageFrame ranged_image = ChannelFusionEngine::Fuse({ranged_channel});
    if (!ranged_image.IsValid() || ranged_image.bgr[2] != 128 || ranged_image.bgr[5] != 255) {
        return Fail("Fluorescence channel black/white range did not scale intensity as expected.");
    }

    ImageFrame processing_override = MakeSolidImage(2, 1, 9, 8, 7);
    PreviewFrameComposition processing_composition;
    processing_composition.source = &image;
    processing_composition.processing_result = &processing_override;
    processing_composition.fluorescence_channels = nullptr;
    processing_composition.show_processing_result = true;
    processing_composition.show_fusion_preview = true;
    processing_composition.pseudo_color_palette = PseudoColorPalette::Hot;
    const ImageFrame processing_preview = PreviewFrameComposer::Compose(processing_composition);
    if (!processing_preview.IsValid() || processing_preview.bgr[0] != 9 || processing_preview.bgr[2] != 7) {
        return Fail("PreviewFrameComposer did not prioritize processing results.");
    }

    std::vector<FluorescenceChannel> composer_channels = {green_channel};
    PreviewFrameComposition fusion_composition;
    fusion_composition.source = &image;
    fusion_composition.fluorescence_channels = &composer_channels;
    fusion_composition.show_fusion_preview = true;
    fusion_composition.pseudo_color_palette = PseudoColorPalette::Hot;
    const ImageFrame fusion_preview = PreviewFrameComposer::Compose(fusion_composition);
    if (!fusion_preview.IsValid() || fusion_preview.width != channel_image.width || fusion_preview.bgr[1] == 0 || fusion_preview.bgr[2] != 0) {
        return Fail("PreviewFrameComposer did not prioritize fluorescence fusion over pseudo color.");
    }

    PreviewFrameComposition pseudo_composition;
    pseudo_composition.source = &image;
    pseudo_composition.pseudo_color_palette = PseudoColorPalette::Hot;
    const ImageFrame pseudo_preview = PreviewFrameComposer::Compose(pseudo_composition);
    if (!pseudo_preview.IsValid() || pseudo_preview.bgr[2] <= pseudo_preview.bgr[0]) {
        return Fail("PreviewFrameComposer did not fall back to pseudo color.");
    }
    ProcessingResultFrames preview_action_frames;
    ProcessingJobResult preview_action_result;
    preview_action_result.kind = ProcessingJobKind::Stitch;
    preview_action_result.succeeded = true;
    preview_action_result.image = processing_override;
    preview_action_frames.Apply(std::move(preview_action_result));
    const ImageFrame action_processing_preview = PreviewDisplayActions::BuildPreviewFrame(
        image,
        preview_action_frames,
        composer_channels,
        true,
        PseudoColorPalette::Hot);
    ProcessingResultFrames empty_preview_action_frames;
    const ImageFrame action_fusion_preview = PreviewDisplayActions::BuildPreviewFrame(
        image,
        empty_preview_action_frames,
        composer_channels,
        true,
        PseudoColorPalette::Hot);
    const ImageFrame action_pseudo_preview = PreviewDisplayActions::BuildPreviewFrame(
        image,
        empty_preview_action_frames,
        composer_channels,
        false,
        PseudoColorPalette::Hot);
    const std::wstring processing_preview_mode = PreviewDisplayActions::PreviewModeLabel(
        image,
        preview_action_frames,
        composer_channels,
        true,
        PseudoColorPalette::Hot);
    const std::wstring fusion_preview_mode = PreviewDisplayActions::PreviewModeLabel(
        image,
        empty_preview_action_frames,
        composer_channels,
        true,
        PseudoColorPalette::Hot);
    const std::wstring pseudo_preview_mode = PreviewDisplayActions::PreviewModeLabel(
        image,
        empty_preview_action_frames,
        composer_channels,
        false,
        PseudoColorPalette::Hot);
    const std::wstring original_preview_mode = PreviewDisplayActions::PreviewModeLabel(
        image,
        empty_preview_action_frames,
        composer_channels,
        false,
        PseudoColorPalette::Original);
    const std::wstring empty_preview_mode = PreviewDisplayActions::PreviewModeLabel(
        ImageFrame{},
        empty_preview_action_frames,
        composer_channels,
        false,
        PseudoColorPalette::Original);
    if (!action_processing_preview.IsValid() ||
        action_processing_preview.bgr[0] != 9 ||
        !action_fusion_preview.IsValid() ||
        action_fusion_preview.bgr[1] == 0 ||
        !action_pseudo_preview.IsValid() ||
        action_pseudo_preview.bgr[2] <= action_pseudo_preview.bgr[0] ||
        processing_preview_mode != L"Processing result: Stitch result" ||
        fusion_preview_mode != L"Fluorescence fusion" ||
        pseudo_preview_mode != L"Pseudo color: Hot" ||
        original_preview_mode != L"Original image" ||
        empty_preview_mode != L"(none)") {
        return Fail("PreviewDisplayActions did not build preview frames in the expected priority order.");
    }

    const ImageFrame pattern = MakePatternImage(32, 24);
    const ImageFrame reference_crop = CropImage(pattern, 0, 0, 20, 16);
    const ImageFrame moving_crop = CropImage(pattern, 6, 3, 20, 16);
    const TranslationOffset translation = ImageRegistration::EstimateTranslation(reference_crop, moving_crop, 12, 8);
    if (!translation.valid || translation.dx != 6 || translation.dy != 3) {
        return Fail("Image registration did not recover the expected translation.");
    }
    const ImageFrame brighter_moving_crop = AdjustBrightness(moving_crop, 30);
    const TranslationOffset exposure_translation =
        ImageRegistration::EstimateTranslation(reference_crop, brighter_moving_crop, 12, 8);
    if (!exposure_translation.valid || exposure_translation.dx != 6 || exposure_translation.dy != 3) {
        return Fail("Image registration did not recover translation under exposure changes.");
    }
    const TranslationOffset refined_exposure_translation =
        ImageRegistration::RefineTranslation(reference_crop, brighter_moving_crop, 5, 2, 3, 3);
    if (!refined_exposure_translation.valid ||
        refined_exposure_translation.dx != 6 ||
        refined_exposure_translation.dy != 3) {
        return Fail("Image registration did not refine translation under exposure changes.");
    }

    const StitchTilePlacementResult first_placement =
        StitchTilePlacementPlanner::PlaceNext(MakeSolidImage(3, 2, 1, 2, 3), {}, 50);
    if (first_placement.registered ||
        first_placement.tile.offset_x != 0 ||
        first_placement.tile.offset_y != 0 ||
        !first_placement.tile.frame.IsValid()) {
        return Fail("StitchTilePlacementPlanner did not place the first tile at the origin.");
    }
    StitchTile previous_registered_tile;
    previous_registered_tile.frame = reference_crop;
    previous_registered_tile.offset_x = 10;
    previous_registered_tile.offset_y = 20;
    const StitchTilePlacementResult registered_placement =
        StitchTilePlacementPlanner::PlaceNext(moving_crop, {previous_registered_tile}, 50);
    if (!registered_placement.registered ||
        !registered_placement.registration.valid ||
        registered_placement.registration.dx != 6 ||
        registered_placement.registration.dy != 3 ||
        registered_placement.tile.offset_x != 16 ||
        registered_placement.tile.offset_y != 23) {
        return Fail("StitchTilePlacementPlanner did not apply the registration offset.");
    }
    StitchTile unrelated_last_tile;
    unrelated_last_tile.frame = MakeSolidImage(20, 16, 7, 7, 7);
    unrelated_last_tile.offset_x = 200;
    unrelated_last_tile.offset_y = 120;
    const StitchTilePlacementResult adjacent_default_placement =
        StitchTilePlacementPlanner::PlaceNext(
            moving_crop,
            {previous_registered_tile, unrelated_last_tile},
            50);
    if (adjacent_default_placement.registered ||
        adjacent_default_placement.tile.offset_x != 220 ||
        adjacent_default_placement.tile.offset_y != 120) {
        return Fail("StitchTilePlacementPlanner did not default to the previous tile relation.");
    }
    const ImageFrame large_pattern = MakePatternImage(420, 300);
    StitchTile large_previous_tile;
    large_previous_tile.frame = CropImage(large_pattern, 0, 0, 320, 240);
    large_previous_tile.offset_x = 30;
    large_previous_tile.offset_y = 40;
    const StitchTilePlacementResult large_registered_placement =
        StitchTilePlacementPlanner::PlaceNext(
            CropImage(large_pattern, 24, 12, 320, 240),
            {large_previous_tile},
            50);
    if (large_registered_placement.registered ||
        !large_registered_placement.estimated ||
        !large_registered_placement.tile.estimated_position ||
        large_registered_placement.tile.offset_x != 190 ||
        large_registered_placement.tile.offset_y != 40) {
        return Fail("StitchTilePlacementPlanner did not use fast adjacent placement for larger tiles.");
    }
    const ImageFrame precise_large_moving_tile = CropImage(large_pattern, 25, 13, 320, 240);
    const StitchTilePlacementResult precise_large_registered_placement =
        StitchTilePlacementPlanner::PlaceNext(
            precise_large_moving_tile,
            {large_previous_tile},
            50);
    if (precise_large_registered_placement.registered ||
        !precise_large_registered_placement.estimated ||
        !precise_large_registered_placement.tile.estimated_position ||
        precise_large_registered_placement.tile.offset_x != 190 ||
        precise_large_registered_placement.tile.offset_y != 40) {
        return Fail("StitchTilePlacementPlanner did not keep larger Add Tile placement fast.");
    }
    StitchTile first_fast_large_tile;
    first_fast_large_tile.frame = MakeSolidImage(1920, 1080, 1, 2, 3);
    first_fast_large_tile.offset_x = 0;
    first_fast_large_tile.offset_y = 0;
    std::vector<StitchTile> fast_large_tiles;
    fast_large_tiles.push_back(std::move(first_fast_large_tile));
    ImageFrame second_fast_large_frame = MakeSolidImage(1920, 1080, 4, 5, 6);
    const auto fast_add_start = std::chrono::steady_clock::now();
    const StitchTilePlacementResult fast_large_placement =
        StitchTilePlacementPlanner::PlaceNext(std::move(second_fast_large_frame), fast_large_tiles, 85);
    const auto fast_add_elapsed = std::chrono::steady_clock::now() - fast_add_start;
    if (fast_large_placement.registered ||
        !fast_large_placement.estimated ||
        !fast_large_placement.tile.estimated_position ||
        fast_large_placement.tile.offset_x != 1632 ||
        fast_large_placement.tile.offset_y != 0 ||
        std::chrono::duration_cast<std::chrono::milliseconds>(fast_add_elapsed).count() >= 2000) {
        return Fail("StitchTilePlacementPlanner did not keep large Add Tile placement under 2 seconds.");
    }
    constexpr int kFastQueueTileWidth = 4096;
    constexpr int kFastQueueTileHeight = 3072;
    StitchTile fast_queue_previous_tile;
    fast_queue_previous_tile.frame = MakeSolidImage(kFastQueueTileWidth, kFastQueueTileHeight, 1, 2, 3);
    fast_queue_previous_tile.offset_x = 0;
    fast_queue_previous_tile.offset_y = 0;
    std::vector<StitchTile> fast_queue_tiles;
    fast_queue_tiles.push_back(std::move(fast_queue_previous_tile));
    ProcessingResultFrames fast_queue_frames;
    FrameBuffer fast_queue_buffer;
    fast_queue_buffer.Publish(MakeSolidImage(kFastQueueTileWidth, kFastQueueTileHeight, 4, 5, 6));
    const auto fast_queue_add_start = std::chrono::steady_clock::now();
    const ProcessingQueueActionResult fast_queue_add =
        ProcessingQueueActions::AddStitchTile(
            fast_queue_tiles,
            fast_queue_frames,
            fast_queue_buffer.Snapshot(),
            L"85",
            85);
    const auto fast_queue_add_elapsed = std::chrono::steady_clock::now() - fast_queue_add_start;
    if (!fast_queue_add.changed ||
        fast_queue_add.preview_changed ||
        fast_queue_add.status != ProcessingQueueActionStatus::StitchAdded ||
        fast_queue_tiles.size() != 2 ||
        !fast_queue_tiles[1].estimated_position ||
        fast_queue_tiles[1].offset_x != 3481 ||
        fast_queue_tiles[1].offset_y != 0 ||
        std::chrono::duration_cast<std::chrono::milliseconds>(fast_queue_add_elapsed).count() >= 2000) {
        return Fail("ProcessingQueueActions did not keep 4096x3072 Add Tile under 2 seconds.");
    }
    StitchTile previous_fallback_tile;
    previous_fallback_tile.frame = MakeSolidImage(2, 1, 1, 1, 1);
    previous_fallback_tile.offset_x = 7;
    previous_fallback_tile.offset_y = 9;
    const StitchTilePlacementResult fallback_placement =
        StitchTilePlacementPlanner::PlaceNext(MakeSolidImage(2, 1, 2, 2, 2), {previous_fallback_tile}, 50);
    if (fallback_placement.registered ||
        fallback_placement.tile.offset_x != 9 ||
        fallback_placement.tile.offset_y != 9) {
        return Fail("StitchTilePlacementPlanner did not fall back to horizontal placement.");
    }

    std::vector<StitchTile> stitch_action_tiles;
    const StitchTileListActionResult missing_stitch_action =
        StitchTileListActions::AddCurrentFrame(stitch_action_tiles, ImageFrame{}, 50);
    if (missing_stitch_action.changed ||
        missing_stitch_action.status != StitchTileListActionStatus::NoFrame ||
        !stitch_action_tiles.empty() ||
        missing_stitch_action.message != L"No image frame to add as a stitch tile.") {
        return Fail("StitchTileListActions did not reject an empty frame.");
    }
    const StitchTileListActionResult first_stitch_action =
        StitchTileListActions::AddCurrentFrame(stitch_action_tiles, MakeSolidImage(3, 2, 1, 2, 3), 50);
    if (!first_stitch_action.changed ||
        first_stitch_action.status != StitchTileListActionStatus::Added ||
        first_stitch_action.registered ||
        first_stitch_action.tile_count != 1 ||
        stitch_action_tiles.size() != 1 ||
        stitch_action_tiles[0].offset_x != 0 ||
        stitch_action_tiles[0].offset_y != 0 ||
        first_stitch_action.message != L"Stitch tile added: 1.") {
        return Fail("StitchTileListActions did not add the first tile at the origin.");
    }
    std::vector<StitchTile> registered_action_tiles{previous_registered_tile};
    const StitchTileListActionResult registered_stitch_action =
        StitchTileListActions::AddCurrentFrame(registered_action_tiles, moving_crop, 50);
    if (!registered_stitch_action.changed ||
        registered_stitch_action.status != StitchTileListActionStatus::Added ||
        !registered_stitch_action.registered ||
        registered_stitch_action.estimated ||
        registered_action_tiles[1].estimated_position ||
        registered_stitch_action.registration.dx != 6 ||
        registered_stitch_action.registration.dy != 3 ||
        registered_stitch_action.tile_count != 2 ||
        registered_action_tiles.size() != 2 ||
        registered_action_tiles[1].offset_x != 16 ||
        registered_action_tiles[1].offset_y != 23 ||
        registered_stitch_action.message != L"Stitch tile added with offset 6,3.") {
        return Fail("StitchTileListActions did not add a registered tile with status text.");
    }
    std::vector<StitchTile> estimated_action_tiles;
    StitchTile estimated_action_previous;
    estimated_action_previous.frame = MakeSolidImage(320, 240, 1, 2, 3);
    estimated_action_previous.offset_x = 30;
    estimated_action_previous.offset_y = 40;
    estimated_action_tiles.push_back(std::move(estimated_action_previous));
    const StitchTileListActionResult estimated_stitch_action =
        StitchTileListActions::AddCurrentFrame(estimated_action_tiles, MakeSolidImage(320, 240, 4, 5, 6), 50);
    if (!estimated_stitch_action.changed ||
        estimated_stitch_action.status != StitchTileListActionStatus::Added ||
        estimated_stitch_action.registered ||
        !estimated_stitch_action.estimated ||
        estimated_stitch_action.tile_count != 2 ||
        !estimated_action_tiles[1].estimated_position ||
        estimated_action_tiles[1].offset_x != 190 ||
        estimated_action_tiles[1].offset_y != 40 ||
        estimated_stitch_action.message !=
            L"Stitch tile added with fast estimated position. Stitch will refine alignment.") {
        return Fail("StitchTileListActions did not report fast estimated stitch placement.");
    }
    const std::vector<std::wstring> stitch_tile_lines =
        StitchTileDisplayActions::TileLines(registered_action_tiles);
    if (stitch_tile_lines.size() != 2 ||
        stitch_tile_lines[0] != L"Tile 1  20x16  @ 10,20" ||
        stitch_tile_lines[1] != L"Tile 2  20x16  @ 16,23") {
        return Fail("StitchTileDisplayActions did not format stitch tile gallery lines.");
    }
    const std::vector<std::wstring> estimated_stitch_tile_lines =
        StitchTileDisplayActions::TileLines(estimated_action_tiles);
    if (estimated_stitch_tile_lines.size() != 2 ||
        estimated_stitch_tile_lines[0] != L"Tile 1  320x240  @ 30,40" ||
        estimated_stitch_tile_lines[1] != L"Tile 2  320x240  @ 190,40  estimated") {
        return Fail("StitchTileDisplayActions did not mark estimated stitch tile positions.");
    }
    const StitchTileListActionResult missing_delete_stitch_tile =
        StitchTileListActions::DeleteSelected(registered_action_tiles, -1);
    if (missing_delete_stitch_tile.changed ||
        missing_delete_stitch_tile.status != StitchTileListActionStatus::NoSelection ||
        registered_action_tiles.size() != 2) {
        return Fail("StitchTileListActions should reject deleting without a selected tile.");
    }
    const StitchTileListActionResult delete_stitch_tile =
        StitchTileListActions::DeleteSelected(registered_action_tiles, 0);
    if (!delete_stitch_tile.changed ||
        delete_stitch_tile.status != StitchTileListActionStatus::Deleted ||
        delete_stitch_tile.tile_count != 1 ||
        !delete_stitch_tile.next_selection ||
        *delete_stitch_tile.next_selection != 0 ||
        registered_action_tiles.size() != 1 ||
        registered_action_tiles[0].offset_x != 16) {
        return Fail("StitchTileListActions did not delete the selected stitch tile.");
    }
    const StitchTileListActionResult clear_stitch_tiles =
        StitchTileListActions::Clear(registered_action_tiles);
    if (!clear_stitch_tiles.changed ||
        clear_stitch_tiles.status != StitchTileListActionStatus::Cleared ||
        clear_stitch_tiles.tile_count != 0 ||
        !registered_action_tiles.empty()) {
        return Fail("StitchTileListActions did not clear stitch tiles.");
    }
    const StitchTileListActionResult clear_empty_stitch_tiles =
        StitchTileListActions::Clear(registered_action_tiles);
    if (clear_empty_stitch_tiles.changed ||
        clear_empty_stitch_tiles.status != StitchTileListActionStatus::Empty ||
        clear_empty_stitch_tiles.message != L"No stitch tiles to clear.") {
        return Fail("StitchTileListActions should report an empty stitch tile gallery.");
    }

    StitchTile left_tile;
    left_tile.frame = MakeSolidImage(2, 1, 10, 20, 30);
    left_tile.offset_x = 0;
    left_tile.offset_y = 0;
    StitchTile right_tile;
    right_tile.frame = MakeSolidImage(2, 1, 50, 70, 90);
    right_tile.offset_x = 1;
    right_tile.offset_y = 0;
    std::vector<int> stitch_progress;
    const ImageFrame stitched_image = ImageStitcher::StitchAverage(
        {left_tile, right_tile},
        nullptr,
        [&](int percent) { stitch_progress.push_back(percent); });
    if (!stitched_image.IsValid() || stitched_image.width != 3 || stitched_image.height != 1) {
        return Fail("Image stitching did not produce the expected canvas size.");
    }
    if (stitched_image.bgr[0] != 10 ||
        stitched_image.bgr[3] != 10 ||
        stitched_image.bgr[4] != 20 ||
        stitched_image.bgr[5] != 30 ||
        stitched_image.bgr[6] != 50) {
        return Fail("Image stitching did not preserve sharp source pixels in the overlap.");
    }
    if (!NonDecreasingProgress(stitch_progress)) {
        return Fail("Image stitching did not report monotonic progress from 0 to 100.");
    }
    StitchTile exposure_left_tile;
    exposure_left_tile.frame = MakeSolidImage(96, 64, 90, 100, 110);
    exposure_left_tile.offset_x = 0;
    exposure_left_tile.offset_y = 0;
    StitchTile exposure_right_tile;
    exposure_right_tile.frame = MakeSolidImage(96, 64, 125, 135, 145);
    exposure_right_tile.offset_x = 48;
    exposure_right_tile.offset_y = 0;
    const ImageFrame exposure_matched_stitch =
        ImageStitcher::StitchAverage({exposure_left_tile, exposure_right_tile});
    if (!exposure_matched_stitch.IsValid() ||
        exposure_matched_stitch.width != 144 ||
        exposure_matched_stitch.height != 64) {
        return Fail("Image stitching did not build the exposure-matched canvas.");
    }
    const unsigned char* exposure_right_only =
        exposure_matched_stitch.bgr.data() +
        static_cast<std::size_t>(32) * static_cast<std::size_t>(exposure_matched_stitch.stride) +
        static_cast<std::size_t>(120) * 3U;
    if (std::abs(static_cast<int>(exposure_right_only[0]) - 90) > 1 ||
        std::abs(static_cast<int>(exposure_right_only[1]) - 100) > 1 ||
        std::abs(static_cast<int>(exposure_right_only[2]) - 110) > 1) {
        return Fail("Image stitching did not compensate exposure differences across stitched tiles.");
    }

    const ImageFrame stitch_pattern = MakePatternImage(64, 64);
    StitchTile top_left_tile;
    top_left_tile.frame = CropImage(stitch_pattern, 0, 0, 32, 32);
    top_left_tile.offset_x = 0;
    top_left_tile.offset_y = 0;
    StitchTile top_right_tile;
    top_right_tile.frame = CropImage(stitch_pattern, 20, 0, 32, 32);
    top_right_tile.offset_x = 20;
    top_right_tile.offset_y = 0;
    StitchTile bottom_right_tile;
    bottom_right_tile.frame = CropImage(stitch_pattern, 20, 20, 32, 32);
    bottom_right_tile.offset_x = 22;
    bottom_right_tile.offset_y = 19;
    StitchTile bottom_left_tile;
    bottom_left_tile.frame = CropImage(stitch_pattern, 0, 20, 32, 32);
    bottom_left_tile.offset_x = 3;
    bottom_left_tile.offset_y = 21;
    StitchOptimizationOptions optimization_options;
    optimization_options.search_radius_x = 6;
    optimization_options.search_radius_y = 6;
    optimization_options.iterations = 40;
    const StitchOptimizationResult optimized_stitch = ImageStitcher::OptimizeTileOffsets(
        {top_left_tile, top_right_tile, bottom_right_tile, bottom_left_tile},
        optimization_options);
    if (!optimized_stitch.optimized || optimized_stitch.constraint_count < 4) {
        return Fail("Global stitch optimization did not collect enough tile relations.");
    }
    if (optimized_stitch.tiles[1].offset_x != 20 ||
        optimized_stitch.tiles[1].offset_y != 0 ||
        optimized_stitch.tiles[2].offset_x != 20 ||
        optimized_stitch.tiles[2].offset_y != 20 ||
        optimized_stitch.tiles[3].offset_x != 0 ||
        optimized_stitch.tiles[3].offset_y != 20) {
        return Fail(
            "Global stitch optimization did not correct drifted tile positions; got " +
            std::to_string(optimized_stitch.tiles[1].offset_x) + "," +
            std::to_string(optimized_stitch.tiles[1].offset_y) + " / " +
            std::to_string(optimized_stitch.tiles[2].offset_x) + "," +
            std::to_string(optimized_stitch.tiles[2].offset_y) + " / " +
            std::to_string(optimized_stitch.tiles[3].offset_x) + "," +
            std::to_string(optimized_stitch.tiles[3].offset_y) + ".");
    }

    std::atomic_bool cancel_stitch = true;
    if (ImageStitcher::StitchAverage({left_tile, right_tile}, &cancel_stitch).IsValid()) {
        return Fail("Image stitching did not honor cancellation.");
    }

    std::vector<int> stitch_job_progress;
    const ProcessingJobResult stitch_job_result = ProcessingJobExecutor::RunStitch(
        42,
        {left_tile, right_tile},
        50,
        nullptr,
        [&](int percent) { stitch_job_progress.push_back(percent); });
    if (stitch_job_result.job_id != 42 ||
        stitch_job_result.kind != ProcessingJobKind::Stitch ||
        !stitch_job_result.succeeded ||
        !stitch_job_result.image.IsValid() ||
        stitch_job_result.image.width != 3 ||
        stitch_job_result.status.empty()) {
        return Fail("ProcessingJobExecutor did not produce a successful stitch result.");
    }
    if (!NonDecreasingProgress(stitch_job_progress)) {
        return Fail("ProcessingJobExecutor did not report monotonic stitch progress.");
    }
    std::vector<int> large_stitch_job_progress;
    const ProcessingJobResult large_stitch_job_result = ProcessingJobExecutor::RunStitch(
        44,
        {large_previous_tile, large_registered_placement.tile},
        50,
        nullptr,
        [&](int percent) { large_stitch_job_progress.push_back(percent); });
    if (large_stitch_job_result.job_id != 44 ||
        large_stitch_job_result.kind != ProcessingJobKind::Stitch ||
        !large_stitch_job_result.succeeded ||
        !large_stitch_job_result.image.IsValid() ||
        large_stitch_job_result.image.width != 344 ||
        large_stitch_job_result.image.height != 252) {
        return Fail(
            "ProcessingJobExecutor did not refine a larger estimated stitch result; got " +
            std::to_string(large_stitch_job_result.image.width) +
            "x" +
            std::to_string(large_stitch_job_result.image.height) +
            ".");
    }
    if (!NonDecreasingProgress(large_stitch_job_progress)) {
        return Fail("ProcessingJobExecutor did not report monotonic larger stitch progress.");
    }
    std::atomic_bool cancel_stitch_job = true;
    const ProcessingJobResult canceled_stitch_job = ProcessingJobExecutor::RunStitch(
        43,
        {left_tile, right_tile},
        50,
        &cancel_stitch_job);
    if (canceled_stitch_job.succeeded ||
        canceled_stitch_job.status != ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Stitch)) {
        return Fail("ProcessingJobExecutor did not return a canceled stitch result.");
    }

    ProcessingJobState stitch_worker_state;
    const ProcessingJobLaunch stitch_worker_launch = stitch_worker_state.Begin();
    std::vector<std::wstring> stitch_worker_statuses;
    ProcessingJobResult stitch_worker_result;
    int stitch_worker_publish_count = 0;
    std::thread stitch_worker = ProcessingWorkerActions::StartStitch(
        stitch_worker_launch,
        {left_tile, right_tile},
        50,
        [&](const std::wstring& message) { stitch_worker_statuses.push_back(message); },
        [&](ProcessingJobResult result) {
            stitch_worker_result = std::move(result);
            ++stitch_worker_publish_count;
        });
    if (!stitch_worker.joinable()) {
        return Fail("ProcessingWorkerActions did not create a stitch worker thread.");
    }
    stitch_worker.join();
    stitch_worker_state.MarkIdle();
    const bool stitch_worker_reported =
        std::any_of(stitch_worker_statuses.begin(), stitch_worker_statuses.end(), [](const std::wstring& message) {
            return message.find(L"Stitch processing ") == 0;
        });
    if (stitch_worker_publish_count != 1 ||
        stitch_worker_result.job_id != stitch_worker_launch.job_id ||
        stitch_worker_result.kind != ProcessingJobKind::Stitch ||
        !stitch_worker_result.succeeded ||
        !stitch_worker_result.image.IsValid() ||
        !stitch_worker_reported) {
        return Fail("ProcessingWorkerActions did not run and publish a stitch worker result.");
    }

    std::vector<ImageFrame> edf_action_stack;
    const EdfStackListActionResult missing_edf_action =
        EdfStackListActions::AddCurrentFrame(edf_action_stack, ImageFrame{});
    if (missing_edf_action.changed ||
        missing_edf_action.status != EdfStackListActionStatus::NoFrame ||
        !edf_action_stack.empty() ||
        missing_edf_action.message != L"No image frame to add to the EDF stack.") {
        return Fail("EdfStackListActions did not reject an empty frame.");
    }
    const EdfStackListActionResult first_edf_action =
        EdfStackListActions::AddCurrentFrame(edf_action_stack, MakeSolidImage(5, 1, 0, 0, 0));
    if (!first_edf_action.changed ||
        first_edf_action.status != EdfStackListActionStatus::Added ||
        first_edf_action.frame_count != 1 ||
        edf_action_stack.size() != 1 ||
        !edf_action_stack[0].IsValid() ||
        first_edf_action.message != L"EDF frame added: 1.") {
        return Fail("EdfStackListActions did not add the first EDF frame.");
    }
    const EdfStackListActionResult second_edf_action =
        EdfStackListActions::AddCurrentFrame(edf_action_stack, MakeSolidImage(5, 1, 0, 0, 0));
    if (!second_edf_action.changed ||
        second_edf_action.frame_count != 2 ||
        edf_action_stack.size() != 2 ||
        second_edf_action.message != L"EDF frame added: 2.") {
        return Fail("EdfStackListActions did not report the EDF frame count.");
    }

    ImageFrame edf_a = MakeSolidImage(5, 1, 0, 0, 0);
    ImageFrame edf_b = MakeSolidImage(5, 1, 0, 0, 0);
    edf_a.bgr[1 * 3 + 2] = 255;
    edf_b.bgr[3 * 3 + 1] = 255;
    std::vector<int> edf_progress;
    const EdfResult edf_result = EdfProcessor::ComposeFocusStack(
        {edf_a, edf_b},
        nullptr,
        [&](int percent) { edf_progress.push_back(percent); });
    const ImageFrame& edf_image = edf_result.composite_frame;
    if (!edf_image.IsValid() || edf_image.width != 5 || edf_image.height != 1) {
        return Fail("EDF did not produce a valid fused image.");
    }
    if (edf_image.bgr[1 * 3 + 2] != 255 || edf_image.bgr[3 * 3 + 1] != 255) {
        return Fail("EDF did not select sharp pixels from the expected source frames.");
    }
    if (!edf_result.focus_map.IsValid() ||
        edf_result.focus_map.bgr[1 * 3] != 0 ||
        edf_result.focus_map.bgr[3 * 3] != 255) {
        return Fail("EDF focus map did not encode the selected source frame.");
    }
    if (!NonDecreasingProgress(edf_progress)) {
        return Fail("EDF did not report monotonic progress from 0 to 100.");
    }

    ImageFrame edf_radius_a = MakeSolidImage(5, 1, 0, 0, 0);
    ImageFrame edf_radius_b = MakeSolidImage(5, 1, 0, 0, 0);
    edf_radius_a.bgr[1 * 3 + 2] = 170;
    edf_radius_b.bgr[3 * 3 + 1] = 80;
    edf_radius_b.bgr[0 * 3 + 1] = 255;
    edf_radius_b.bgr[4 * 3 + 1] = 255;
    EdfOptions radius_one;
    radius_one.focus_radius = 1;
    EdfOptions radius_two;
    radius_two.focus_radius = 2;
    const EdfResult edf_radius_one = EdfProcessor::ComposeFocusStack({edf_radius_a, edf_radius_b}, radius_one);
    const EdfResult edf_radius_two = EdfProcessor::ComposeFocusStack({edf_radius_a, edf_radius_b}, radius_two);
    if (!edf_radius_one.focus_map.IsValid() ||
        !edf_radius_two.focus_map.IsValid() ||
        edf_radius_one.focus_map.bgr[2 * 3] != 0 ||
        edf_radius_two.focus_map.bgr[2 * 3] != 255) {
        return Fail("EDF focus radius did not affect the selected source frame.");
    }

    std::atomic_bool cancel_edf = true;
    if (EdfProcessor::ComposeFocusStack({edf_a, edf_b}, &cancel_edf).composite_frame.IsValid()) {
        return Fail("EDF did not honor cancellation.");
    }

    std::vector<int> edf_job_progress;
    const ProcessingJobResult edf_job_result = ProcessingJobExecutor::RunEdf(
        52,
        {edf_a, edf_b},
        EdfOptions{},
        nullptr,
        [&](int percent) { edf_job_progress.push_back(percent); });
    if (edf_job_result.job_id != 52 ||
        edf_job_result.kind != ProcessingJobKind::Edf ||
        !edf_job_result.succeeded ||
        !edf_job_result.image.IsValid() ||
        !edf_job_result.focus_map.IsValid() ||
        edf_job_result.status.empty()) {
        return Fail("ProcessingJobExecutor did not produce a successful EDF result.");
    }
    if (!NonDecreasingProgress(edf_job_progress)) {
        return Fail("ProcessingJobExecutor did not report monotonic EDF progress.");
    }
    std::atomic_bool cancel_edf_job = true;
    const ProcessingJobResult canceled_edf_job = ProcessingJobExecutor::RunEdf(
        53,
        {edf_a, edf_b},
        EdfOptions{},
        &cancel_edf_job);
    if (canceled_edf_job.succeeded ||
        canceled_edf_job.status != ProcessingStatusFormatter::FormatCanceled(ProcessingJobKind::Edf)) {
        return Fail("ProcessingJobExecutor did not return a canceled EDF result.");
    }

    ProcessingJobState edf_worker_state;
    const ProcessingJobLaunch edf_worker_launch = edf_worker_state.Begin();
    std::vector<std::wstring> edf_worker_statuses;
    ProcessingJobResult edf_worker_result;
    int edf_worker_publish_count = 0;
    std::thread edf_worker = ProcessingWorkerActions::StartEdf(
        edf_worker_launch,
        {edf_a, edf_b},
        EdfOptions{},
        [&](const std::wstring& message) { edf_worker_statuses.push_back(message); },
        [&](ProcessingJobResult result) {
            edf_worker_result = std::move(result);
            ++edf_worker_publish_count;
        });
    if (!edf_worker.joinable()) {
        return Fail("ProcessingWorkerActions did not create an EDF worker thread.");
    }
    edf_worker.join();
    edf_worker_state.MarkIdle();
    const bool edf_worker_reported =
        std::any_of(edf_worker_statuses.begin(), edf_worker_statuses.end(), [](const std::wstring& message) {
            return message.find(L"EDF processing ") == 0;
        });
    if (edf_worker_publish_count != 1 ||
        edf_worker_result.job_id != edf_worker_launch.job_id ||
        edf_worker_result.kind != ProcessingJobKind::Edf ||
        !edf_worker_result.succeeded ||
        !edf_worker_result.image.IsValid() ||
        !edf_worker_result.focus_map.IsValid() ||
        !edf_worker_reported) {
        return Fail("ProcessingWorkerActions did not run and publish an EDF worker result.");
    }

    MeasurementCollection csv_measurements;
    csv_measurements.AddLength(L"CSV, \"Length\"", ImagePoint{0.0, 0.0}, ImagePoint{3.0, 4.0});
    csv_measurements.AddPolygonArea(
        L"CSV Polygon",
        std::vector<ImagePoint>{ImagePoint{0.0, 0.0}, ImagePoint{4.0, 0.0}, ImagePoint{4.0, 4.0}});
    const std::filesystem::path csv_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.csv";
    if (!MeasurementCsvExporter::Save(csv_path, csv_measurements, calibration, MeasurementUnit::Micrometers, error)) {
        return Fail("CSV export failed.");
    }

    std::ifstream csv(csv_path, std::ios::binary);
    csv.seekg(0, std::ios::end);
    const std::streamoff csv_size = csv.tellg();
    csv.seekg(0, std::ios::beg);
    std::string csv_text(static_cast<std::size_t>(csv_size), '\0');
    if (!csv_text.empty()) {
        csv.read(csv_text.data(), static_cast<std::streamsize>(csv_text.size()));
    }
    if (csv_text.size() < 3 ||
        static_cast<unsigned char>(csv_text[0]) != 0xEF ||
        static_cast<unsigned char>(csv_text[1]) != 0xBB ||
        static_cast<unsigned char>(csv_text[2]) != 0xBF ||
        csv_text.find("\"CSV, \"\"Length\"\"\"") == std::string::npos ||
        csv_text.find("0.0000:0.0000;4.0000:0.0000;4.0000:4.0000") == std::string::npos) {
        return Fail("CSV export did not preserve BOM, escaping, or polygon point list.");
    }

    const std::filesystem::path action_csv_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.csv";
    const ExportActionResult empty_csv_export = ExportActions::SaveMeasurementsCsv(
        action_csv_path,
        MeasurementCollection{},
        calibration,
        MeasurementUnit::Micrometers);
    if (empty_csv_export.saved ||
        empty_csv_export.status != ExportActionStatus::NoMeasurements ||
        empty_csv_export.message != L"No measurements to export.") {
        return Fail("ExportActions did not reject empty measurement CSV export.");
    }
    const ExportActionResult csv_export = ExportActions::SaveMeasurementsCsv(
        action_csv_path,
        csv_measurements,
        calibration,
        MeasurementUnit::Micrometers);
    if (!csv_export.saved ||
        csv_export.status != ExportActionStatus::Saved ||
        csv_export.message != L"CSV exported." ||
        !std::filesystem::exists(action_csv_path)) {
        return Fail("ExportActions did not save a measurement CSV.");
    }
    std::filesystem::remove(action_csv_path);

    const std::vector<LengthMeasurement> export_measurements = {
        LengthMeasurement(L"Export Length", ImagePoint{1.0, 1.0}, ImagePoint{14.0, 1.0})
    };
    const std::size_t preserved_pixel =
        static_cast<std::size_t>(image.height - 1) * static_cast<std::size_t>(image.stride) +
        static_cast<std::size_t>(image.width - 1) * 3U;
    image.bgr[preserved_pixel] = 12;
    image.bgr[preserved_pixel + 1] = 34;
    image.bgr[preserved_pixel + 2] = 56;
    const std::filesystem::path bmp_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.bmp";
    if (!ImageExporter::SaveBmp(bmp_path, image, export_measurements, error)) {
        return Fail("BMP export failed.");
    }

    std::ifstream bmp(bmp_path, std::ios::binary);
    bmp.seekg(0, std::ios::end);
    const std::streamoff bmp_size = bmp.tellg();
    bmp.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(bmp_size));
    if (!bytes.empty()) {
        bmp.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    bmp.close();
    if (bytes.size() < 54 || bytes[0] != 'B' || bytes[1] != 'M') {
        return Fail("BMP export did not create a valid BMP header.");
    }
    const std::size_t pixel_offset = 54U + static_cast<std::size_t>(image.height - 1 - 1) * static_cast<std::size_t>(image.stride) + 1U * 3U;
    if (pixel_offset + 2 >= bytes.size() || bytes[pixel_offset] != 74 || bytes[pixel_offset + 1] != 214 || bytes[pixel_offset + 2] != 255) {
        return Fail("BMP export did not draw measurement overlay pixels.");
    }
    ImageFrame loaded_bmp;
    std::wstring load_error;
    if (!ImageExporter::LoadBmp(bmp_path, loaded_bmp, load_error) ||
        !loaded_bmp.IsValid() ||
        loaded_bmp.width != image.width ||
        loaded_bmp.height != image.height ||
        loaded_bmp.stride != image.stride ||
        loaded_bmp.bgr[preserved_pixel] != image.bgr[preserved_pixel] ||
        loaded_bmp.bgr[preserved_pixel + 1] != image.bgr[preserved_pixel + 1] ||
        loaded_bmp.bgr[preserved_pixel + 2] != image.bgr[preserved_pixel + 2]) {
        std::filesystem::remove(bmp_path);
        return Fail("BMP import did not read an exported BMP image.");
    }
    std::filesystem::remove(bmp_path);
    const std::filesystem::path missing_bmp_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsMissingImport.bmp";
    std::filesystem::remove(missing_bmp_path);
    if (ImageExporter::LoadBmp(missing_bmp_path, loaded_bmp, load_error) ||
        load_error != L"Failed to open image file.") {
        return Fail("BMP import did not reject a missing image file.");
    }
    ImageFrame wic_source = MakeSolidImage(3, 2, 12, 34, 56);
    wic_source.bgr[0] = 90;
    wic_source.bgr[1] = 91;
    wic_source.bgr[2] = 92;
    wic_source.bgr[static_cast<std::size_t>(wic_source.stride) + 2U * 3U + 0U] = 210;
    wic_source.bgr[static_cast<std::size_t>(wic_source.stride) + 2U * 3U + 1U] = 211;
    wic_source.bgr[static_cast<std::size_t>(wic_source.stride) + 2U * 3U + 2U] = 212;
    const std::filesystem::path png_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.png";
    const std::filesystem::path tif_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.tif";
    const std::filesystem::path jpg_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests.jpg";
    std::filesystem::remove(png_path);
    std::filesystem::remove(tif_path);
    std::filesystem::remove(jpg_path);
    if (!SaveWicTestImage(png_path, GUID_ContainerFormatPng, wic_source) ||
        !SaveWicTestImage(tif_path, GUID_ContainerFormatTiff, wic_source) ||
        !SaveWicTestImage(jpg_path, GUID_ContainerFormatJpeg, wic_source)) {
        std::filesystem::remove(png_path);
        std::filesystem::remove(tif_path);
        std::filesystem::remove(jpg_path);
        return Fail("WIC test image generation failed.");
    }
    ImageFrame loaded_png;
    if (!ImageExporter::LoadRasterImage(png_path, loaded_png, load_error) ||
        !loaded_png.IsValid() ||
        loaded_png.width != wic_source.width ||
        loaded_png.height != wic_source.height ||
        loaded_png.stride != wic_source.stride ||
        loaded_png.bgr[0] != 90 ||
        loaded_png.bgr[1] != 91 ||
        loaded_png.bgr[2] != 92 ||
        loaded_png.bgr[static_cast<std::size_t>(loaded_png.stride) + 2U * 3U + 0U] != 210 ||
        loaded_png.bgr[static_cast<std::size_t>(loaded_png.stride) + 2U * 3U + 1U] != 211 ||
        loaded_png.bgr[static_cast<std::size_t>(loaded_png.stride) + 2U * 3U + 2U] != 212) {
        std::filesystem::remove(png_path);
        std::filesystem::remove(tif_path);
        std::filesystem::remove(jpg_path);
        return Fail("Image import did not read a PNG image.");
    }
    ImageFrame loaded_tif;
    if (!ImageExporter::LoadRasterImage(tif_path, loaded_tif, load_error) ||
        !loaded_tif.IsValid() ||
        loaded_tif.width != wic_source.width ||
        loaded_tif.height != wic_source.height ||
        loaded_tif.stride != wic_source.stride ||
        loaded_tif.bgr[0] != 90 ||
        loaded_tif.bgr[1] != 91 ||
        loaded_tif.bgr[2] != 92 ||
        loaded_tif.bgr[static_cast<std::size_t>(loaded_tif.stride) + 2U * 3U + 0U] != 210 ||
        loaded_tif.bgr[static_cast<std::size_t>(loaded_tif.stride) + 2U * 3U + 1U] != 211 ||
        loaded_tif.bgr[static_cast<std::size_t>(loaded_tif.stride) + 2U * 3U + 2U] != 212) {
        std::filesystem::remove(png_path);
        std::filesystem::remove(tif_path);
        std::filesystem::remove(jpg_path);
        return Fail("Image import did not read a TIFF image.");
    }
    ImageFrame loaded_jpg;
    if (!ImageExporter::LoadRasterImage(jpg_path, loaded_jpg, load_error) ||
        !loaded_jpg.IsValid() ||
        loaded_jpg.width != wic_source.width ||
        loaded_jpg.height != wic_source.height ||
        loaded_jpg.stride != wic_source.stride) {
        std::filesystem::remove(png_path);
        std::filesystem::remove(tif_path);
        std::filesystem::remove(jpg_path);
        return Fail("Image import did not read a JPEG image.");
    }
    std::filesystem::remove(png_path);
    std::filesystem::remove(tif_path);
    std::filesystem::remove(jpg_path);

    const std::vector<std::uint16_t> gray16_values = {
        1000, 3000, 5000, 1000,
        5000, 3000, 1000, 5000
    };
    const std::filesystem::path gray16_png_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsGray16.png";
    const std::filesystem::path gray16_tif_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsGray16.tif";
    std::filesystem::remove(gray16_png_path);
    std::filesystem::remove(gray16_tif_path);
    if (!SaveWicGray16TestImage(gray16_png_path, GUID_ContainerFormatPng, 4, 2, gray16_values) ||
        !SaveWicGray16TestImage(gray16_tif_path, GUID_ContainerFormatTiff, 4, 2, gray16_values)) {
        std::filesystem::remove(gray16_png_path);
        std::filesystem::remove(gray16_tif_path);
        return Fail("WIC 16-bit gray test image generation failed.");
    }
    ImageFrame loaded_gray16_png;
    ImageFrame loaded_gray16_tif;
    if (!ImageExporter::LoadRasterImage(gray16_png_path, loaded_gray16_png, load_error) ||
        !ImageExporter::LoadRasterImage(gray16_tif_path, loaded_gray16_tif, load_error) ||
        !loaded_gray16_png.IsValid() ||
        !loaded_gray16_tif.IsValid() ||
        loaded_gray16_png.width != 4 ||
        loaded_gray16_png.height != 2 ||
        loaded_gray16_tif.width != 4 ||
        loaded_gray16_tif.height != 2 ||
        loaded_gray16_png.bgr[0] != 0 ||
        loaded_gray16_png.bgr[1 * 3] < 127 ||
        loaded_gray16_png.bgr[1 * 3] > 128 ||
        loaded_gray16_png.bgr[2 * 3] != 255 ||
        loaded_gray16_tif.bgr[0] != 0 ||
        loaded_gray16_tif.bgr[1 * 3] < 127 ||
        loaded_gray16_tif.bgr[1 * 3] > 128 ||
        loaded_gray16_tif.bgr[2 * 3] != 255) {
        std::filesystem::remove(gray16_png_path);
        std::filesystem::remove(gray16_tif_path);
        return Fail("16-bit gray image import did not auto-stretch the display range.");
    }
    std::filesystem::remove(gray16_png_path);
    std::filesystem::remove(gray16_tif_path);

    const std::filesystem::path indexed_bmp_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests8bit.bmp";
    std::filesystem::remove(indexed_bmp_path);
    {
        auto write_u16 = [](std::ofstream& output, unsigned int value) {
            output.put(static_cast<char>(value & 0xFFU));
            output.put(static_cast<char>((value >> 8) & 0xFFU));
        };
        auto write_u32 = [](std::ofstream& output, unsigned int value) {
            output.put(static_cast<char>(value & 0xFFU));
            output.put(static_cast<char>((value >> 8) & 0xFFU));
            output.put(static_cast<char>((value >> 16) & 0xFFU));
            output.put(static_cast<char>((value >> 24) & 0xFFU));
        };
        constexpr unsigned int indexed_width = 3;
        constexpr unsigned int indexed_height = 2;
        constexpr unsigned int indexed_stride = 4;
        constexpr unsigned int indexed_palette_bytes = 256 * 4;
        constexpr unsigned int indexed_pixel_offset = 14 + 40 + indexed_palette_bytes;
        constexpr unsigned int indexed_image_size = indexed_stride * indexed_height;

        std::ofstream indexed_bmp(indexed_bmp_path, std::ios::binary);
        write_u16(indexed_bmp, 0x4D42);
        write_u32(indexed_bmp, indexed_pixel_offset + indexed_image_size);
        write_u16(indexed_bmp, 0);
        write_u16(indexed_bmp, 0);
        write_u32(indexed_bmp, indexed_pixel_offset);
        write_u32(indexed_bmp, 40);
        write_u32(indexed_bmp, indexed_width);
        write_u32(indexed_bmp, indexed_height);
        write_u16(indexed_bmp, 1);
        write_u16(indexed_bmp, 8);
        write_u32(indexed_bmp, 0);
        write_u32(indexed_bmp, indexed_image_size);
        write_u32(indexed_bmp, 0);
        write_u32(indexed_bmp, 0);
        write_u32(indexed_bmp, 256);
        write_u32(indexed_bmp, 0);
        for (unsigned int value = 0; value < 256; ++value) {
            unsigned char b = static_cast<unsigned char>(value);
            unsigned char g = static_cast<unsigned char>(value);
            unsigned char r = static_cast<unsigned char>(value);
            if (value == 20) {
                b = 5;
                g = 6;
                r = 7;
            }
            indexed_bmp.put(static_cast<char>(b));
            indexed_bmp.put(static_cast<char>(g));
            indexed_bmp.put(static_cast<char>(r));
            indexed_bmp.put('\0');
        }
        const unsigned char bottom_row[indexed_stride] = {40, 50, 60, 0};
        const unsigned char top_row[indexed_stride] = {10, 20, 30, 0};
        indexed_bmp.write(reinterpret_cast<const char*>(bottom_row), sizeof(bottom_row));
        indexed_bmp.write(reinterpret_cast<const char*>(top_row), sizeof(top_row));
    }
    ImageFrame indexed_bmp_frame;
    if (!ImageExporter::LoadBmp(indexed_bmp_path, indexed_bmp_frame, load_error) ||
        !indexed_bmp_frame.IsValid() ||
        indexed_bmp_frame.width != 3 ||
        indexed_bmp_frame.height != 2 ||
        indexed_bmp_frame.stride != 12 ||
        indexed_bmp_frame.bgr[0] != 10 ||
        indexed_bmp_frame.bgr[1] != 10 ||
        indexed_bmp_frame.bgr[2] != 10 ||
        indexed_bmp_frame.bgr[3] != 5 ||
        indexed_bmp_frame.bgr[4] != 6 ||
        indexed_bmp_frame.bgr[5] != 7 ||
        indexed_bmp_frame.bgr[12 + 2 * 3] != 60 ||
        indexed_bmp_frame.bgr[12 + 2 * 3 + 1] != 60 ||
        indexed_bmp_frame.bgr[12 + 2 * 3 + 2] != 60) {
        std::filesystem::remove(indexed_bmp_path);
        return Fail("BMP import did not read an 8-bit indexed BMP image.");
    }
    std::filesystem::remove(indexed_bmp_path);

    const std::filesystem::path bgra_bmp_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTests32bitTopDown.bmp";
    std::filesystem::remove(bgra_bmp_path);
    {
        auto write_u16 = [](std::ofstream& output, unsigned int value) {
            output.put(static_cast<char>(value & 0xFFU));
            output.put(static_cast<char>((value >> 8) & 0xFFU));
        };
        auto write_u32 = [](std::ofstream& output, unsigned int value) {
            output.put(static_cast<char>(value & 0xFFU));
            output.put(static_cast<char>((value >> 8) & 0xFFU));
            output.put(static_cast<char>((value >> 16) & 0xFFU));
            output.put(static_cast<char>((value >> 24) & 0xFFU));
        };
        constexpr unsigned int bgra_width = 2;
        constexpr unsigned int bgra_height = 2;
        constexpr unsigned int bgra_stride = 8;
        constexpr unsigned int bgra_pixel_offset = 14 + 40;
        constexpr unsigned int bgra_image_size = bgra_stride * bgra_height;

        std::ofstream bgra_bmp(bgra_bmp_path, std::ios::binary);
        write_u16(bgra_bmp, 0x4D42);
        write_u32(bgra_bmp, bgra_pixel_offset + bgra_image_size);
        write_u16(bgra_bmp, 0);
        write_u16(bgra_bmp, 0);
        write_u32(bgra_bmp, bgra_pixel_offset);
        write_u32(bgra_bmp, 40);
        write_u32(bgra_bmp, bgra_width);
        write_u32(bgra_bmp, 0U - bgra_height);
        write_u16(bgra_bmp, 1);
        write_u16(bgra_bmp, 32);
        write_u32(bgra_bmp, 0);
        write_u32(bgra_bmp, bgra_image_size);
        write_u32(bgra_bmp, 0);
        write_u32(bgra_bmp, 0);
        write_u32(bgra_bmp, 0);
        write_u32(bgra_bmp, 0);
        const unsigned char top_row[bgra_stride] = {1, 2, 3, 255, 4, 5, 6, 128};
        const unsigned char bottom_row[bgra_stride] = {7, 8, 9, 64, 10, 11, 12, 0};
        bgra_bmp.write(reinterpret_cast<const char*>(top_row), sizeof(top_row));
        bgra_bmp.write(reinterpret_cast<const char*>(bottom_row), sizeof(bottom_row));
    }
    ImageFrame bgra_bmp_frame;
    if (!ImageExporter::LoadBmp(bgra_bmp_path, bgra_bmp_frame, load_error) ||
        !bgra_bmp_frame.IsValid() ||
        bgra_bmp_frame.width != 2 ||
        bgra_bmp_frame.height != 2 ||
        bgra_bmp_frame.stride != 8 ||
        bgra_bmp_frame.bgr[0] != 1 ||
        bgra_bmp_frame.bgr[1] != 2 ||
        bgra_bmp_frame.bgr[2] != 3 ||
        bgra_bmp_frame.bgr[3] != 4 ||
        bgra_bmp_frame.bgr[4] != 5 ||
        bgra_bmp_frame.bgr[5] != 6 ||
        bgra_bmp_frame.bgr[8] != 7 ||
        bgra_bmp_frame.bgr[9] != 8 ||
        bgra_bmp_frame.bgr[10] != 9 ||
        bgra_bmp_frame.bgr[11] != 10 ||
        bgra_bmp_frame.bgr[12] != 11 ||
        bgra_bmp_frame.bgr[13] != 12) {
        std::filesystem::remove(bgra_bmp_path);
        return Fail("BMP import did not read a 32-bit top-down BMP image.");
    }
    std::filesystem::remove(bgra_bmp_path);

    const std::filesystem::path action_bmp_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.bmp";
    const ExportActionResult no_image_export =
        ExportActions::SaveImageBmp(action_bmp_path, ImageFrame{}, csv_measurements);
    if (no_image_export.saved ||
        no_image_export.status != ExportActionStatus::NoImageFrame ||
        no_image_export.message != L"No image frame to export.") {
        return Fail("ExportActions did not reject image export without a frame.");
    }
    const ExportActionResult image_export =
        ExportActions::SaveImageBmp(action_bmp_path, image, csv_measurements);
    if (!image_export.saved ||
        image_export.status != ExportActionStatus::Saved ||
        image_export.message != L"Image exported: 16x16." ||
        !std::filesystem::exists(action_bmp_path)) {
        return Fail("ExportActions did not save a BMP image.");
    }
    std::filesystem::remove(action_bmp_path);
    const ExportActionResult labeled_image_export =
        ExportActions::SaveImageBmp(action_bmp_path, image, csv_measurements, L"Pseudo color: Hot");
    if (!labeled_image_export.saved ||
        labeled_image_export.status != ExportActionStatus::Saved ||
        labeled_image_export.message != L"Image exported: 16x16 (Pseudo color: Hot)." ||
        !std::filesystem::exists(action_bmp_path)) {
        return Fail("ExportActions did not include the preview display mode in image export status.");
    }
    std::filesystem::remove(action_bmp_path);

    MeasurementCollection image_export_measurements;
    image_export_measurements.AddLength(
        L"Image Export Length",
        ImagePoint{1.0, 1.0},
        ImagePoint{14.0, 1.0});
    const std::size_t image_export_overlay_pixel =
        static_cast<std::size_t>(1) * static_cast<std::size_t>(image.stride) +
        static_cast<std::size_t>(1) * 3U;
    const std::filesystem::path action_png_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.png";
    const std::filesystem::path action_tif_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.tif";
    const std::filesystem::path action_jpg_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.jpg";
    std::filesystem::remove(action_png_path);
    std::filesystem::remove(action_tif_path);
    std::filesystem::remove(action_jpg_path);
    const ExportActionResult png_image_export =
        ExportActions::SaveImage(action_png_path, image, image_export_measurements);
    const ExportActionResult tif_image_export =
        ExportActions::SaveImage(action_tif_path, image, image_export_measurements);
    const ExportActionResult jpg_image_export =
        ExportActions::SaveImage(action_jpg_path, image, image_export_measurements);
    if (!png_image_export.saved ||
        !tif_image_export.saved ||
        !jpg_image_export.saved ||
        !std::filesystem::exists(action_png_path) ||
        !std::filesystem::exists(action_tif_path) ||
        !std::filesystem::exists(action_jpg_path)) {
        std::filesystem::remove(action_png_path);
        std::filesystem::remove(action_tif_path);
        std::filesystem::remove(action_jpg_path);
        return Fail("ExportActions did not save PNG, TIFF, and JPEG images.");
    }
    ImageFrame exported_png;
    ImageFrame exported_tif;
    ImageFrame exported_jpg;
    if (!ImageExporter::LoadRasterImage(action_png_path, exported_png, load_error) ||
        !ImageExporter::LoadRasterImage(action_tif_path, exported_tif, load_error) ||
        !ImageExporter::LoadRasterImage(action_jpg_path, exported_jpg, load_error) ||
        exported_png.width != image.width ||
        exported_png.height != image.height ||
        exported_tif.width != image.width ||
        exported_tif.height != image.height ||
        exported_jpg.width != image.width ||
        exported_jpg.height != image.height ||
        exported_png.bgr[image_export_overlay_pixel] != 74 ||
        exported_png.bgr[image_export_overlay_pixel + 1] != 214 ||
        exported_png.bgr[image_export_overlay_pixel + 2] != 255 ||
        exported_tif.bgr[image_export_overlay_pixel] != 74 ||
        exported_tif.bgr[image_export_overlay_pixel + 1] != 214 ||
        exported_tif.bgr[image_export_overlay_pixel + 2] != 255) {
        std::filesystem::remove(action_png_path);
        std::filesystem::remove(action_tif_path);
        std::filesystem::remove(action_jpg_path);
        return Fail("Exported PNG, TIFF, or JPEG images could not be read back.");
    }
    std::filesystem::remove(action_png_path);
    std::filesystem::remove(action_tif_path);
    std::filesystem::remove(action_jpg_path);

    const std::filesystem::path action_gif_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsAction.gif";
    const ExportActionResult unsupported_image_export =
        ExportActions::SaveImage(action_gif_path, image, MeasurementCollection{});
    if (unsupported_image_export.saved ||
        unsupported_image_export.status != ExportActionStatus::WriteFailed ||
        unsupported_image_export.message != L"Unsupported image export format.") {
        return Fail("ExportActions did not reject unsupported image export formats.");
    }

    const std::filesystem::path action_report_path =
        std::filesystem::temp_directory_path() / "CameraViewDomainTestsDiagnostic.txt";
    const ExportActionResult diagnostic_export =
        ExportActions::SaveDiagnosticReport(action_report_path, L"Diagnostic line\n显微测量");
    if (!diagnostic_export.saved ||
        diagnostic_export.status != ExportActionStatus::Saved ||
        diagnostic_export.message != L"Diagnostic report saved.") {
        return Fail("ExportActions did not save a diagnostic report.");
    }
    std::ifstream diagnostic_file(action_report_path, std::ios::binary);
    diagnostic_file.seekg(0, std::ios::end);
    const std::streamoff diagnostic_size = diagnostic_file.tellg();
    diagnostic_file.seekg(0, std::ios::beg);
    std::string diagnostic_bytes(static_cast<std::size_t>(diagnostic_size), '\0');
    if (!diagnostic_bytes.empty()) {
        diagnostic_file.read(diagnostic_bytes.data(), static_cast<std::streamsize>(diagnostic_bytes.size()));
    }
    diagnostic_file.close();
    std::filesystem::remove(action_report_path);
    if (diagnostic_bytes.size() < 3 ||
        static_cast<unsigned char>(diagnostic_bytes[0]) != 0xEF ||
        static_cast<unsigned char>(diagnostic_bytes[1]) != 0xBB ||
        static_cast<unsigned char>(diagnostic_bytes[2]) != 0xBF ||
        diagnostic_bytes.find("Diagnostic line") == std::string::npos) {
        return Fail("ExportActions did not write diagnostic report as UTF-8 with BOM.");
    }

    std::cout << "domain smoke tests passed\n";
    return 0;
}
