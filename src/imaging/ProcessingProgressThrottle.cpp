#include "ProcessingProgressThrottle.h"

ProcessingProgressThrottle::ProcessingProgressThrottle(int step_percent)
    : step_percent_(step_percent > 0 ? step_percent : 1),
      last_reported_percent_(-(step_percent > 0 ? step_percent : 1))
{
}

bool ProcessingProgressThrottle::ShouldReport(int percent)
{
    if (percent == 100 || percent >= last_reported_percent_ + step_percent_) {
        last_reported_percent_ = percent;
        return true;
    }
    return false;
}
