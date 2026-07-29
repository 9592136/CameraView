#include "OpenCvStitchBackend.h"

#include "ProcessingParameterRules.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#ifdef CAMERAVIEW_WITH_OPENCV
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/flann.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#endif

namespace {

bool IsCanceled(const std::atomic_bool* cancel_requested)
{
    return cancel_requested && cancel_requested->load();
}

void ReportProgress(const std::function<void(int)>& progress_callback, int percent)
{
    if (progress_callback) {
        progress_callback(std::clamp(percent, 0, 100));
    }
}

bool HasReadablePixels(const ImageFrame& frame)
{
    return frame.IsValid() &&
        frame.stride >= frame.width * 3 &&
        frame.bgr.size() >= static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
}

#ifdef CAMERAVIEW_WITH_OPENCV

constexpr double kPi = 3.14159265358979323846;
constexpr double kAutoFeatureConfidenceThreshold = 0.30;
constexpr int kSiftMaxDim = 1024;
constexpr int kEccMaxDim = 2048;
constexpr int kSiftFeatureCount = 5000;
constexpr double kRatioThreshold = 0.75;
constexpr double kRansacThreshold = 5.0;
constexpr int kMinimumMatches = 8;

struct SignedRange {
    double min_dx = 0.0;
    double max_dx = 0.0;
    double min_dy = 0.0;
    double max_dy = 0.0;
    bool valid = false;
};

struct Shift {
    double dx = 0.0;
    double dy = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

struct WarpEstimate {
    cv::Mat warp;
    double inlier_ratio = 0.0;
    int inliers = 0;
    bool valid = false;
};

struct PositionedImages {
    std::vector<cv::Mat> images;
    std::vector<cv::Point2d> positions;
    std::vector<Shift> pair_shifts;
};

cv::Mat FrameToBgrMat(const ImageFrame& frame)
{
    if (!HasReadablePixels(frame)) {
        return {};
    }
    cv::Mat source(
        frame.height,
        frame.width,
        CV_8UC3,
        const_cast<unsigned char*>(frame.bgr.data()),
        static_cast<std::size_t>(frame.stride));
    return source.clone();
}

ImageFrame MatToFrame(const cv::Mat& image)
{
    if (image.empty()) {
        return {};
    }
    cv::Mat bgr;
    if (image.channels() == 1) {
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
    } else {
        bgr = image;
    }
    if (bgr.depth() != CV_8U) {
        bgr.convertTo(bgr, CV_8U);
    }

    ImageFrame frame;
    frame.width = bgr.cols;
    frame.height = bgr.rows;
    frame.stride = (frame.width * 3 + 3) & ~3;
    frame.bgr.assign(static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height), 0);
    for (int y = 0; y < frame.height; ++y) {
        const unsigned char* src = bgr.ptr<unsigned char>(y);
        unsigned char* dst = frame.bgr.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride);
        std::copy(src, src + static_cast<std::ptrdiff_t>(frame.width) * 3, dst);
    }
    return frame;
}

