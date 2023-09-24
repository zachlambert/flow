#include "flow/engine.h"


namespace flow {

Engine::Engine():
    start_init(false),
    init_count(0),
    init_valid(true),
    running(false),
    time_source(nullptr)
{}

void Engine::push_callback(const callback_t& callback) {
    {
        std::scoped_lock<std::mutex> lock(queue_mutex);
        callback_queue.push(callback);
    }
    cv.notify_one();
}

void Engine::create_poll_callback(const bool_callback_t& poll) {
    threads.emplace_back([poll, this](){
        while (!running && init_valid) {}
        if (!init_valid) return;
        while (running && poll()) {}
    });
}

void Engine::create_init_poll_callback(const bool_callback_t& init, const bool_callback_t& poll) {
    threads.emplace_back([init, poll, this](){
        init_count++;
        while (!start_init) {}
        if (!init()) {
            init_valid = false;
            init_count--;
            return;
        }
        init_count--;
        while (!running && init_valid) {}
        if (!init_valid) return;
        while (running && poll()) {}
    });
}

void Engine::create_poll_shutdown_callback(const bool_callback_t& poll, const callback_t& shutdown) {
    threads.emplace_back([poll, shutdown, this](){
        while (!running && init_valid) {}
        if (init_valid) {
            while (running && poll()) {}
        }
        shutdown();
    });
}

void Engine::create_init_poll_shutdown_callback(
    const bool_callback_t& init,
    const bool_callback_t& poll,
    const callback_t& shutdown)
{
    threads.emplace_back([init, poll, shutdown, this](){
        init_count++;
        while (!start_init) {}
        if (!init()) {
            init_valid = false;
            init_count--;
            return;
        }
        init_count--;
        while (!running && init_valid) {}
        if (init_valid) {
            while (running && poll()) {}
        }
        shutdown();
    });
}

void Engine::create_init_callback(const bool_callback_t& init)
{
    threads.emplace_back([init, this](){
        init_count++;
        while (!start_init) {}
        if (!init()) {
            init_valid = false;
            init_count--;
            return;
        }
        init_count--;
    });
}

void Engine::create_shutdown_callback(const callback_t& shutdown)
{
    // TODO: Don't need thread for this
    threads.emplace_back([shutdown, this](){
        while (!running && init_valid) {}
        if (init_valid) {
            while (running) {}
        }
        shutdown();
    });
}

void Engine::create_timer_callback(double period, const timer_callback_t& callback) {
    TimerCallback timer_callback;
    timer_callback.period = period;
    timer_callback.next_time = 0;
    timer_callback.callback = callback;
    timer_callbacks.push_back(timer_callback);
}

TimePoint Engine::get_time() const {
    TimePoint temp;
    {
        std::shared_lock<std::shared_mutex> lock(time_mutex);
        temp = time;
    }
    return temp;
}

void Engine::set_time_source(const time_source_t& time_source) {
    this->time_source = time_source;
}

void Engine::run(size_t num_callback_threads) {
    if (num_callback_threads == 0) num_callback_threads = 1;

    // Timing thread
    threads.emplace_back([this](){
        while (!running && init_valid) {}
        int64_t initial_timestamp = TimePoint::now_timestamp();

        while (running && init_valid) {
            TimePoint new_time;
            if (time_source) {
                new_time = time_source();
            } else {
                new_time = TimePoint::now(initial_timestamp);
            }

            {
                std::scoped_lock<std::shared_mutex> lock(time_mutex);
                time = new_time;
            }

            for (auto& callback: timer_callbacks) {
                if (callback.next_time < new_time.time) {
                    // Copy the current time instead of using reference
                    // when constructing lambda for callback.
                    push_callback(std::bind([new_time, &callback]() {
                        callback.callback(new_time);
                    }));
                    callback.next_time += callback.period;
                }
            }
        }
    });

    for (size_t i = 0; i < num_callback_threads; i++) {
        threads.emplace_back([this](){
            while (!running && init_valid) {}
            while (running) {
                execute_callback();
            }
        });
    }

    start_init = true;
    while (init_count > 0) {}
    running = true;

    for (auto& thread: threads) {
        thread.join();
    }
}

void Engine::stop() {
    running = false;
    cv.notify_all();
}

void Engine::execute_callback() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    cv.wait(lock, [&]{ return callback_queue.size() > 0 || !running; });
    if (!running) return;

    assert(!callback_queue.empty());
    callback_t callback = callback_queue.front();
    callback_queue.pop();
    lock.unlock();
    callback();
}

} // namespace flow
