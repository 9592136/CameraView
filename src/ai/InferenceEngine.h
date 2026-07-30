#pragma once

#include "ModelDef.h"
#include "ModelTrainer.h"
#include "../domain/ImageFrame.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

// ── Unified inference result ──────────────────────────────────────────────

struct AiInferenceResult {
    bool success = false;
    ClassificationResult classification;           // for classification
    std::vector<DetectionBox> detections;           // for detection
    SegmentationMask segmentation;                  // for segmentation
    int64_t elapsed_ms = 0;                         // inference time
};

// ── Model version record ──────────────────────────────────────────────────

struct ModelVersionRecord {
    int version_id = 0;
    std::wstring file_path;
    std::wstring created_at;
    int training_samples = 0;
    float train_accuracy = 0.0f;
    float val_accuracy = 0.0f;
    bool deployed = false;
    std::wstring notes;
};

// ── Training progress ─────────────────────────────────────────────────────

enum class TrainPhase { Idle, Preparing, Training, Validating, Done, Failed };

struct TrainingProgress {
    TrainPhase phase = TrainPhase::Idle;
    int epoch_current = 0;
    int epoch_total = 0;
    float loss = 0.0f;
    float accuracy = 0.0f;
    std::wstring status_message;
    bool cancelled = false;
};

// ── Inference engine: load model + run unified inference ──────────────────

class InferenceEngine {
public:
    InferenceEngine() = default;

    // ── Model lifecycle ────────────────────────────────────────────────

    bool LoadModel(const std::wstring& path) {
        std::vector<TrainingSample> samples;
        if (!::LoadModel(path, model_info_, samples)) return false;
        samples_ = std::move(samples);
        RebuildClassifier();
        model_loaded_path_ = path;
        return true;
    }

    bool LoadModelRaw(const ModelInfo& info, const std::vector<TrainingSample>& samples) {
        model_info_ = info;
        samples_ = samples;
        RebuildClassifier();
        model_loaded_path_ = info.file_path;
        return true;
    }

    void Unload() {
        knn_.reset();
        svm_.reset();
        model_info_ = ModelInfo{};
        samples_.clear();
        model_loaded_path_.clear();
    }

    const ModelInfo& GetModelInfo() const { return model_info_; }
    bool IsLoaded() const { return model_info_.feature_dim > 0; }
    const std::wstring& GetModelPath() const { return model_loaded_path_; }

    // ── Unified inference (from ImageFrame) ─────────────────────────────

    AiInferenceResult RunInference(const ImageFrame& frame,
                                    float confidence_threshold = 0.5f,
                                    int seg_clusters = 4) {
        AiInferenceResult result;
        if (!frame.IsValid() || !IsLoaded()) return result;

        auto t0 = std::chrono::steady_clock::now();

        // BGR → RGB data pointer
        const uint8_t* data = frame.bgr.data();
        int w = frame.width, h = frame.height, stride = frame.stride;

        switch (model_info_.task_type) {
        case AiTaskType::Classification:
            result.classification = Classify(data, w, h, stride);
            result.success = result.classification.confidence >= confidence_threshold;
            break;
        case AiTaskType::Detection:
            result.detections = Detect(data, w, h, stride, confidence_threshold);
            result.success = !result.detections.empty();
            break;
        case AiTaskType::Segmentation:
            result.segmentation = Segment(data, w, h, stride, seg_clusters);
            result.success = result.segmentation.width > 0;
            break;
        }

        auto t1 = std::chrono::steady_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        return result;
    }

    // ── Classification (raw) ────────────────────────────────────────────

    ClassificationResult Classify(const uint8_t* rgb_data,
                                   int src_w, int src_h, int src_stride)
    {
        auto features = ExtractRGBFeatures(rgb_data, src_w, src_h, src_stride,
                                           model_info_.input_width, model_info_.input_height);
        FeatureVector fv;
        fv.values = std::move(features);

        ClassificationResult result;
        if (knn_) result = knn_->Predict(fv);
        else if (svm_) result = svm_->Predict(fv);

        for (const auto& label : model_info_.labels) {
            if (label.id == result.label_id) { result.label_name = label.name; break; }
        }
        return result;
    }

    // ── Detection (sliding window) ─────────────────────────────────────

