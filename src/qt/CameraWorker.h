#pragma once

#include "camera/MUCamCameraDriver.h"

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <array>
#include <memory>

class CameraWorker final : public QObject {
    Q_OBJECT

public:
    explicit CameraWorker(QObject* parent = nullptr);

public slots:
    void initialize();
    void refreshDevices();
    void openCamera(int deviceIndex, double exposureMs);
    void stopCamera();
    void setExposure(double value);
    void autoExposure();
    void setGain(double value);
    void whiteBalance();
    void frameConsumed();
    void shutdown();

signals:
    void devicesReady(QStringList labels, QVector<int> indices, QString diagnostic);
    void frameReady(QImage image, quint64 sequence, quint32 timestamp);
    void cameraStateChanged(bool opened, QString message);
    void operationFinished(QString message, bool success);
    void exposureApplied(double value, bool success, QString message);

private slots:
    void captureOne();

private:
    QString lastError() const;
    std::shared_ptr<ImageFrame> acquireFrameBuffer();

    MUCamCameraDriver driver_;
    QTimer* timer_ = nullptr;
    std::array<std::shared_ptr<ImageFrame>, 3> frame_pool_;
    std::size_t next_frame_pool_index_ = 0;
    quint64 sequence_ = 0;
    bool frame_delivery_pending_ = false;
};