cv::Mat ToGray(const cv::Mat& image)
{
    if (image.empty()) {
        return {};
    }
    if (image.channels() == 1) {
        return image.clone();
    }
    cv::Mat gray;
    cv::cvtColor(image, gray, image.channels() == 4 ? cv::COLOR_BGRA2GRAY : cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat ToFloat01(const cv::Mat& image)
{
    cv::Mat result;
    if (image.depth() == CV_8U) {
        image.convertTo(result, CV_32F, 1.0 / 255.0);
    } else if (image.depth() == CV_16U) {
        image.convertTo(result, CV_32F, 1.0 / 65535.0);
    } else {
        image.convertTo(result, CV_32F);
    }
    return result;
}

cv::Mat ToUint8MinMax(const cv::Mat& image)
{
    if (image.empty()) {
        return {};
    }
    if (image.depth() == CV_8U) {
        return image.clone();
    }
    cv::Mat float_image;
    image.convertTo(float_image, CV_32F);
    double min_value = 0.0;
    double max_value = 0.0;
    cv::minMaxLoc(float_image.reshape(1), &min_value, &max_value);
    if (max_value - min_value < 1e-8) {
        return cv::Mat::zeros(image.size(), CV_MAKETYPE(CV_8U, image.channels()));
    }
    cv::Mat normalized;
    float_image.convertTo(normalized, CV_32F, 255.0 / (max_value - min_value), -min_value * 255.0 / (max_value - min_value));
    cv::Mat result;
    normalized.convertTo(result, CV_MAKETYPE(CV_8U, image.channels()));
    return result;
}

void CropBothToSame(cv::Mat& left, cv::Mat& right)
{
    const int width = std::min(left.cols, right.cols);
    const int height = std::min(left.rows, right.rows);
    if (width <= 0 || height <= 0) {
        left.release();
        right.release();
        return;
    }
    left = left(cv::Rect(0, 0, width, height)).clone();
    right = right(cv::Rect(0, 0, width, height)).clone();
}

cv::Mat HanningWindowSqrt(int height, int width)
{
    cv::Mat window(height, width, CV_32F);
    for (int y = 0; y < height; ++y) {
        const double wy = height > 1 ? 0.5 - 0.5 * std::cos((2.0 * kPi * y) / (height - 1)) : 1.0;
        float* row = window.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const double wx = width > 1 ? 0.5 - 0.5 * std::cos((2.0 * kPi * x) / (width - 1)) : 1.0;
            row[x] = static_cast<float>(std::sqrt(std::max(0.0, wx * wy)));
        }
    }
    return window;
}

std::pair<double, double> SignedCandidates(int coordinate, int axis_length)
{
    const double value = coordinate <= axis_length / 2 ?
        static_cast<double>(coordinate) :
        static_cast<double>(coordinate - axis_length);
    const double alternate = value >= 0.0 ? value - axis_length : value + axis_length;
    return {value, alternate};
}

bool CandidateInRange(double value, double alternate, double min_value, double max_value)
{
    return (value >= min_value && value <= max_value) ||
        (alternate >= min_value && alternate <= max_value);
}

bool CoordinateInSignedRange(int x, int y, int width, int height, const SignedRange& range)
{
    if (!range.valid) {
        return true;
    }
    const auto [dx, dx_alt] = SignedCandidates(x, width);
    const auto [dy, dy_alt] = SignedCandidates(y, height);
    return CandidateInRange(dx, dx_alt, range.min_dx, range.max_dx) &&
        CandidateInRange(dy, dy_alt, range.min_dy, range.max_dy);
}

std::pair<double, double> SubpixelPeak(const cv::Mat& surface, int px, int py)
{
    double dx = 0.0;
    double dy = 0.0;
    if (px - 1 >= 0 && px + 1 < surface.cols) {
        const double left = surface.at<float>(py, px - 1);
        const double center = surface.at<float>(py, px);
        const double right = surface.at<float>(py, px + 1);
        const double denominator = left - 2.0 * center + right;
        if (std::abs(denominator) > 1e-12) {
            dx = 0.5 * (left - right) / denominator;
            if (!std::isfinite(dx) || std::abs(dx) > 1.0) {
                dx = 0.0;
            }
        }
    }
    if (py - 1 >= 0 && py + 1 < surface.rows) {
        const double top = surface.at<float>(py - 1, px);
        const double center = surface.at<float>(py, px);
        const double bottom = surface.at<float>(py + 1, px);
        const double denominator = top - 2.0 * center + bottom;
        if (std::abs(denominator) > 1e-12) {
            dy = 0.5 * (top - bottom) / denominator;
            if (!std::isfinite(dy) || std::abs(dy) > 1.0) {
                dy = 0.0;
            }
        }
    }
    return {static_cast<double>(px) + dx, static_cast<double>(py) + dy};
}

Shift PhaseCorrelationShift(const cv::Mat& image1, const cv::Mat& image2, const SignedRange& search_range)
{
    cv::Mat gray1 = ToGray(image1);
    cv::Mat gray2 = ToGray(image2);
    CropBothToSame(gray1, gray2);
    if (gray1.empty() || gray2.empty()) {
        return {};
    }

    cv::Mat a = ToFloat01(gray1);
    cv::Mat b = ToFloat01(gray2);
    const cv::Mat window = HanningWindowSqrt(a.rows, a.cols);
    a = a.mul(window);
    b = b.mul(window);
    a -= cv::mean(a)[0];
    b -= cv::mean(b)[0];

    cv::Mat dft_a;
    cv::Mat dft_b;
    cv::dft(a, dft_a, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(b, dft_b, cv::DFT_COMPLEX_OUTPUT);

    std::vector<cv::Mat> planes_a;
    std::vector<cv::Mat> planes_b;
    cv::split(dft_a, planes_a);
    cv::split(dft_b, planes_b);
    cv::Mat real = planes_a[0].mul(planes_b[0]) + planes_a[1].mul(planes_b[1]);
    cv::Mat imag = planes_a[1].mul(planes_b[0]) - planes_a[0].mul(planes_b[1]);
    cv::Mat magnitude;
    cv::magnitude(real, imag, magnitude);
    magnitude += 1e-8;
    real /= magnitude;
    imag /= magnitude;

    cv::Mat cross_power;
    cv::merge(std::vector<cv::Mat>{real, imag}, cross_power);
    cv::Mat correlation;
    cv::dft(cross_power, correlation, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);

    cv::Mat masked = correlation.clone();
    for (int y = 0; y < masked.rows; ++y) {
        float* row = masked.ptr<float>(y);
        for (int x = 0; x < masked.cols; ++x) {
            if (!CoordinateInSignedRange(x, y, masked.cols, masked.rows, search_range)) {
                row[x] = 0.0f;
            }
        }
    }

    double peak = 0.0;
    cv::Point peak_location;
    cv::minMaxLoc(masked, nullptr, &peak, nullptr, &peak_location);

    auto [cx, cx_alt] = SignedCandidates(peak_location.x, masked.cols);
    auto [cy, cy_alt] = SignedCandidates(peak_location.y, masked.rows);
    if (search_range.valid && !(cx >= search_range.min_dx && cx <= search_range.max_dx)) {
        cx = cx_alt;
    }
    if (search_range.valid && !(cy >= search_range.min_dy && cy <= search_range.max_dy)) {
        cy = cy_alt;
    }

    const auto [sub_x, sub_y] = SubpixelPeak(masked, peak_location.x, peak_location.y);
    const double dx = cx + (sub_x - peak_location.x);
    const double dy = cy + (sub_y - peak_location.y);

    cv::Mat background = masked.clone();
    for (int y = std::max(0, peak_location.y - 2); y <= std::min(masked.rows - 1, peak_location.y + 2); ++y) {
        float* row = background.ptr<float>(y);
        for (int x = std::max(0, peak_location.x - 2); x <= std::min(masked.cols - 1, peak_location.x + 2); ++x) {
            row[x] = 0.0f;
        }
    }
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(background, mean, stddev);
    const double confidence = std::clamp(
        (peak - mean[0]) / (peak + stddev[0] + 1e-8),
        0.0,
        1.0);

    return Shift{dx, dy, confidence, true};
}

double Median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t middle = values.size() / 2U;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    const double high = values[middle];
    if ((values.size() % 2U) != 0U) {
        return high;
    }
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle - 1U), values.end());
    return (values[middle - 1U] + high) * 0.5;
}

