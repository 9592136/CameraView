#pragma once

class ProcessingProgressThrottle {
public:
    explicit ProcessingProgressThrottle(int step_percent = 10);

    bool ShouldReport(int percent);

private:
    int step_percent_ = 10;
    int last_reported_percent_ = -10;
};
