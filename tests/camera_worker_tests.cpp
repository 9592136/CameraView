#include "qt/CameraWorker.h"

#include <QCoreApplication>

#include <algorithm>
#include <iostream>
#include <memory>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

class FakeCameraDriver final : public ICameraDriver {
public:
    FakeCameraDriver()
    {
        capabilities.resolutions = {{0, 640, 480}, {1, 320, 240}};
        capabilities.exposure_minimum = 0.1f;
        capabilities.exposure_maximum = 100.0f;
        capabilities.gain_values = {1.0f, 2.0f, 4.0f};
        capabilities.offset_minimum = -16;
        capabilities.offset_maximum = 16;
        capabilities.frame_format = 5;
        capabilities.color = true;
        capabilities.has_exposure = true;
        capabilities.has_auto_exposure = true;
        capabilities.has_gain = true;
        capabilities.has_offset = true;
        capabilities.has_white_balance = true;
        capabilities.has_roi = true;
        capabilities.has_trigger = true;
        capabilities.has_flip = true;
        capabilities.has_mirror = true;
    }

    bool Load() override { return load_succeeds; }
    std::wstring LastError() const override { return error; }
    CameraSdkDiagnostics Diagnostics() const override { return {}; }
    std::vector<CameraDevice> EnumerateDevices() override { return devices; }
    bool Open(int device_index, float initial_exposure_ms) override
    {
        ++open_calls;
        if (!open_succeeds || device_index < 0) return false;
        opened = true;
        connected = true;
        configuration.exposure_ms = initial_exposure_ms;
        return true;
    }
    void Close() override { opened = false; connected = false; }
    bool IsOpen() const override { return opened; }
    bool IsConnected() const override { return connected; }
    CameraOpenInfo OpenInfo() const override { return info; }
    CameraCapabilities Capabilities() const override { return capabilities; }
    CameraConfiguration Configuration() const override { return configuration; }
    bool Configure(const CameraConfiguration& value) override
    {
        ++configure_calls;
        if (!configure_succeeds) return false;
        configuration = value;
        if (value.roi.Enabled()) {
            info.width = value.roi.width;
            info.height = value.roi.height;
        } else {
            const auto selected = std::find_if(
                capabilities.resolutions.begin(), capabilities.resolutions.end(),
                [&value](const CameraResolutionOption& option) {
                    return option.index == value.resolution_index;
                });
            if (selected != capabilities.resolutions.end()) {
                info.width = selected->width;
                info.height = selected->height;
            }
        }
        return true;
    }
    bool HasExposureControl() const override { return capabilities.has_exposure; }
    bool GetExposureRange(float& minimum, float& maximum) const override
    {
        minimum = capabilities.exposure_minimum;
        maximum = capabilities.exposure_maximum;
        return capabilities.has_exposure;
    }
    bool SetExposure(float value) override
    {
        ++exposure_calls;
        configuration.exposure_ms = value;
        return capabilities.has_exposure;
    }
    bool HasAutoExposureControl() const override { return capabilities.has_auto_exposure; }
    bool ApplyAutoExposure() override { return capabilities.has_auto_exposure; }
    bool HasGainControl() const override { return capabilities.has_gain; }
    bool SetGain(float value) override { return SetRgbGain(value, value, value); }
    bool SetRgbGain(float red, float green, float blue) override
    {
        ++gain_calls;
        configuration.red_gain = red;
        configuration.green_gain = green;
        configuration.blue_gain = blue;
        return capabilities.has_gain;
    }
    bool SetRgbOffset(int red, int green, int blue) override
    {
        ++offset_calls;
        configuration.red_offset = red;
        configuration.green_offset = green;
        configuration.blue_offset = blue;
        return capabilities.has_offset;
    }
    bool HasWhiteBalanceControl() const override { return capabilities.has_white_balance; }
    bool ApplyWhiteBalance() override { return capabilities.has_white_balance; }
    bool GrabFrame(uint64_t sequence, ImageFrame& frame) override
    {
        ++grab_calls;
        if (!opened || !connected || !grab_succeeds) return false;
        frame.width = info.width;
        frame.height = info.height;
        frame.stride = (info.width * 3 + 3) & ~3;
        frame.sequence = sequence;
        frame.bgr.assign(static_cast<std::size_t>(frame.stride * frame.height), 127);
        return true;
    }