Shift FeatureBasedShift(const cv::Mat& image1, const cv::Mat& image2)
{
    cv::Mat gray1 = ToGray(image1);
    cv::Mat gray2 = ToGray(image2);
    if (gray1.empty() || gray2.empty()) {
        return {};
    }

    cv::Ptr<cv::ORB> orb = cv::ORB::create(3000);
    std::vector<cv::KeyPoint> keypoints1;
    std::vector<cv::KeyPoint> keypoints2;
    cv::Mat descriptors1;
    cv::Mat descriptors2;
    orb->detectAndCompute(gray1, cv::noArray(), keypoints1, descriptors1);
    orb->detectAndCompute(gray2, cv::noArray(), keypoints2, descriptors2);
    if (descriptors1.empty() || descriptors2.empty() || descriptors1.rows < 2 || descriptors2.rows < 2) {
        return {};
    }

    std::vector<std::vector<cv::DMatch>> raw_matches;
    cv::BFMatcher matcher(cv::NORM_HAMMING, false);
    matcher.knnMatch(descriptors1, descriptors2, raw_matches, 2);

    std::vector<cv::DMatch> good_matches;
    for (const std::vector<cv::DMatch>& pair : raw_matches) {
        if (pair.size() < 2U) {
            continue;
        }
        if (pair[0].distance < static_cast<float>(kRatioThreshold) * pair[1].distance) {
            good_matches.push_back(pair[0]);
        }
    }
    if (static_cast<int>(good_matches.size()) < kMinimumMatches) {
        return {};
    }

    std::vector<double> dxs;
    std::vector<double> dys;
    dxs.reserve(good_matches.size());
    dys.reserve(good_matches.size());
    for (const cv::DMatch& match : good_matches) {
        const cv::Point2f& p1 = keypoints1[static_cast<std::size_t>(match.queryIdx)].pt;
        const cv::Point2f& p2 = keypoints2[static_cast<std::size_t>(match.trainIdx)].pt;
        dxs.push_back(static_cast<double>(p1.x - p2.x));
        dys.push_back(static_cast<double>(p1.y - p2.y));
    }

    const double dx_median = Median(dxs);
    const double dy_median = Median(dys);
    const double inlier_threshold = std::max(3.0, 0.05 * static_cast<double>(std::max(image1.rows, image1.cols)));
    std::vector<double> inlier_dxs;
    std::vector<double> inlier_dys;
    for (std::size_t index = 0; index < dxs.size(); ++index) {
        if (std::hypot(dxs[index] - dx_median, dys[index] - dy_median) < inlier_threshold) {
            inlier_dxs.push_back(dxs[index]);
            inlier_dys.push_back(dys[index]);
        }
    }

    if (static_cast<int>(inlier_dxs.size()) < std::min(kMinimumMatches, 5)) {
        return Shift{dx_median, dy_median, static_cast<double>(good_matches.size()) / 3000.0, true};
    }
    return Shift{
        Median(inlier_dxs),
        Median(inlier_dys),
        static_cast<double>(inlier_dxs.size()) / static_cast<double>(good_matches.size()),
        true};
}