    std::vector<DetectionBox> Detect(const uint8_t* rgb_data,
                                      int src_w, int src_h, int src_stride,
                                      float confidence_threshold = 0.5f,
                                      int window_scale = 1)
    {
        std::vector<DetectionBox> results;
        if (!IsLoaded() || model_info_.task_type != AiTaskType::Detection) return results;

        int win_w = model_info_.input_width * window_scale;
        int win_h = model_info_.input_height * window_scale;
        int stride_x = std::max(8, win_w / 3);
        int stride_y = std::max(8, win_h / 3);

        for (int y = 0; y + win_h <= src_h; y += stride_y) {
            for (int x = 0; x + win_w <= src_w; x += stride_x) {
                // Extract features directly from source region (no intermediate copy)
                const uint8_t* window_start = rgb_data +
                    static_cast<size_t>(y) * src_stride +
                    static_cast<size_t>(x) * 3;

                auto features = ExtractRGBFeatures(window_start, win_w, win_h, src_stride,
                                                   model_info_.input_width, model_info_.input_height);
                FeatureVector fv;
                fv.values = std::move(features);

                ClassificationResult cls;
                if (knn_) cls = knn_->Predict(fv);
                else if (svm_) cls = svm_->Predict(fv);

                if (cls.confidence >= confidence_threshold) {
                    DetectionBox box;
                    box.label_id = cls.label_id;
                    box.confidence = cls.confidence;
                    box.x = x; box.y = y;
                    box.width = win_w; box.height = win_h;
                    for (const auto& label : model_info_.labels) {
                        if (label.id == cls.label_id) { box.label_name = label.name; break; }
                    }
                    results.push_back(box);
                }
            }
        }
        return NMS(results, 0.3f);
    }

    // ── Segmentation ───────────────────────────────────────────────────

    SegmentationMask Segment(const uint8_t* rgb_data,
                              int src_w, int src_h, int src_stride,
                              int k = 4)
    {
        // Clamp k to a reasonable range: at least 2, at most min(sqrt(N), n_labels or 16)
        int max_clusters = std::min(16, static_cast<int>(std::sqrt(
            static_cast<size_t>(src_w) * src_h)));
        if (!model_info_.labels.empty())
            max_clusters = std::min(max_clusters,
                std::max(2, static_cast<int>(model_info_.labels.size())));
        int effective_k = std::max(2, std::min(k, max_clusters));

        SegmentationMask mask;
        mask.width = src_w;
        mask.height = src_h;
        mask.data.resize(static_cast<size_t>(src_w) * src_h, 0);
        if (!IsLoaded() || model_info_.task_type != AiTaskType::Segmentation) return mask;

        std::vector<FeatureVector> pixels;
        pixels.reserve(static_cast<size_t>(src_w) * src_h);
        for (int y = 0; y < src_h; ++y) {
            for (int x = 0; x < src_w; ++x) {
                const uint8_t* px = rgb_data +
                    static_cast<size_t>(y) * src_stride +
                    static_cast<size_t>(x) * 3;
                FeatureVector fv;
                fv.values = {px[2]/255.0f, px[1]/255.0f, px[0]/255.0f};
                pixels.push_back(fv);
            }
        }

        SimpleKMeans kmeans;
        kmeans.Cluster(pixels, effective_k);
        const auto& assignments = kmeans.Assignments();

        for (size_t i = 0; i < assignments.size(); ++i)
            mask.data[i] = static_cast<uint8_t>(assignments[i] + 1);

        return mask;
    }

    // ── Model evaluation ───────────────────────────────────────────────

    float EvaluateAccuracy(const std::vector<TrainingSample>& test_samples) {
        if (test_samples.empty() || !IsLoaded()) return 0.0f;
        int correct = 0;
        for (const auto& sample : test_samples) {
            ClassificationResult r;
            if (knn_) r = knn_->Predict(sample.features);
            else if (svm_) r = svm_->Predict(sample.features);
            if (r.label_id == sample.label_id) ++correct;
        }
        return static_cast<float>(correct) / test_samples.size();
    }

private:
    void RebuildClassifier() {
        knn_.reset();
        svm_.reset();
        if (samples_.empty()) return;

        switch (model_info_.backend) {
        case AiBackend::SimpleKNN:
            knn_ = std::make_unique<SimpleKNN>();
            knn_->Train(samples_);
            break;
        case AiBackend::SimpleSVM: {
            svm_ = std::make_unique<SimpleSVM>();
            int num_labels = std::max(1, static_cast<int>(model_info_.labels.size()));
            svm_->Train(samples_, num_labels);
            break;
        }
        default:
            knn_ = std::make_unique<SimpleKNN>();
            knn_->Train(samples_);
            break;
        }
    }

    static std::vector<DetectionBox> NMS(const std::vector<DetectionBox>& boxes, float iou_threshold) {
        // Make mutable copy for sorting
        std::vector<DetectionBox> sorted = boxes;
        std::sort(sorted.begin(), sorted.end(),
            [](const DetectionBox& a, const DetectionBox& b) { return a.confidence > b.confidence; });

        std::vector<DetectionBox> result;
        std::vector<bool> suppressed(sorted.size(), false);
        for (size_t i = 0; i < sorted.size(); ++i) {
            if (suppressed[i]) continue;
            result.push_back(sorted[i]);
            for (size_t j = i + 1; j < sorted.size(); ++j) {
                if (suppressed[j]) continue;
                if (sorted[i].label_id != sorted[j].label_id) continue;
                float iou = ComputeIOU(sorted[i], sorted[j]);
                if (iou > iou_threshold) suppressed[j] = true;
            }
        }
        return result;
    }

