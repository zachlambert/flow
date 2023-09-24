#pragma once

#include <cstdint>


namespace flow {

// If using real time:
// - Time = Elapsed time since start of program
// - Timestamp = Unix timestamp (nanoseconds since epoch)
// - Rate = 1
// If using sim or data playback:
// - Time = Elapsed system time since start of sim or playback (not same as real time)
// - Timestamp = Nanoseconds since start of sim or playback
// - Rate = Ratio between real time and system time (eg: 2 if simulation is running 2x faster)

struct TimePoint {
    double time;
    int64_t timestamp;
    double rate;

    static int64_t now_timestamp();
    static TimePoint now(int64_t initial_timestamp);
};

struct Duration {
    double elapsed;
    int64_t elapsed_timestamp;
};

Duration operator-(const TimePoint& lhs, const TimePoint& rhs);

} // namespace flow