cv::Mat PreprocessMicroscopy(const cv::Mat& image)
{
    cv::Mat gray = ToGray(image);
    if (gray.empty()) {
        return {};
    }
    if (gray.depth() != CV_8U) {
        cv::normalize(gray, gray, 0, 255, cv::NORM_MINMAX, CV_8U);
    }
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(gray, gray);

    cv::Mat normalized_float;
    gray.convertTo(normalized_float, CV_32F);
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(normalized_float, mean, stddev);
    if (stddev[0] > 1e-6) {
        normalized_float = (normalized_float - mean[0]) / stddev[0];
    }
    cv::Mat normalized;
    cv::normalize(normalized_float, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
    return normalized;
}

cv::Mat DownscaleTo(const cv::Mat& image, int max_dimension, double& scale)
{
    const int longest = std::max(image.rows, image.cols);
    if (longest <= max_dimension || max_dimension <= 0) {
        scale = 1.0;
        return image.clone();
    }
    scale = static_cast<double>(max_dimension) / static_cast<double>(longest);
    const int width = std::max(1, static_cast<int>(std::lround(image.cols * scale)));
    const int height = std::max(1, static_cast<int>(std::lround(image.rows * scale)));
    cv::Mat result;
    cv::resize(image, result, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
    return result;
}

cv::Mat IdentityWarp(StitchTransformModel transform)
{
    if (transform == StitchTransformModel::Homography) {
        return cv::Mat::eye(3, 3, CV_32F);
    }
    cv::Mat warp = cv::Mat::zeros(2, 3, CV_32F);
    warp.at<float>(0, 0) = 1.0f;
    warp.at<float>(1, 1) = 1.0f;
    return warp;
}

cv::Mat ScaleWarp(const cv::Mat& warp, double scale, StitchTransformModel transform)
{
    cv::Mat result;
    warp.convertTo(result, CV_32F);
    if (std::abs(scale - 1.0) < 1e-12) {
        return result;
    }
    result.at<float>(0, 2) = static_cast<float>(result.at<float>(0, 2) * scale);
    result.at<float>(1, 2) = static_cast<float>(result.at<float>(1, 2) * scale);
    if (transform == StitchTransformModel::Homography) {
        result.at<float>(2, 0) = static_cast<float>(result.at<float>(2, 0) / scale);
        result.at<float>(2, 1) = static_cast<float>(result.at<float>(2, 1) / scale);
    }
    return result;
}

WarpEstimate SiftAlign(const cv::Mat& image1, const cv::Mat& image2, StitchTransformModel transform)
{
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create(kSiftFeatureCount);
    std::vector<cv::KeyPoint> keypoints1;
    std::vector<cv::KeyPoint> keypoints2;
    cv::Mat descriptors1;
    cv::Mat descriptors2;
    sift->detectAndCompute(image1, cv::noArray(), keypoints1, descriptors1);
    sift->detectAndCompute(image2, cv::noArray(), keypoints2, descriptors2);
    if (descriptors1.empty() || descriptors2.empty() || descriptors1.rows < 2 || descriptors2.rows < 2) {
        return {};
    }

    cv::FlannBasedMatcher matcher(
        cv::makePtr<cv::flann::KDTreeIndexParams>(5),
        cv::makePtr<cv::flann::SearchParams>(50));
    std::vector<std::vector<cv::DMatch>> raw_matches;
    try {
        matcher.knnMatch(descriptors1, descriptors2, raw_matches, 2);
    } catch (const cv::Exception&) {
        return {};
    }

    std::vector<cv::DMatch> good_matches;
    for (const std::vector<cv::DMatch>& pair : raw_matches) {
        if (pair.size() < 2U) {
            continue;
        }
        if (pair[0].distance < static_cast<float>(kRatioThreshold) * pair[1].distance) {
            good_matches.push_back(pair[0]);
        }
    }
    if (static_cast<int>(good_matches.size()) < kMinimumMatches) {
        WarpEstimate result;
        result.inliers = static_cast<int>(good_matches.size());
        return result;
    }

    std::vector<cv::Point2f> points1;
    std::vector<cv::Point2f> points2;
    points1.reserve(good_matches.size());
    points2.reserve(good_matches.size());
    for (const cv::DMatch& match : good_matches) {
        points1.push_back(keypoints1[static_cast<std::size_t>(match.queryIdx)].pt);
        points2.push_back(keypoints2[static_cast<std::size_t>(match.trainIdx)].pt);
    }

    cv::Mat inliers;
    cv::Mat warp;
    if (transform == StitchTransformModel::Homography) {
        warp = cv::findHomography(points2, points1, cv::RANSAC, kRansacThreshold, inliers);
    } else if (transform == StitchTransformModel::Affine) {
        warp = cv::estimateAffine2D(points2, points1, inliers, cv::RANSAC, kRansacThreshold, 2000, 0.995);
    } else {
        warp = cv::estimateAffinePartial2D(points2, points1, inliers, cv::RANSAC, kRansacThreshold, 2000, 0.995);
        if (!warp.empty()) {
            cv::Mat translation = IdentityWarp(StitchTransformModel::Translation);
            cv::Mat warp64;
            warp.convertTo(warp64, CV_64F);
            translation.at<float>(0, 2) = static_cast<float>(warp64.at<double>(0, 2));
            translation.at<float>(1, 2) = static_cast<float>(warp64.at<double>(1, 2));
            warp = translation;
        }
    }
    if (warp.empty()) {
        return {};
    }

    WarpEstimate result;
    warp.convertTo(result.warp, CV_32F);
    result.inliers = inliers.empty() ? 0 : cv::countNonZero(inliers);
    result.inlier_ratio = static_cast<double>(result.inliers) / static_cast<double>(good_matches.size());
    result.valid = true;
    return result;
}

int EccMotionType(StitchTransformModel transform)
{
    switch (transform) {
    case StitchTransformModel::Translation:
        return cv::MOTION_TRANSLATION;
    case StitchTransformModel::Homography:
        return cv::MOTION_HOMOGRAPHY;
    case StitchTransformModel::Affine:
    default:
        return cv::MOTION_AFFINE;
    }
}

double Correlation(const cv::Mat& image1, const cv::Mat& image2, const cv::Mat& warp, StitchTransformModel transform)
{
    try {
        cv::Mat warped;
        if (transform == StitchTransformModel::Homography) {
            cv::warpPerspective(image2, warped, warp, image1.size(), cv::INTER_LINEAR);
        } else {
            cv::warpAffine(image2, warped, warp, image1.size(), cv::INTER_LINEAR);
        }
        cv::Mat mask1;
        cv::Mat mask2;
        cv::compare(warped, 0, mask1, cv::CMP_GT);
        cv::compare(image1, 0, mask2, cv::CMP_GT);
        cv::Mat mask;
        cv::bitwise_and(mask1, mask2, mask);
        if (cv::countNonZero(mask) < 50) {
            return 0.0;
        }
        cv::Mat a;
        cv::Mat b;
        image1.convertTo(a, CV_32F);
        warped.convertTo(b, CV_32F);
        cv::Scalar mean_a;
        cv::Scalar std_a;
        cv::Scalar mean_b;
        cv::Scalar std_b;
        cv::meanStdDev(a, mean_a, std_a, mask);
        cv::meanStdDev(b, mean_b, std_b, mask);
        a = (a - mean_a[0]) / (std_a[0] + 1e-8);
        b = (b - mean_b[0]) / (std_b[0] + 1e-8);
        return std::clamp(cv::mean(a.mul(b), mask)[0] * 0.5 + 0.5, 0.0, 1.0);
    } catch (const cv::Exception&) {
        return 0.0;
    }
}

std::pair<cv::Mat, double> EccRefine(
    const cv::Mat& image1,
    const cv::Mat& image2,
    const cv::Mat& warp_init,
    StitchTransformModel transform)
{
    cv::Mat template_image;
    cv::Mat input_image;
    image1.convertTo(template_image, CV_32F);
    image2.convertTo(input_image, CV_32F);
    cv::Mat warp;
    warp_init.convertTo(warp, CV_32F);
    try {
        const cv::TermCriteria criteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 100, 1e-5);
        const double corr = cv::findTransformECC(
            template_image,
            input_image,
            warp,
            EccMotionType(transform),
            criteria,
            cv::noArray(),
            5);
        return {warp, corr};
    } catch (const cv::Exception&) {
        return {warp, Correlation(image1, image2, warp, transform)};
    }
}

std::pair<double, double> WarpToShift(const cv::Mat& warp, cv::Size shape, StitchTransformModel transform)
{
    const double cx = static_cast<double>(shape.width) / 2.0;
    const double cy = static_cast<double>(shape.height) / 2.0;
    if (transform == StitchTransformModel::Homography) {
        std::vector<cv::Point2f> source = {cv::Point2f(static_cast<float>(cx), static_cast<float>(cy))};
        std::vector<cv::Point2f> destination;
        cv::perspectiveTransform(source, destination, warp);
        return {static_cast<double>(destination[0].x) - cx, static_cast<double>(destination[0].y) - cy};
    }

    cv::Mat w;
    warp.convertTo(w, CV_64F);
    const double mapped_x = w.at<double>(0, 0) * cx + w.at<double>(0, 1) * cy + w.at<double>(0, 2);
    const double mapped_y = w.at<double>(1, 0) * cx + w.at<double>(1, 1) * cy + w.at<double>(1, 2);
    return {mapped_x - cx, mapped_y - cy};
}

bool IsInRange(const SignedRange& range, double dx, double dy)
{
    return !range.valid ||
        (dx >= range.min_dx - 1.0 && dx <= range.max_dx + 1.0 &&
         dy >= range.min_dy - 1.0 && dy <= range.max_dy + 1.0);
}

Shift RegisterSiftOnly(
    const cv::Mat& image1,
    const cv::Mat& image2,
    StitchTransformModel transform,
    const SignedRange& search_range)
{
    cv::Mat gray1 = PreprocessMicroscopy(image1);
    cv::Mat gray2 = PreprocessMicroscopy(image2);
    if (gray1.empty() || gray2.empty()) {
        return {};
    }

    double scale1 = 1.0;
    double scale2 = 1.0;
    cv::Mat sift1 = DownscaleTo(gray1, kSiftMaxDim, scale1);
    cv::Mat sift2 = DownscaleTo(gray2, kSiftMaxDim, scale2);
    (void)scale2;
    CropBothToSame(sift1, sift2);
    WarpEstimate sift = SiftAlign(sift1, sift2, transform);
    if (!sift.valid) {
        return PhaseCorrelationShift(image1, image2, search_range);
    }

    const cv::Mat warp = ScaleWarp(sift.warp, 1.0 / scale1, transform);
    const auto [dx, dy] = WarpToShift(warp, gray1.size(), transform);
    double confidence = std::clamp(sift.inlier_ratio, 0.0, 1.0);
    if (!IsInRange(search_range, dx, dy)) {
        confidence *= 0.3;
    }
    return Shift{dx, dy, confidence, true};
}

Shift RegisterMicroscopy(
    const cv::Mat& image1,
    const cv::Mat& image2,
    StitchTransformModel transform,
    const SignedRange& search_range)
{
    cv::Mat gray1 = PreprocessMicroscopy(image1);
    cv::Mat gray2 = PreprocessMicroscopy(image2);
    if (gray1.empty() || gray2.empty()) {
        return {};
    }

    double sift_scale1 = 1.0;
    double sift_scale2 = 1.0;
    cv::Mat sift1 = DownscaleTo(gray1, kSiftMaxDim, sift_scale1);
    cv::Mat sift2 = DownscaleTo(gray2, kSiftMaxDim, sift_scale2);
    (void)sift_scale2;
    CropBothToSame(sift1, sift2);
    WarpEstimate sift = SiftAlign(sift1, sift2, transform);
    if (!sift.valid) {
        return PhaseCorrelationShift(image1, image2, search_range);
    }

    const cv::Mat warp_init = ScaleWarp(sift.warp, 1.0 / sift_scale1, transform);
    double ecc_scale1 = 1.0;
    double ecc_scale2 = 1.0;
    cv::Mat ecc1 = DownscaleTo(gray1, kEccMaxDim, ecc_scale1);
    cv::Mat ecc2 = DownscaleTo(gray2, kEccMaxDim, ecc_scale2);
    (void)ecc_scale2;
    CropBothToSame(ecc1, ecc2);
    cv::Mat warp_work = ScaleWarp(warp_init, ecc_scale1, transform);
    auto [warp_refined, corr] = EccRefine(ecc1, ecc2, warp_work, transform);
    cv::Mat warp_final = ScaleWarp(warp_refined, 1.0 / ecc_scale1, transform);
    const auto [dx, dy] = WarpToShift(warp_final, gray1.size(), transform);
    double confidence = std::clamp(0.6 * sift.inlier_ratio + 0.4 * corr, 0.0, 1.0);
    if (!IsInRange(search_range, dx, dy)) {
        confidence *= 0.3;
    }
    return Shift{dx, dy, confidence, true};
}

bool IsFeatureLike(StitchRegistrationMethod method)
{
    return method == StitchRegistrationMethod::Feature ||
        method == StitchRegistrationMethod::Sift ||
        method == StitchRegistrationMethod::Micro;
}

Shift RegisterPair(
    const cv::Mat& image1,
    const cv::Mat& image2,
    StitchRegistrationMethod method,
    StitchTransformModel transform,
    const SignedRange& search_range)
{
    switch (method) {
    case StitchRegistrationMethod::Sift:
        return RegisterSiftOnly(image1, image2, transform, search_range);
    case StitchRegistrationMethod::Micro:
        return RegisterMicroscopy(image1, image2, transform, search_range);
    case StitchRegistrationMethod::Feature:
        return FeatureBasedShift(image1, image2);
    case StitchRegistrationMethod::Auto: {
        Shift feature = FeatureBasedShift(image1, image2);
        if (feature.valid && feature.confidence > kAutoFeatureConfidenceThreshold) {
            return feature;
        }
        Shift phase = PhaseCorrelationShift(image1, image2, search_range);
        return phase.valid ? phase : feature;
    }
    case StitchRegistrationMethod::Phase:
    default:
        return PhaseCorrelationShift(image1, image2, search_range);
    }
}

SignedRange InferSearchRange(const cv::Mat& image, int overlap_percent, bool horizontal)
{
    const double overlap = static_cast<double>(ProcessingParameterRules::ClampStitchOverlapPercent(overlap_percent)) / 100.0;
    const double tolerance = 0.2;
    const double width = static_cast<double>(std::max(1, image.cols));
    const double height = static_cast<double>(std::max(1, image.rows));
    SignedRange range;
    range.valid = true;
    if (horizontal) {
        const double dx = (1.0 - overlap) * width;
        range.min_dx = std::max(dx - tolerance * width, 0.05 * width);
        range.max_dx = std::min(dx + tolerance * width, 0.98 * width);
        range.min_dy = -tolerance * height;
        range.max_dy = tolerance * height;
    } else {
        const double dy = (1.0 - overlap) * height;
        range.min_dx = -tolerance * width;
        range.max_dx = tolerance * width;
        range.min_dy = std::max(dy - tolerance * height, 0.05 * height);
        range.max_dy = std::min(dy + tolerance * height, 0.98 * height);
    }
    return range;
}

void NormalizePositions(std::vector<cv::Point2d>& positions)
{
    if (positions.empty()) {
        return;
    }
    double min_x = positions.front().x;
    double min_y = positions.front().y;
    for (const cv::Point2d& position : positions) {
        min_x = std::min(min_x, position.x);
        min_y = std::min(min_y, position.y);
    }
    for (cv::Point2d& position : positions) {
        position.x -= min_x;
        position.y -= min_y;
    }
}

PositionedImages PositionLinear(
    std::vector<cv::Mat> images,
    const StitchProcessingOptions& options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    PositionedImages result;
    result.images = std::move(images);
    if (result.images.empty()) {
        return result;
    }
    result.positions.assign(result.images.size(), cv::Point2d(0.0, 0.0));
    if (result.images.size() == 1U) {
        return result;
    }

    const SignedRange horizontal_range = IsFeatureLike(options.registration_method)
        ? SignedRange{}
        : InferSearchRange(result.images.front(), options.overlap_percent, true);
    double x = 0.0;
    double y = 0.0;
    for (std::size_t index = 1; index < result.images.size(); ++index) {
        if (IsCanceled(cancel_requested)) {
            return {};
        }
        Shift shift = RegisterPair(
            result.images[index - 1U],
            result.images[index],
            options.registration_method,
            options.transform_model,
            horizontal_range);
        result.pair_shifts.push_back(shift);
        x += shift.dx;
        y += shift.dy;
        result.positions[index] = cv::Point2d(x, y);
        ReportProgress(progress_callback, static_cast<int>((index * 90U) / std::max<std::size_t>(1U, result.images.size() - 1U)));
    }
    NormalizePositions(result.positions);
    return result;
}

PositionedImages PositionGrid(
    std::vector<cv::Mat> images,
    StitchProcessingOptions options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    PositionedImages result;
    result.images = std::move(images);
    const int rows = std::max(1, options.grid_rows);
    const int cols = std::max(1, options.grid_cols);
    const int expected = rows * cols;
    if (static_cast<int>(result.images.size()) != expected) {
        return result;
    }

    const SignedRange horizontal_range = IsFeatureLike(options.registration_method)
        ? SignedRange{}
        : InferSearchRange(result.images.front(), options.overlap_percent, true);
    const SignedRange vertical_range = IsFeatureLike(options.registration_method)
        ? SignedRange{}
        : InferSearchRange(result.images.front(), options.overlap_percent, false);

    std::vector<Shift> horizontal_shifts(result.images.size());
    std::vector<Shift> vertical_shifts(result.images.size());
    const int pair_count = rows * std::max(0, cols - 1) + std::max(0, rows - 1) * cols;
    int processed_pairs = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (IsCanceled(cancel_requested)) {
                return {};
            }
            const int index = row * cols + col;
            if (col > 0) {
                Shift shift = RegisterPair(
                    result.images[static_cast<std::size_t>(row * cols + col - 1)],
                    result.images[static_cast<std::size_t>(index)],
                    options.registration_method,
                    options.transform_model,
                    horizontal_range);
                horizontal_shifts[static_cast<std::size_t>(index)] = shift;
                result.pair_shifts.push_back(shift);
                ++processed_pairs;
                ReportProgress(progress_callback, (processed_pairs * 90) / std::max(1, pair_count));
            }
            if (row > 0) {
                Shift shift = RegisterPair(
                    result.images[static_cast<std::size_t>((row - 1) * cols + col)],
                    result.images[static_cast<std::size_t>(index)],
                    options.registration_method,
                    options.transform_model,
                    vertical_range);
                vertical_shifts[static_cast<std::size_t>(index)] = shift;
                result.pair_shifts.push_back(shift);
                ++processed_pairs;
                ReportProgress(progress_callback, (processed_pairs * 90) / std::max(1, pair_count));
            }
        }
    }

    std::vector<cv::Point2d> positions(result.images.size(), cv::Point2d(0.0, 0.0));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const int index = row * cols + col;
            if (row == 0 && col == 0) {
                continue;
            }
            double sum_x = 0.0;
            double sum_y = 0.0;
            double sum_weight = 0.0;
            if (col > 0) {
                const int left_index = row * cols + col - 1;
                const Shift& shift = horizontal_shifts[static_cast<std::size_t>(index)];
                const double weight = std::max(shift.confidence, 1e-3);
                sum_x += (positions[static_cast<std::size_t>(left_index)].x + shift.dx) * weight;
                sum_y += (positions[static_cast<std::size_t>(left_index)].y + shift.dy) * weight;
                sum_weight += weight;
            }
            if (row > 0) {
                const int top_index = (row - 1) * cols + col;
                const Shift& shift = vertical_shifts[static_cast<std::size_t>(index)];
                const double weight = std::max(shift.confidence, 1e-3);
                sum_x += (positions[static_cast<std::size_t>(top_index)].x + shift.dx) * weight;
                sum_y += (positions[static_cast<std::size_t>(top_index)].y + shift.dy) * weight;
                sum_weight += weight;
            }
            if (sum_weight > 0.0) {
                positions[static_cast<std::size_t>(index)] = cv::Point2d(sum_x / sum_weight, sum_y / sum_weight);
            }
        }
    }
    NormalizePositions(positions);
    result.positions = std::move(positions);
    return result;
}

