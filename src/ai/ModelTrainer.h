#pragma once

#include "ModelDef.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <numeric>
#include <random>
#include <unordered_map>
#include <cmath>

// ── Feature extraction from raw image data ───────────────────────────────
//
// Extracts fixed-size RGB feature vector (target_w × target_h × 3 floats in [0,1]).
// Uses bilinear interpolation for smooth downscaling (anti-aliasing).
//
// The input rgb_data is in RGB byte order, 3 bytes per pixel, with line stride.

inline std::vector<float> ExtractRGBFeatures(const uint8_t* rgb_data,
                                              int src_w, int src_h,
                                              int src_stride,
                                              int target_w, int target_h)
{
    std::vector<float> features(static_cast<size_t>(target_w) * target_h * 3, 0.0f);
    float scale_x = static_cast<float>(src_w) / target_w;
    float scale_y = static_cast<float>(src_h) / target_h;

    for (int ty = 0; ty < target_h; ++ty) {
        // Bilinear: source sub-pixel center
        float sy_raw = (static_cast<float>(ty) + 0.5f) * scale_y - 0.5f;
        int sy0 = std::max(0, static_cast<int>(std::floor(sy_raw)));
        int sy1 = std::min(src_h - 1, sy0 + 1);
        float wy = sy_raw - static_cast<float>(sy0);

        for (int tx = 0; tx < target_w; ++tx) {
            float sx_raw = (static_cast<float>(tx) + 0.5f) * scale_x - 0.5f;
            int sx0 = std::max(0, static_cast<int>(std::floor(sx_raw)));
            int sx1 = std::min(src_w - 1, sx0 + 1);
            float wx = sx_raw - static_cast<float>(sx0);

            auto sample = [&](int sx, int sy) -> std::array<float, 3> {
                const uint8_t* px = rgb_data +
                    static_cast<size_t>(sy) * src_stride +
                    static_cast<size_t>(sx) * 3;
                // Input is BGR byte order → convert to RGB float
                return {px[2] / 255.0f, px[1] / 255.0f, px[0] / 255.0f};
            };

            auto p00 = sample(sx0, sy0);
            auto p10 = sample(sx1, sy0);
            auto p01 = sample(sx0, sy1);
            auto p11 = sample(sx1, sy1);

            int idx = (ty * target_w + tx) * 3;
            // Note: rgb_data is stored as RGB (R=idx0, G=idx1, B=idx2)
            for (int c = 0; c < 3; ++c) {
                float top    = p00[c] * (1.0f - wx) + p10[c] * wx;
                float bottom = p01[c] * (1.0f - wx) + p11[c] * wx;
                features[idx + c] = top * (1.0f - wy) + bottom * wy;
            }
        }
    }
    return features;
}

// ── Training sample ──────────────────────────────────────────────────────

// TrainingSample is defined in ModelDef.h

// ── k-NN classifier ──────────────────────────────────────────────────────

class SimpleKNN {
public:
    void Train(const std::vector<TrainingSample>& samples) {
        samples_ = samples;
    }

    ClassificationResult Predict(const FeatureVector& features, int k = 5) const {
        if (samples_.empty()) return {};

        struct Neighbor {
            int label;
            float dist;
        };
        std::vector<Neighbor> neighbors;
        neighbors.reserve(samples_.size());

        for (const auto& s : samples_) {
            neighbors.push_back({s.label_id, features.EuclideanDistance(s.features)});
        }
        std::sort(neighbors.begin(), neighbors.end(),
            [](const Neighbor& a, const Neighbor& b) { return a.dist < b.dist; });

        int effective_k = std::min(k, static_cast<int>(neighbors.size()));
        std::unordered_map<int, int> votes;
        for (int i = 0; i < effective_k; ++i) {
            ++votes[neighbors[i].label];
        }

        int best_label = -1, best_count = 0;
        for (const auto& p : votes) {
            if (p.second > best_count) {
                best_count = p.second;
                best_label = p.first;
            }
        }

        ClassificationResult result;
        result.label_id = best_label;
        result.confidence = static_cast<float>(best_count) / effective_k;
        return result;
    }

    size_t SampleCount() const { return samples_.size(); }

private:
    std::vector<TrainingSample> samples_;
};

// ── Simple linear SVM (one-vs-rest) ──────────────────────────────────────

class SimpleSVM {
public:
    struct SVMWeight {
        int label_id = 0;
        std::vector<float> w;
        float b = 0.0f;
    };

