#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

struct FluorescenceCaptureSequenceState {
    int current_index = -1;
    std::size_t preset_count = 0;
    std::uint64_t baseline_sequence = 0;
    double expected_exposure_ms = 0.0;
    bool active = false;
    bool exposure_ready = false;
};

enum class FluorescenceCaptureAdvance {
    Rejected,
    NextPreset,
    Complete
};

class FluorescenceCaptureSequence final {
public:
    static bool Start(FluorescenceCaptureSequenceState& state, std::size_t preset_count)
    {
        state = {};
        if (preset_count == 0) return false;
        state.active = true;
        state.current_index = 0;
        state.preset_count = preset_count;
        return true;
    }

    static bool RequestExposure(
        FluorescenceCaptureSequenceState& state,
        double exposure_ms,
        std::uint64_t latest_sequence)
    {
        if (!IsActive(state) || exposure_ms <= 0.0) return false;
        state.expected_exposure_ms = exposure_ms;
        state.baseline_sequence = latest_sequence;
        state.exposure_ready = false;
        return true;
    }

    static bool ConfirmExposure(
        FluorescenceCaptureSequenceState& state,
        double exposure_ms,
        bool success,
        std::uint64_t latest_sequence)
    {
        if (!IsActive(state) ||
            std::abs(exposure_ms - state.expected_exposure_ms) > 0.000001) {
            return false;
        }
        state.exposure_ready = success;
        state.baseline_sequence = latest_sequence;
        return true;
    }

    static bool CanCapture(
        const FluorescenceCaptureSequenceState& state,
        std::uint64_t latest_sequence)
    {
        return IsActive(state) && state.exposure_ready &&
            latest_sequence > state.baseline_sequence;
    }

    static FluorescenceCaptureAdvance Capture(
        FluorescenceCaptureSequenceState& state,
        std::uint64_t latest_sequence)
    {
        if (!CanCapture(state, latest_sequence)) {
            return FluorescenceCaptureAdvance::Rejected;
        }
        ++state.current_index;
        state.exposure_ready = false;
        if (state.current_index >= static_cast<int>(state.preset_count)) {
            state.active = false;
            state.current_index = -1;
            return FluorescenceCaptureAdvance::Complete;
        }
        return FluorescenceCaptureAdvance::NextPreset;
    }

    static void Cancel(FluorescenceCaptureSequenceState& state)
    {
        state.active = false;
        state.exposure_ready = false;
        state.current_index = -1;
    }

    static bool IsActive(const FluorescenceCaptureSequenceState& state)
    {
        return state.active && state.current_index >= 0 &&
            state.current_index < static_cast<int>(state.preset_count);
    }
};
