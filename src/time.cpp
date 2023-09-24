#include "flow/time.h"
#include <chrono>

namespace flow {

int64_t TimePoint::now_timestamp() {
    return std::chrono::system_clock::now().time_since_epoch().count();
}

TimePoint TimePoint::now(int64_t initial_timestamp) {
    TimePoint result;
    result.timestamp = now_timestamp();
    result.time = 1e-9 * static_cast<double>(result.timestamp - initial_timestamp);
    result.rate = 1;
    return result;
};

Duration operator-(const TimePoint& lhs, const TimePoint& rhs) {
    Duration duration;
    duration.elapsed = lhs.time - rhs.time;
    duration.elapsed_timestamp = lhs.timestamp - rhs.timestamp;
    return duration;
}

} // namespace flow