    bool load_succeeds = true;
    bool open_succeeds = true;
    bool configure_succeeds = true;
    bool grab_succeeds = true;
    bool opened = false;
    bool connected = false;
    std::wstring error = L"simulated failure";
    std::vector<CameraDevice> devices{{0, 42, L"Simulated color camera"}};
    CameraCapabilities capabilities;
    CameraConfiguration configuration;
    CameraOpenInfo info{0, 42, 640, 480};
    int open_calls = 0;
    int configure_calls = 0;
    int exposure_calls = 0;
    int gain_calls = 0;
    int offset_calls = 0;
    int grab_calls = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    {
        auto unavailable = std::make_unique<FakeCameraDriver>();
        unavailable->load_succeeds = false;
        bool no_sdk_reported = false;
        CameraWorker worker(std::move(unavailable));
        QObject::connect(&worker, &CameraWorker::devicesReady,
            [&no_sdk_reported](const QStringList& labels, const QVector<int>&, const QString& diagnostic) {
                no_sdk_reported = labels.isEmpty() && diagnostic.contains(QStringLiteral("加载失败"));
            });
        worker.initialize();
        if (!no_sdk_reported) return fail("Camera worker did not report a missing SDK inline.");
    }

    auto fake = std::make_unique<FakeCameraDriver>();
    FakeCameraDriver* driver = fake.get();
    CameraWorker worker(std::move(fake));
    int device_count = -1;
    int capability_updates = 0;
    int configuration_updates = 0;
    int delivered_frames = 0;
    QString last_state;
    bool last_configuration_success = false;
    QObject::connect(&worker, &CameraWorker::devicesReady,
        [&device_count](const QStringList& labels, const QVector<int>&, const QString&) {
            device_count = labels.size();
        });
    QObject::connect(&worker, &CameraWorker::cameraCapabilitiesChanged,
        [&capability_updates](const CameraCapabilities&, const CameraConfiguration&, const CameraOpenInfo&) {
            ++capability_updates;
        });
    QObject::connect(&worker, &CameraWorker::configurationFinished,
        [&configuration_updates, &last_configuration_success](
            const CameraConfiguration&, bool success, const QString&) {
            ++configuration_updates;
            last_configuration_success = success;
        });
    QObject::connect(&worker, &CameraWorker::cameraStateChanged,
        [&last_state](bool, const QString& message) { last_state = message; });
    QObject::connect(&worker, &CameraWorker::frameReady,
        [&delivered_frames](const QImage&, quint64, quint32) { ++delivered_frames; });

    worker.initialize();
    if (device_count != 1) return fail("Camera worker did not enumerate the simulated device.");

    driver->open_succeeds = false;
    worker.openCamera(0, 12.5);
    if (!last_state.contains(QStringLiteral("打开相机失败"))) {
        return fail("Camera worker did not report a simulated open failure.");
    }
    driver->open_succeeds = true;
    worker.openCamera(0, 12.5);
    if (!driver->opened || capability_updates != 1 || !last_state.contains(QStringLiteral("已连接"))) {
        return fail("Camera worker did not publish capabilities after opening.");
    }

    CameraConfiguration software = driver->configuration;
    software.resolution_index = 1;
    software.trigger_mode = CameraTriggerMode::Software;
    software.roi = {10, 20, 120, 80};
    worker.reconfigureCamera(software);
    if (!last_configuration_success || driver->configure_calls != 1 ||
        !last_state.contains(QStringLiteral("等待软件触发")) ||
        driver->info.width != 120 || driver->info.height != 80) {
        return fail("Camera worker did not safely apply a software-trigger ROI configuration.");
    }
    worker.captureOneFrame();
    if (delivered_frames != 1 || driver->grab_calls != 1) {
        return fail("Software trigger did not deliver exactly one frame.");
    }

    driver->configure_succeeds = false;
    CameraConfiguration rejected = software;
    rejected.roi = {0, 0, 40, 40};
    worker.reconfigureCamera(rejected);
    if (last_configuration_success || configuration_updates < 2 ||
        driver->configuration.roi.width != 120) {
        return fail("Failed reconfiguration did not preserve the last good configuration.");
    }

    worker.setExposure(25.0);
    worker.setRgbGain(2.0, 4.0, 2.0);
    worker.setRgbOffset(1, 2, 3);
    if (driver->exposure_calls != 1 || driver->gain_calls != 1 || driver->offset_calls != 1) {
        return fail("Camera parameter operations did not reach the injected driver once.");
    }

    driver->connected = false;
    worker.captureOneFrame();
    if (!last_state.contains(QStringLiteral("连接已断开")) || driver->opened) {
        return fail("Unexpected camera disconnect was not reported and closed safely.");
    }
    return 0;
}
