#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

// ── AI task types ────────────────────────────────────────────────────────

enum class AiTaskType {
    Classification,    // Whole-image classification
    Detection,         // Bounding-box object detection
    Segmentation       // Pixel-level segmentation
};

enum class AiBackend {
    SimpleKNN,         // Built-in k-NN classifier (no external deps)
    SimpleSVM,         // Built-in linear SVM
    SimpleKMeans       // Built-in k-means clustering for segmentation
};

// ── Label definition ─────────────────────────────────────────────────────

struct AiLabel {
    int id = 0;
    std::wstring name;
    uint32_t color = 0xFF0000;  // BGR for visualization
};

// ── Classification result ────────────────────────────────────────────────

struct ClassificationResult {
    int label_id = -1;
    std::wstring label_name;
    float confidence = 0.0f;
};

// ── Detection result (bounding box) ──────────────────────────────────────

struct DetectionBox {
    int label_id = -1;
    std::wstring label_name;
    float confidence = 0.0f;
    int x = 0, y = 0, width = 0, height = 0;
};

// ── Segmentation mask ────────────────────────────────────────────────────

struct SegmentationMask {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;  // label_id per pixel (0 = background)
};

// ── Model metadata ───────────────────────────────────────────────────────

struct ModelInfo {
    std::wstring name;
    std::wstring file_path;
    AiTaskType task_type = AiTaskType::Classification;
    AiBackend backend = AiBackend::SimpleKNN;
    std::vector<AiLabel> labels;
    int input_width = 64;
    int input_height = 64;
    int feature_dim = 0;  // auto-computed from input size * 3 (RGB)
    float accuracy = 0.0f;
    int training_samples = 0;
};

// ── Feature vector helper ────────────────────────────────────────────────

struct FeatureVector {
    std::vector<float> values;

    float Dot(const FeatureVector& other) const {
        float sum = 0;
        size_t n = std::min(values.size(), other.values.size());
        for (size_t i = 0; i < n; ++i) sum += values[i] * other.values[i];
        return sum;
    }

    float Norm() const {
        float sum = 0;
        for (float v : values) sum += v * v;
        return std::sqrt(sum);
    }

    float CosineSimilarity(const FeatureVector& other) const {
        float n1 = Norm(), n2 = other.Norm();
        if (n1 < 1e-6f || n2 < 1e-6f) return 0;
        return Dot(other) / (n1 * n2);
    }

    float EuclideanDistance(const FeatureVector& other) const {
        float sum = 0;
        size_t n = std::min(values.size(), other.values.size());
        for (size_t i = 0; i < n; ++i) {
            float d = values[i] - other.values[i];
            sum += d * d;
        }
        return std::sqrt(sum);
    }
};

// ── Training sample ───────────────────────────────────────────────────────

struct TrainingSample {
    int label_id = -1;
    FeatureVector features;
    std::wstring source_path;
    int original_width = 0;
    int original_height = 0;
};