cv::Mat DistanceWeight(int height, int width)
{
    cv::Mat weight(height, width, CV_32F, cv::Scalar(0.0f));
    if (height <= 0 || width <= 0) {
        return weight;
    }
    int max_distance = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            max_distance = std::max(max_distance, std::min({y, height - 1 - y, x, width - 1 - x}));
        }
    }
    if (max_distance <= 0) {
        weight.setTo(1.0f);
        return weight;
    }
    for (int y = 0; y < height; ++y) {
        float* row = weight.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const int distance = std::min({y, height - 1 - y, x, width - 1 - x});
            row[x] = static_cast<float>(static_cast<double>(distance) / static_cast<double>(max_distance));
        }
    }
    return weight;
}

cv::Mat BlendImages(
    const std::vector<cv::Mat>& images,
    const std::vector<cv::Point2d>& positions,
    StitchBlendMode blend_mode)
{
    if (images.empty() || positions.size() != images.size()) {
        return {};
    }

    double max_x = 0.0;
    double max_y = 0.0;
    for (std::size_t index = 0; index < images.size(); ++index) {
        max_x = std::max(max_x, positions[index].x + images[index].cols);
        max_y = std::max(max_y, positions[index].y + images[index].rows);
    }
    const int canvas_width = std::max(1, static_cast<int>(std::ceil(max_x)));
    const int canvas_height = std::max(1, static_cast<int>(std::ceil(max_y)));
    cv::Mat canvas(canvas_height, canvas_width, CV_32FC3, cv::Scalar(0.0f, 0.0f, 0.0f));
    cv::Mat weight(canvas_height, canvas_width, CV_32F, cv::Scalar(0.0f));

    for (std::size_t index = 0; index < images.size(); ++index) {
        const int xi = static_cast<int>(std::lround(positions[index].x));
        const int yi = static_cast<int>(std::lround(positions[index].y));
        const cv::Mat& image = images[index];
        const int x0 = std::max(0, xi);
        const int y0 = std::max(0, yi);
        const int x1 = std::min(canvas_width, xi + image.cols);
        const int y1 = std::min(canvas_height, yi + image.rows);
        if (x0 >= x1 || y0 >= y1) {
            continue;
        }
        const int sx0 = x0 - xi;
        const int sy0 = y0 - yi;
        const int width = x1 - x0;
        const int height = y1 - y0;
        cv::Mat source_float = ToFloat01(image(cv::Rect(sx0, sy0, width, height)));
        cv::Mat image_weight = blend_mode == StitchBlendMode::Linear
            ? DistanceWeight(height, width)
            : cv::Mat(height, width, CV_32F, cv::Scalar(1.0f));

        cv::Mat canvas_roi = canvas(cv::Rect(x0, y0, width, height));
        cv::Mat weight_roi = weight(cv::Rect(x0, y0, width, height));
        for (int y = 0; y < height; ++y) {
            const cv::Vec3f* src_row = source_float.ptr<cv::Vec3f>(y);
            const float* weight_row = image_weight.ptr<float>(y);
            cv::Vec3f* dst_row = canvas_roi.ptr<cv::Vec3f>(y);
            float* dst_weight_row = weight_roi.ptr<float>(y);
            for (int x = 0; x < width; ++x) {
                const float pixel_weight = weight_row[x];
                dst_row[x] += src_row[x] * pixel_weight;
                dst_weight_row[x] += pixel_weight;
            }
        }
    }

    for (int y = 0; y < canvas.rows; ++y) {
        cv::Vec3f* canvas_row = canvas.ptr<cv::Vec3f>(y);
        const float* weight_row = weight.ptr<float>(y);
        for (int x = 0; x < canvas.cols; ++x) {
            canvas_row[x] /= std::max(weight_row[x], 1e-8f);
        }
    }
    return ToUint8MinMax(canvas);
}