    void Train(const std::vector<TrainingSample>& samples, int num_labels, int max_iter = 500) {
        if (samples.empty() || num_labels < 2) return;
        int dim = static_cast<int>(samples[0].features.values.size());

        weights_.clear();
        for (int label = 0; label < num_labels; ++label) {
            // One-vs-rest binary labels
            std::vector<int> binary;
            for (const auto& s : samples) {
                binary.push_back(s.label_id == label ? 1 : -1);
            }

            SVMWeight sw;
            sw.label_id = label;
            sw.w.assign(dim, 0.0f);
            sw.b = 0.0f;

            // Simple SGD training
            float lr = 0.01f;
            float lambda = 0.001f;
            std::mt19937 rng(42);
            for (int iter = 0; iter < max_iter; ++iter) {
                for (size_t i = 0; i < samples.size(); ++i) {
                    float score = sw.b;
                    const auto& fv = samples[i].features.values;
                    for (int d = 0; d < dim; ++d) score += sw.w[d] * fv[d];

                    if (binary[i] * score < 1.0f) {
                        for (int d = 0; d < dim; ++d) {
                            sw.w[d] = sw.w[d] * (1 - lr * lambda) + lr * binary[i] * fv[d];
                        }
                        sw.b += lr * binary[i];
                    }
                }
                lr *= 0.99f;
            }
            weights_.push_back(sw);
        }
        num_labels_ = num_labels;
    }

    ClassificationResult Predict(const FeatureVector& features) const {
        if (weights_.empty()) return {};

        float best_score = -std::numeric_limits<float>::max();
        int best_label = -1;

        for (const auto& sw : weights_) {
            float score = sw.b;
            const auto& fv = features.values;
            int dim = std::min(static_cast<int>(fv.size()), static_cast<int>(sw.w.size()));
            for (int d = 0; d < dim; ++d) score += sw.w[d] * fv[d];
            if (score > best_score) {
                best_score = score;
                best_label = sw.label_id;
            }
        }

        ClassificationResult result;
        result.label_id = best_label;
        result.confidence = std::min(1.0f, std::max(0.0f, (best_score + 1.0f) / 2.0f));
        return result;
    }

private:
    std::vector<SVMWeight> weights_;
    int num_labels_ = 0;
};

// ── Simple k-Means clustering for segmentation ───────────────────────────

class SimpleKMeans {
public:
    void Cluster(const std::vector<FeatureVector>& pixels, int k, int max_iter = 20) {
        if (pixels.empty() || k < 2) return;
        int dim = static_cast<int>(pixels[0].values.size());
        size_t n = pixels.size();

        // K-Means++ initialization: pick centroids with distance-proportional probability
        centroids_.resize(k);
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> first_dist(0, n - 1);
        centroids_[0] = pixels[first_dist(rng)].values;

        for (int c = 1; c < k; ++c) {
            std::vector<float> min_dist_sq(n, std::numeric_limits<float>::max());
            float total_dist = 0.0f;
            for (size_t i = 0; i < n; ++i) {
                for (int pc = 0; pc < c; ++pc) {
                    float d = 0;
                    for (int j = 0; j < dim; ++j) {
                        float diff = pixels[i].values[j] - centroids_[pc][j];
                        d += diff * diff;
                    }
                    if (d < min_dist_sq[i]) min_dist_sq[i] = d;
                }
                total_dist += min_dist_sq[i];
            }

            if (total_dist <= 0.0f) {
                // Fallback: all remaining pixels identical, pick random
                centroids_[c] = pixels[first_dist(rng)].values;
            } else {
                std::uniform_real_distribution<float> prob_dist(0.0f, total_dist);
                float r = prob_dist(rng);
                float cumulative = 0.0f;
                size_t selected = n - 1;
                for (size_t i = 0; i < n; ++i) {
                    cumulative += min_dist_sq[i];
                    if (cumulative >= r) { selected = i; break; }
                }
                centroids_[c] = pixels[selected].values;
            }
        }

        assignments_.resize(n);

        for (int iter = 0; iter < max_iter; ++iter) {
            // Assign pixels to nearest centroid
            bool changed = false;
            for (size_t i = 0; i < n; ++i) {
                float best_dist = std::numeric_limits<float>::max();
                int best_c = 0;
                for (int c = 0; c < k; ++c) {
                    float d = 0;
                    for (int j = 0; j < dim; ++j) {
                        float diff = pixels[i].values[j] - centroids_[c][j];
                        d += diff * diff;
                    }
                    if (d < best_dist) {
                        best_dist = d;
                        best_c = c;
                    }
                }
                if (assignments_[i] != best_c) changed = true;
                assignments_[i] = best_c;
            }
            if (!changed) break;

            // Update centroids; reinitialize any empty clusters
            std::vector<std::vector<float>> sums(k, std::vector<float>(dim, 0.0f));
            std::vector<int> counts(k, 0);
            for (size_t i = 0; i < n; ++i) {
                int c = assignments_[i];
                ++counts[c];
                for (int j = 0; j < dim; ++j) sums[c][j] += pixels[i].values[j];
            }
            for (int c = 0; c < k; ++c) {
                if (counts[c] > 0) {
                    float inv = 1.0f / static_cast<float>(counts[c]);
                    for (int j = 0; j < dim; ++j)
                        centroids_[c][j] = sums[c][j] * inv;
                } else {
                    // Reinitialize empty cluster to a random pixel (prevents NaN)
                    size_t ri = first_dist(rng);
                    centroids_[c] = pixels[ri].values;
                }
            }
        }
    }

