#pragma once

#include <QDialog>
#include <QImage>

class ImageSurface3DWidget;

class ImageSurface3DDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ImageSurface3DDialog(
        const QImage& image,
        const QString& sourceName,
        QWidget* parent = nullptr);

    ImageSurface3DWidget* surfaceWidget() const { return surface_; }

private:
    void exportView();

    ImageSurface3DWidget* surface_ = nullptr;
};
