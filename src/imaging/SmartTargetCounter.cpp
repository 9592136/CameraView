#include "SmartTargetCounter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#ifdef CAMERAVIEW_WITH_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace {

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() && frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

bool IsCanceled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
}

void ReportProgress(const std::function<void(int)>& callback, int value)
{
    if (callback) callback(std::clamp(value, 0, 100));
}

SmartTargetRegion ClampRegion(const SmartTargetRegion& region, int width, int height)
{
    const int left = std::clamp(region.x, 0, width);
    const int top = std::clamp(region.y, 0, height);
    const int right = std::clamp(region.x + region.width, 0, width);
    const int bottom = std::clamp(region.y + region.height, 0, height);
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

double IntersectionOverUnion(const SmartTargetRegion& lhs, const SmartTargetRegion& rhs)
{
    const int left = std::max(lhs.x, rhs.x);
    const int top = std::max(lhs.y, rhs.y);
    const int right = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const int bottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
    const double intersection = static_cast<double>(std::max(0, right - left)) * std::max(0, bottom - top);
    const double lhs_area = static_cast<double>(lhs.width) * lhs.height;
    const double rhs_area = static_cast<double>(rhs.width) * rhs.height;
    const double combined = lhs_area + rhs_area - intersection;
    return combined > 0.0 ? intersection / combined : 0.0;
}

bool RepresentsSameTarget(
    const SmartTargetRegion& lhs,
    const SmartTargetRegion& rhs,
    double iou_threshold)
{
    if (IntersectionOverUnion(lhs, rhs) >= iou_threshold) return true;
    const double lhs_x = lhs.x + lhs.width / 2.0;
    const double lhs_y = lhs.y + lhs.height / 2.0;
    const double rhs_x = rhs.x + rhs.width / 2.0;
    const double rhs_y = rhs.y + rhs.height / 2.0;
    const double center_distance = std::hypot(lhs_x - rhs_x, lhs_y - rhs_y);
    const double target_size = std::min(
        static_cast<double>(std::max(lhs.width, lhs.height)),
        static_cast<double>(std::max(rhs.width, rhs.height)));
    return target_size > 0.0 && center_distance <= target_size * 0.35;
}

#ifdef CAMERAVIEW_WITH_OPENCV

cv::Mat FrameToGray(const ImageFrame& frame)
{
    cv::Mat source(frame.height, frame.width, CV_8UC3,
        const_cast<unsigned char*>(frame.bgr.data()), static_cast<std::size_t>(frame.stride));
    cv::Mat gray;
    cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat GradientMagnitude(const cv::Mat& gray)
{
    cv::Mat gx;
    cv::Mat gy;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
    cv::Mat magnitude;
    cv::magnitude(gx, gy, magnitude);
    cv::Mat normalized;
    cv::normalize(magnitude, normalized, 0.0, 255.0, cv::NORM_MINMAX, CV_8U);
    return normalized;
}

void ReplaceInvalidScores(cv::Mat& scores)
{
    for (int row = 0; row < scores.rows; ++row) {
        float* values = scores.ptr<float>(row);
        for (int column = 0; column < scores.cols; ++column) {
            if (!std::isfinite(values[column])) values[column] = -1.0F;
        }
    }
}

std::vector<cv::Point> LocalPeaks(const cv::Mat& scores, double threshold, cv::Size template_size)
{
    cv::Mat threshold_mask;
    cv::compare(scores, threshold, threshold_mask, cv::CMP_GE);
    if (cv::countNonZero(threshold_mask) == 0) return {};

    const int kernel_width = std::max(3, template_size.width / 2 * 2 + 1);
    const int kernel_height = std::max(3, template_size.height / 2 * 2 + 1);
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {kernel_width, kernel_height});
    cv::Mat dilated;
    cv::dilate(scores, dilated, kernel);
    cv::Mat peak_mask;
    cv::compare(scores, dilated - 1e-6F, peak_mask, cv::CMP_GE);
    cv::bitwise_and(peak_mask, threshold_mask, peak_mask);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(peak_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<cv::Point> peaks;
    peaks.reserve(contours.size());
    for (const auto& contour : contours) {
        const cv::Rect bounds = cv::boundingRect(contour);
        double maximum = 0.0;
        cv::Point location;
        cv::minMaxLoc(scores(bounds), nullptr, &maximum, nullptr, &location);
        peaks.push_back(location + bounds.tl());
    }
    return peaks;
}

#endif

} // namespace

bool SmartTargetCounter::IsAvailable()
{
#ifdef CAMERAVIEW_WITH_OPENCV
    return true;
#else
    return false;
#endif
}

SmartTargetCountResult SmartTargetCounter::Count(
    const ImageFrame& frame,
    const std::vector<SmartTargetRegion>& samples,
    SmartTargetCountOptions options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    const auto started = std::chrono::steady_clock::now();
    SmartTargetCountResult result;
    result.available = IsAvailable();
    if (!result.available) {
        result.message = L"Smart target counting requires an OpenCV-enabled build.";
        return result;
    }
    if (!HasReadablePixels(frame)) {
        result.message = L"No readable image is available for target counting.";
        return result;
    }
    if (samples.empty()) {
        result.message = L"Select at least one target sample before counting.";
        return result;
    }

    options.similarity_threshold = std::clamp(options.similarity_threshold, 0.40, 0.99);
    options.scale_tolerance = std::clamp(options.scale_tolerance, 0.0, 0.40);
    options.scale_steps = std::clamp(options.scale_steps, 1, 9);
    options.duplicate_iou_threshold = std::clamp(options.duplicate_iou_threshold, 0.05, 0.90);
    options.maximum_results = std::clamp(options.maximum_results, 1, 50000);

#ifdef CAMERAVIEW_WITH_OPENCV
    try {
        const cv::Mat gray = FrameToGray(frame);
        const cv::Mat gradient = GradientMagnitude(gray);
        std::vector<SmartTargetMatch> candidates;

        const int scale_count = options.scale_tolerance > 0.0 ? options.scale_steps : 1;
        const int total_templates = static_cast<int>(samples.size()) * scale_count;
        int processed_templates = 0;
        ReportProgress(progress_callback, 0);

        for (std::size_t sample_index = 0; sample_index < samples.size(); ++sample_index) {
            const SmartTargetRegion sample = ClampRegion(samples[sample_index], frame.width, frame.height);
            if (sample.width < 6 || sample.height < 6) continue;
            const cv::Rect sample_rect(sample.x, sample.y, sample.width, sample.height);
            const cv::Mat gray_template_original = gray(sample_rect).clone();
            const cv::Mat gradient_template_original = gradient(sample_rect).clone();

            for (int scale_index = 0; scale_index < scale_count; ++scale_index) {
                if (IsCanceled(cancel_requested)) {
                    result.canceled = true;
                    result.message = L"Smart target counting was canceled.";
                    return result;
                }
                const double fraction = scale_count == 1 ? 0.5 :
                    static_cast<double>(scale_index) / static_cast<double>(scale_count - 1);
                const double scale = 1.0 - options.scale_tolerance + 2.0 * options.scale_tolerance * fraction;
                const int template_width = std::max(6, static_cast<int>(std::lround(sample.width * scale)));
                const int template_height = std::max(6, static_cast<int>(std::lround(sample.height * scale)));
                ++processed_templates;
                if (template_width > gray.cols || template_height > gray.rows) continue;

                cv::Mat gray_template;
                cv::Mat gradient_template;
                cv::resize(gray_template_original, gray_template, {template_width, template_height},
                    0.0, 0.0, cv::INTER_LINEAR);
                cv::resize(gradient_template_original, gradient_template, {template_width, template_height},
                    0.0, 0.0, cv::INTER_LINEAR);
                cv::Mat gray_scores;
                cv::Mat gradient_scores;
                cv::matchTemplate(gray, gray_template, gray_scores, cv::TM_CCOEFF_NORMED);
                cv::matchTemplate(gradient, gradient_template, gradient_scores, cv::TM_CCORR_NORMED);
                ReplaceInvalidScores(gray_scores);
                ReplaceInvalidScores(gradient_scores);
                cv::Mat scores = gray_scores * 0.58F + gradient_scores * 0.42F;
                ReplaceInvalidScores(scores);

                const auto peaks = LocalPeaks(scores, options.similarity_threshold,
                    {template_width, template_height});
                for (const cv::Point& peak : peaks) {
                    candidates.push_back({
                        SmartTargetRegion{peak.x, peak.y, template_width, template_height},
                        static_cast<double>(scores.at<float>(peak)),
                        sample_index});
                }
                result.evaluated_template_count = processed_templates;
                ReportProgress(progress_callback,
                    static_cast<int>(std::lround(processed_templates * 90.0 / std::max(1, total_templates))));
            }
        }

        if (processed_templates == 0) {
            result.message = L"All selected target samples are too small or outside the image.";
            return result;
        }

        std::sort(candidates.begin(), candidates.end(), [](const SmartTargetMatch& lhs, const SmartTargetMatch& rhs) {
            return lhs.confidence > rhs.confidence;
        });
        result.matches.reserve(std::min<std::size_t>(
            candidates.size(), static_cast<std::size_t>(options.maximum_results)));
        for (const SmartTargetMatch& candidate : candidates) {
            if (IsCanceled(cancel_requested)) {
                result.canceled = true;
                result.matches.clear();
                result.message = L"Smart target counting was canceled.";
                return result;
            }
            const bool duplicate = std::any_of(result.matches.begin(), result.matches.end(),
                [&candidate, &options](const SmartTargetMatch& accepted) {
                    return RepresentsSameTarget(candidate.region, accepted.region, options.duplicate_iou_threshold);
                });
            if (!duplicate) result.matches.push_back(candidate);
            if (static_cast<int>(result.matches.size()) >= options.maximum_results) break;
        }

        result.succeeded = true;
        result.message = L"Smart target counting completed.";
        ReportProgress(progress_callback, 100);
    } catch (const cv::Exception&) {
        result.matches.clear();
        result.succeeded = false;
        result.message = L"OpenCV could not complete smart target matching for this image.";
    }
#endif
    result.elapsed_milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}