    static float ComputeIOU(const DetectionBox& a, const DetectionBox& b) {
        int x1 = std::max(a.x, b.x);
        int y1 = std::max(a.y, b.y);
        int x2 = std::min(a.x + a.width, b.x + b.width);
        int y2 = std::min(a.y + a.height, b.y + b.height);
        if (x2 <= x1 || y2 <= y1) return 0;
        float inter = static_cast<float>((x2 - x1) * (y2 - y1));
        float area_a = static_cast<float>(a.width * a.height);
        float area_b = static_cast<float>(b.width * b.height);
        return inter / (area_a + area_b - inter);
    }

    ModelInfo model_info_;
    std::vector<TrainingSample> samples_;
    std::wstring model_loaded_path_;
    std::unique_ptr<SimpleKNN> knn_;
    std::unique_ptr<SimpleSVM> svm_;
};

// ── Model manager: version tracking + deployment ─────────────────────────

class ModelManager {
public:
    // Add a version record
    void AddVersion(const ModelVersionRecord& record) {
        versions_.push_back(record);
        if (!versions_.empty()) {
            next_id_ = versions_.back().version_id + 1;
        }
    }

    int CreateVersion(const std::wstring& file_path, int samples, float train_acc, float val_acc,
                      const std::wstring& notes = L"") {
        ModelVersionRecord rec;
        rec.version_id = next_id_++;
        rec.file_path = file_path;
        rec.training_samples = samples;
        rec.train_accuracy = train_acc;
        rec.val_accuracy = val_acc;
        rec.notes = notes;

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &tt);
        wchar_t buf[64];
        std::wcsftime(buf, 64, L"%Y-%m-%d %H:%M", &tm_buf);
        rec.created_at = buf;

        versions_.push_back(rec);

        // Persist version history
        SaveVersionHistory();

        return rec.version_id;
    }

    // Mark a version as deployed (only one at a time)
    void Deploy(int version_id) {
        for (auto& v : versions_) v.deployed = false;
        for (auto& v : versions_) {
            if (v.version_id == version_id) {
                v.deployed = true;
                deployed_version_id_ = version_id;
                break;
            }
        }
        SaveVersionHistory();
    }

    int GetDeployedVersionId() const { return deployed_version_id_; }
    const std::vector<ModelVersionRecord>& GetVersions() const { return versions_; }

    const ModelVersionRecord* GetVersion(int id) const {
        for (const auto& v : versions_) {
            if (v.version_id == id) return &v;
        }
        return nullptr;
    }

    // Persistence
    bool SaveVersionHistory(const std::wstring& path = L"") {
        std::wstring save_path = path.empty() ? GetDefaultHistoryPath() : path;
        std::ofstream ofs(std::string(save_path.begin(), save_path.end()));
        if (!ofs) return false;

        ofs << "version_id,deployed,train_accuracy,val_accuracy,training_samples,file_path,notes,created_at\n";
        for (const auto& v : versions_) {
            ofs << v.version_id << ","
                << (v.deployed ? 1 : 0) << ","
                << v.train_accuracy << ","
                << v.val_accuracy << ","
                << v.training_samples << ",\""
                << WideToUtf8(v.file_path) << "\",\""
                << WideToUtf8(v.notes) << "\",\""
                << WideToUtf8(v.created_at) << "\"\n";
        }
        return true;
    }

    bool LoadVersionHistory(const std::wstring& path = L"") {
        std::wstring load_path = path.empty() ? GetDefaultHistoryPath() : path;
        std::ifstream ifs(std::string(load_path.begin(), load_path.end()));
        if (!ifs) return false;

        versions_.clear();
        std::string line;
        std::getline(ifs, line); // skip header

        while (std::getline(ifs, line)) {
            ModelVersionRecord rec;
            std::istringstream iss(line);
            std::string token;

            auto get_quoted = [&]() -> std::string {
                std::string result;
                std::getline(iss, result, '\"');
                std::getline(iss, result, '\"');
                // consume trailing comma
                std::getline(iss, token, ',');
                return result;
            };

            std::getline(iss, token, ','); rec.version_id = std::stoi(token);
            std::getline(iss, token, ','); rec.deployed = (std::stoi(token) == 1);
            std::getline(iss, token, ','); rec.train_accuracy = std::stof(token);
            std::getline(iss, token, ','); rec.val_accuracy = std::stof(token);
            std::getline(iss, token, ','); rec.training_samples = std::stoi(token);
            rec.file_path = Utf8ToWide(get_quoted());
            rec.notes = Utf8ToWide(get_quoted());
            rec.created_at = Utf8ToWide(get_quoted());

            if (rec.deployed) deployed_version_id_ = rec.version_id;
            versions_.push_back(rec);
            if (rec.version_id >= next_id_) next_id_ = rec.version_id + 1;
        }
        return true;
    }

private:
    static std::wstring GetDefaultHistoryPath() {
        return L"model_versions.csv";
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                                       nullptr, 0, nullptr, nullptr);
        std::string result(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                             result.data(), len, nullptr, nullptr);
        return result;
    }

    static std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                       nullptr, 0);
        std::wstring result(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                             result.data(), len);
        return result;
    }

    std::vector<ModelVersionRecord> versions_;
    int next_id_ = 1;
    int deployed_version_id_ = -1;
};
