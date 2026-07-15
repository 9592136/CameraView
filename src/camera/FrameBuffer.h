#pragma once

#include "../domain/ImageFrame.h"

#include <memory>
#include <mutex>

class FrameBuffer {
public:
    void Publish(ImageFrame frame);
    ImageFrame Snapshot() const;
    std::shared_ptr<const ImageFrame> SnapshotShared() const;
    void Clear();
    bool HasFrame() const;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const ImageFrame> latest_frame_;
};