    const std::vector<int>& Assignments() const { return assignments_; }
    int NumClusters() const { return static_cast<int>(centroids_.size()); }

private:
    std::vector<std::vector<float>> centroids_;
    std::vector<int> assignments_;
};

// ── Model persistence (simple CSV format) ────────────────────────────────

inline bool SaveModel(const std::wstring& path, const ModelInfo& info,
                      const std::vector<TrainingSample>& samples)
{
    std::ofstream f(std::string(path.begin(), path.end()), std::ios::binary);
    if (!f) return false;

    // Header
    f << "# CameraView AI Model v1\n";
    f << "name:" << std::string(info.name.begin(), info.name.end()) << "\n";
    f << "task_type:" << static_cast<int>(info.task_type) << "\n";
    f << "backend:" << static_cast<int>(info.backend) << "\n";
    f << "input_w:" << info.input_width << "\n";
    f << "input_h:" << info.input_height << "\n";
    f << "feature_dim:" << info.feature_dim << "\n";
    f << "num_labels:" << info.labels.size() << "\n";
    f << "num_samples:" << samples.size() << "\n";

    // Labels
    for (const auto& label : info.labels) {
        f << "label:" << label.id << "|"
          << std::string(label.name.begin(), label.name.end()) << "|"
          << std::hex << label.color << std::dec << "\n";
    }

    // Samples
    for (const auto& s : samples) {
        f << "sample:" << s.label_id;
        for (float v : s.features.values) f << "," << v;
        f << "\n";
    }

    return f.good();
}

inline bool LoadModel(const std::wstring& path, ModelInfo& info,
                      std::vector<TrainingSample>& samples)
{
    std::ifstream f(std::string(path.begin(), path.end()), std::ios::binary);
    if (!f) return false;

    info = {};
    samples.clear();

    std::string line;
    std::getline(f, line); // version header
    if (line.find("# CameraView AI Model") != 0) return false;

    while (std::getline(f, line)) {
        if (line.empty()) continue;

        if (line.rfind("name:", 0) == 0) {
            info.name = std::wstring(line.begin() + 5, line.end());
        } else if (line.rfind("task_type:", 0) == 0) {
            info.task_type = static_cast<AiTaskType>(std::stoi(line.substr(10)));
        } else if (line.rfind("backend:", 0) == 0) {
            info.backend = static_cast<AiBackend>(std::stoi(line.substr(8)));
        } else if (line.rfind("input_w:", 0) == 0) {
            info.input_width = std::stoi(line.substr(8));
        } else if (line.rfind("input_h:", 0) == 0) {
            info.input_height = std::stoi(line.substr(8));
        } else if (line.rfind("feature_dim:", 0) == 0) {
            info.feature_dim = std::stoi(line.substr(12));
        } else if (line.rfind("num_labels:", 0) == 0) {
            // Just read, labels follow
        } else if (line.rfind("num_samples:", 0) == 0) {
            // Just read, samples follow
        } else if (line.rfind("label:", 0) == 0) {
            AiLabel label;
            auto content = line.substr(6);
            auto p1 = content.find('|');
            auto p2 = content.find('|', p1 + 1);
            if (p1 != std::string::npos) {
                label.id = std::stoi(content.substr(0, p1));
                label.name = std::wstring(content.begin() + p1 + 1,
                    p2 != std::string::npos ? content.begin() + p2 : content.end());
                if (p2 != std::string::npos) {
                    try {
                        label.color = static_cast<uint32_t>(std::stoul(content.substr(p2 + 1), nullptr, 16));
                    } catch (...) {}
                }
            }
            info.labels.push_back(label);
        } else if (line.rfind("sample:", 0) == 0) {
            TrainingSample s;
            auto content = line.substr(7);
            auto comma = content.find(',');
            if (comma != std::string::npos) {
                s.label_id = std::stoi(content.substr(0, comma));
                std::string vals = content.substr(comma + 1);
                std::stringstream ss(vals);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    if (!token.empty()) s.features.values.push_back(std::stof(token));
                }
            }
            samples.push_back(s);
        }
    }

    info.training_samples = static_cast<int>(samples.size());
    return f.good() || f.eof();
}
