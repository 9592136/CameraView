#include "CameraWorker.h"

#include <QString>

#include <utility>

CameraWorker::CameraWorker(QObject* parent) : QObject(parent) {}

void CameraWorker::initialize()
{
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setInterval(15);
    connect(timer_, &QTimer::timeout, this, &CameraWorker::captureOne);
    refreshDevices();
}

QString CameraWorker::lastError() const
{
    return QString::fromStdWString(driver_.LastError());
}

void CameraWorker::refreshDevices()
{
    QString diagnostic;
    if (!driver_.Load()) {
        diagnostic = tr("MUCam SDK 加载失败：%1").arg(lastError());
        emit devicesReady({}, {}, diagnostic);
        return;
    }
    const std::vector<CameraDevice> devices = driver_.EnumerateDevices();
    QStringList labels;
    QVector<int> indices;
    for (const CameraDevice& device : devices) {
        labels.push_back(QString::fromStdWString(device.display_name));
        indices.push_back(device.index);
    }
    diagnostic = devices.empty() ? tr("未发现相机") : tr("发现 %1 台相机").arg(devices.size());
    emit devicesReady(labels, indices, diagnostic);
}

void CameraWorker::openCamera(int deviceIndex, double exposureMs)
{
    stopCamera();
    if (deviceIndex < 0) {
        emit cameraStateChanged(false, tr("请选择相机"));
        return;
    }
    if (!driver_.Open(deviceIndex, static_cast<float>(exposureMs))) {
        emit cameraStateChanged(false, tr("打开相机失败：%1").arg(lastError()));
        return;
    }
    sequence_ = 0;
    frame_delivery_pending_ = false;
    timer_->start();
    const CameraOpenInfo info = driver_.OpenInfo();
    emit cameraStateChanged(
        true,
        tr("相机已连接 · %1 × %2").arg(info.width).arg(info.height));
}

void CameraWorker::stopCamera()
{
    if (timer_) {
        timer_->stop();
    }
    frame_delivery_pending_ = false;
    const bool was_open = driver_.IsOpen();
    driver_.Close();
    if (was_open) {
        emit cameraStateChanged(false, tr("相机已停止"));
    }
}

void CameraWorker::setExposure(double value)
{
    const bool ok = driver_.IsOpen() && driver_.HasExposureControl() &&
        driver_.SetExposure(static_cast<float>(value));
    emit operationFinished(ok ? tr("曝光已更新") : tr("曝光设置失败"), ok);
}

void CameraWorker::autoExposure()
{
    const bool ok = driver_.IsOpen() && driver_.HasAutoExposureControl() && driver_.ApplyAutoExposure();
    emit operationFinished(ok ? tr("自动曝光已执行") : tr("自动曝光不可用"), ok);
}

void CameraWorker::setGain(double value)
{
    const bool ok = driver_.IsOpen() && driver_.HasGainControl() &&
        driver_.SetGain(static_cast<float>(value));
    emit operationFinished(ok ? tr("增益已更新") : tr("增益设置不可用"), ok);
}

void CameraWorker::whiteBalance()
{
    const bool ok = driver_.IsOpen() && driver_.HasWhiteBalanceControl() && driver_.ApplyWhiteBalance();
    emit operationFinished(ok ? tr("白平衡已执行") : tr("白平衡不可用"), ok);
}

void CameraWorker::frameConsumed()
{
    frame_delivery_pending_ = false;
}

void CameraWorker::shutdown()
{
    stopCamera();
}

void CameraWorker::captureOne()
{
    // A queued QImage owns roughly width * height * 3 bytes. Do not queue a
    // second full-resolution frame while the UI is presenting the previous
    // one, otherwise a fast camera can grow the event queue without bound.
    if (frame_delivery_pending_) {
        return;
    }

    ImageFrame frame;
    if (!driver_.GrabFrame(++sequence_, frame)) {
        if (!driver_.IsConnected()) {
            stopCamera();
            emit cameraStateChanged(false, tr("相机连接已断开"));
        }
        return;
    }
    if (!frame.IsValid() || frame.bgr.size() < static_cast<std::size_t>(frame.stride * frame.height)) {
        return;
    }
    const QImage view(
        frame.bgr.data(),
        frame.width,
        frame.height,
        frame.stride,
        QImage::Format_BGR888);
    QImage owned_view = view.copy();
    if (owned_view.isNull()) {
        return;
    }
    frame_delivery_pending_ = true;
    emit frameReady(std::move(owned_view), frame.sequence, frame.timestamp);
}