OpenCvStitchResult BuildResult(
    const std::vector<StitchTile>& source_tiles,
    PositionedImages positioned,
    StitchBlendMode blend_mode,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    OpenCvStitchResult result;
    result.available = true;
    if (IsCanceled(cancel_requested) || positioned.images.empty() || positioned.positions.empty()) {
        return result;
    }

    result.positioned_tiles = source_tiles;
    for (std::size_t index = 0; index < result.positioned_tiles.size() && index < positioned.positions.size(); ++index) {
        result.positioned_tiles[index].offset_x = static_cast<int>(std::lround(positioned.positions[index].x));
        result.positioned_tiles[index].offset_y = static_cast<int>(std::lround(positioned.positions[index].y));
        result.positioned_tiles[index].estimated_position = index > 0U;
    }

    cv::Mat blended = BlendImages(positioned.images, positioned.positions, blend_mode);
    if (IsCanceled(cancel_requested) || blended.empty()) {
        return result;
    }

    result.constraint_count = static_cast<int>(positioned.pair_shifts.size());
    result.image = MatToFrame(blended);
    result.succeeded = result.image.IsValid();
    ReportProgress(progress_callback, 100);
    return result;
}

#endif

} // namespace

bool OpenCvStitchBackend::IsAvailable()
{
#ifdef CAMERAVIEW_WITH_OPENCV
    return true;
#else
    return false;
#endif
}

