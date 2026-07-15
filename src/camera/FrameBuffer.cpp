#include "FrameBuffer.h"

void FrameBuffer::Publish(ImageFrame frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    latest_frame_ = std::make_shared<ImageFrame>(std::move(frame));
}

ImageFrame FrameBuffer::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_frame_ ? *latest_frame_ : ImageFrame();
}

std::shared_ptr<const ImageFrame> FrameBuffer::SnapshotShared() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_frame_;
}

void FrameBuffer::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    latest_frame_.reset();
}

bool FrameBuffer::HasFrame() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_frame_ && latest_frame_->IsValid();
}
