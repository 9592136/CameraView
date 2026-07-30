#pragma once

#include "../domain/ImageFrame.h"
#include "../i18n/Localization.h"
#include "ModelDef.h"
#include "ModelTrainer.h"
#include "InferenceEngine.h"
#include "YoloEngine.h"
#include <vector>
#include <string>
#include <algorithm>
#include <map>

// ── AI panel actions: bridges UI controls to AI engine ───────────────────

struct AiPanelState {
    // Training
    std::vector<TrainingSample> training_samples;
    std::vector<AiLabel> labels;
    int current_label_id = 0;
    bool is_training = false;
    float training_progress = 0.0f;
    TrainingProgress train_progress_detail;
    std::map<int, int> label_sample_counts;   // label_id → sample count

    // Model management
    std::vector<ModelInfo> saved_models;
    int selected_model_index = -1;
    InferenceEngine engine;
    ModelManager model_manager;
    bool model_manager_loaded = false;

    // YOLO neural network
    YoloModel yolo_model;
    YoloDetector yolo_detector;
    YoloTrainer yolo_trainer;
    YoloTrainingConfig yolo_config;
    YoloTrainingProgress yolo_progress;
    bool yolo_model_loaded = false;

    // Inference results
    AiInferenceResult last_result;
    ClassificationResult last_classification;
    std::vector<DetectionBox> last_detections;
    SegmentationMask last_segmentation;

    // YOLO results (normalized coords)
    std::vector<YoloDetection> last_yolo_detections;

    // Visualization
    bool show_detection_boxes = true;
    bool show_segmentation_overlay = true;
    float seg_alpha = 0.5f;

    // Training config
    AiTaskType training_task_type = AiTaskType::Classification;
    AiBackend training_backend = AiBackend::SimpleKNN;
    int training_input_size = 64;
    int training_epochs = 10;
    float training_validation_split = 0.2f;

    void Reset() {
        training_samples.clear();
        labels.clear();
        current_label_id = 0;
        is_training = false;
        training_progress = 0.0f;
        train_progress_detail = {};
        label_sample_counts.clear();
        last_classification = {};
        last_detections.clear();
        last_segmentation = {};
        last_result = {};
        last_yolo_detections.clear();
    }

    void RecalcSampleCounts() {
        label_sample_counts.clear();
        for (const auto& s : training_samples) {
            label_sample_counts[s.label_id]++;
        }
    }
};

// ── Training helpers ─────────────────────────────────────────────────────

inline int FindLabelIndex(const std::vector<AiLabel>& labels, int label_id) {
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels[i].id == label_id) return static_cast<int>(i);
    }
    return -1;
}

inline std::wstring TaskTypeToString(AiTaskType t, UILanguage lang) {
    switch (t) {
    case AiTaskType::Classification: return GetLocStr(LocId::AI_TASK_CLASSIFICATION, lang);
    case AiTaskType::Detection:      return GetLocStr(LocId::AI_TASK_DETECTION, lang);
    case AiTaskType::Segmentation:   return GetLocStr(LocId::AI_TASK_SEGMENTATION, lang);
    }
    return GetLocStr(LocId::AI_TASK_UNKNOWN, lang);
}

inline std::wstring BackendToString(AiBackend b, UILanguage lang) {
    switch (b) {
    case AiBackend::SimpleKNN:    return GetLocStr(LocId::AI_BACKEND_KNN, lang);
    case AiBackend::SimpleSVM:    return GetLocStr(LocId::AI_BACKEND_SVM, lang);
    case AiBackend::SimpleKMeans: return GetLocStr(LocId::AI_BACKEND_KMEANS, lang);
    }
    return GetLocStr(LocId::AI_BACKEND_UNKNOWN, lang);
}

inline AiBackend StringToBackend(const std::wstring& s, UILanguage lang) {
    if (s == GetLocStr(LocId::AI_BACKEND_SVM, lang))    return AiBackend::SimpleSVM;
    if (s == GetLocStr(LocId::AI_BACKEND_KMEANS, lang)) return AiBackend::SimpleKMeans;
    return AiBackend::SimpleKNN;
}

// ── Color constants for labels ───────────────────────────────────────────

inline uint32_t GetLabelColor(int index) {
    static const uint32_t kLabelColors[] = {
        0x0000FF, 0x00FF00, 0xFF0000, 0x00FFFF,
        0xFF00FF, 0xFFFF00, 0x8080FF, 0x80FF80,
        0xFF8080, 0x80FFFF, 0xFF80FF, 0xFFFF80,
        0x4080FF, 0x40FF80, 0xFF4080, 0x80FF40,
    };
    constexpr int kNumColors = sizeof(kLabelColors) / sizeof(kLabelColors[0]);
    return kLabelColors[static_cast<size_t>(index) % kNumColors];
}