OpenCvStitchResult OpenCvStitchBackend::Stitch(
    const std::vector<StitchTile>& tiles,
    StitchProcessingOptions options,
    const std::atomic_bool* cancel_requested,
    const std::function<void(int)>& progress_callback)
{
    OpenCvStitchResult result;
#ifdef CAMERAVIEW_WITH_OPENCV
    result.available = true;
    ReportProgress(progress_callback, 0);
    if (tiles.empty() || IsCanceled(cancel_requested)) {
        return result;
    }

    options.overlap_percent = ProcessingParameterRules::ClampStitchOverlapPercent(options.overlap_percent);
    options.grid_rows = std::clamp(options.grid_rows, 1, 50);
    options.grid_cols = std::clamp(options.grid_cols, 1, 50);

    std::vector<cv::Mat> images;
    images.reserve(tiles.size());
    for (const StitchTile& tile : tiles) {
        cv::Mat image = FrameToBgrMat(tile.frame);
        if (image.empty()) {
            return result;
        }
        images.push_back(std::move(image));
    }

    auto alignment_progress = [&progress_callback](int percent) {
        ReportProgress(progress_callback, (percent * 90) / 100);
    };

    PositionedImages positioned = options.layout_mode == StitchLayoutMode::Linear
        ? PositionLinear(std::move(images), options, cancel_requested, alignment_progress)
        : PositionGrid(std::move(images), options, cancel_requested, alignment_progress);
    if (IsCanceled(cancel_requested)) {
        return result;
    }
    return BuildResult(tiles, std::move(positioned), options.blend_mode, cancel_requested, progress_callback);
#else
    (void)tiles;
    (void)options;
    (void)cancel_requested;
    (void)progress_callback;
    return result;
#endif
}