#pragma once


#include <vector>
#include <thread>
#include <iostream>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <assert.h>
#include <shared_mutex>
#include "flow/time.h"


namespace flow {

class Engine {
    typedef std::function<void()> callback_t;
    typedef std::function<bool()> bool_callback_t;
    typedef std::function<void(TimePoint time)> timer_callback_t;
    typedef std::function<TimePoint()> time_source_t;
public:
    Engine();

    void push_callback(const callback_t& callback);

    void create_poll_callback(const bool_callback_t& poll);
    void create_poll_shutdown_callback(const bool_callback_t& poll, const callback_t& shutdown);
    void create_init_poll_callback(const bool_callback_t& init, const bool_callback_t& poll);
    void create_init_poll_shutdown_callback(const bool_callback_t& init, const bool_callback_t& poll, const callback_t& shutdown);
    void create_init_callback(const bool_callback_t& init);
    void create_shutdown_callback(const callback_t& shutdown);

    void create_timer_callback(double period, const timer_callback_t& callback);
    TimePoint get_time() const;
    void set_time_source(const time_source_t& time_source);

    void run(size_t num_callback_threads = 4);
    void stop();

private:
    void execute_callback();

    time_source_t time_source;

    std::atomic<bool> start_init;
    std::atomic<int> init_count;
    std::atomic<bool> init_valid;

    std::atomic<bool> running;
    TimePoint time;
    mutable std::shared_mutex time_mutex;

    struct TimerCallback {
        double period;
        double next_time;
        timer_callback_t callback;
    };
    std::vector<TimerCallback> timer_callbacks;

    std::queue<callback_t> callback_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;

    std::vector<std::jthread> threads;
};

} // namespace flow
