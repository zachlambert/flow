#pragma once

#include <shared_mutex>
#include <atomic>
#include <functional>
#include <assert.h>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <optional>
#include "flow/engine.h"

namespace flow {

template <typename T>
class Output;

class InputBase {
public:
    virtual ~InputBase() {}
};

template <typename T>
class Input: public InputBase {
private:
    virtual void write(const T& value) = 0;

    template <typename T_>
    friend class Output;
};

template <typename T>
class Output {
public:
    virtual void write(const T& value) = 0;
protected:
    void write_value(const T& value)
    {
        std::scoped_lock<std::mutex> lock(inputs_mutex);
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->write(value);
        }
    }

private:
    void add_input(Input<T>& input)
    {
        std::scoped_lock<std::mutex> lock(inputs_mutex);
        inputs.push_back(&input);
    }

    std::vector<Input<T>*> inputs;
    std::mutex inputs_mutex;

    template <typename T_>
    friend void connect(Output<T_>& out, Input<T_>& in);
};

template <typename T>
class DirectOutput: public Output<T> {
public:
    void write(const T& value) override
    {
        this->write_value(value);
    }
};

template <typename T>
class TimedOutput: public Output<T> {
public:
    TimedOutput(Engine& engine, double period) {
        engine.create_timer_callback(period, std::bind(&TimedOutput::write_callback, this));
    }
    void write(const T& value) override {
        std::scoped_lock<std::mutex> lock(mutex);
        this->value = value;
    }
private:
    void write_callback() {
        std::scoped_lock<std::mutex> lock(mutex);
        if (!value.has_value()) return;
        this->write_value(value.value());
    }
    std::optional<T> value;
    std::mutex mutex;
};

template <typename T>
class SampledInput: public Input<T> {
public:
    typedef std::function<void(const T&)> callback_t;
    SampledInput(Engine& engine, const std::optional<callback_t>& callback = std::nullopt):
        engine(engine),
        callback(callback),
        read_cached(true),
        value_received(false)
    {}
    SampledInput(Engine& engine, const T& default_value, const std::optional<callback_t>& callback = std::nullopt):
        engine(engine),
        callback(callback),
        read_cached(true),
        value_received(true)
    {
        cached = default_value;
    }

    class Pointer {
    public:
        const T& operator*()const
        {
            return reading_cached ? parent->cached : parent->other;
        }
        const T* operator->()const
        {
            return reading_cached ? &parent->cached : &parent->other;
        }
        operator bool()const { return parent != nullptr; }

        Pointer(Pointer&& other) = default;
        ~Pointer()
        {
            if (!parent) return;
            std::scoped_lock<std::mutex> lock(parent->mutex);
            if (reading_cached) {
                parent->reading_cached = false;
            } else {
                parent->reading_other = false;
            }
        }
    private:
        Pointer(const SampledInput* parent = nullptr, bool reading_cached = true):
            parent(parent),
            reading_cached(reading_cached)
        {}
        const SampledInput* parent;
        bool reading_cached;
        friend class SampledInput;
    };

    Pointer get()const
    {
        std::scoped_lock<std::mutex> lock(mutex);
        if (!value_received) return Pointer();

        if (read_cached) {
            reading_cached = true;
            return Pointer(this, true);
        } else {
            reading_other = true;
            return Pointer(this, false);
        }
    }

private:
    void write(const T& data) override
    {
        bool write_cached;
        mutex.lock();
        if (!reading_cached && !reading_other) {
            write_cached = !read_cached;
        } else if (!reading_cached) {
            write_cached = true;
        } else { // !reading_other
            write_cached = false;
        }
        mutex.unlock();

        if (write_cached) {
            cached = data;
        } else {
            other = data;
        }

        mutex.lock();
        read_cached = write_cached;
        value_received = true;
        if (callback.has_value()) {
            if (read_cached) {
                callback.value()(cached);
            } else {
                callback.value()(other);
            }
        }
        mutex.unlock();
    }

    // TODO: Remove engine, unused
    Engine& engine;
    std::optional<callback_t> callback;

    T cached;
    T other;
    mutable std::mutex mutex;
    mutable std::mutex other_mutex;
    mutable bool read_cached;
    mutable bool reading_cached;
    mutable bool reading_other;
    bool value_received;
};

template <typename T>
class CallbackInput: public Input<T> {
public:
    typedef std::function<void(const T&)> callback_t;
    CallbackInput(
        Engine& engine,
        const callback_t& callback,
        size_t queue_size = 10
    ):
        engine(engine),
        callback(callback),
        front(0),
        back(0),
        full(false)
    {
        queue.resize(queue_size);
    }

    class Pointer {
    public:
        const T& operator*()const
        {
            return parent->queue[parent->front];
        }
        const T* operator->()const
        {
            return &parent->queue[parent->front];
        }
        operator bool()const
        {
            return parent != nullptr;
        }
        ~Pointer()
        {
            if (!parent) return;

            std::scoped_lock<std::mutex> lock(parent->position_mutex);
            parent->mutex.unlock();

            parent->full = false;
            parent->front = (parent->front + 1) % parent->queue.size();
        }
        Pointer(Pointer&& other) = default;
    private:
        Pointer(const CallbackInput* parent = nullptr):
            parent(parent)
        {}
        const CallbackInput* parent;
        friend class CallbackInput;
    };

    Pointer get()const
    {
        mutex.lock();
        std::scoped_lock<std::mutex> lock(position_mutex);
        if (!full && front == back) {
            mutex.unlock();
            return Pointer();
        }
        return Pointer(this);
    }

    static Pointer null_pointer()
    {
        return Pointer();
    }

private:
    void write(const T& data) override
    {
        std::scoped_lock<std::mutex> lock(position_mutex);
        while (full);
        queue[back] = data;
        back = (back + 1) % queue.size();
        full = (back == front);
        engine.push_callback(std::bind(&CallbackInput::process, this));
    }

    void process()
    {
        auto data = get();
        callback(*data);
    }

    Engine& engine;
    callback_t callback;

    std::vector<T> queue;
    mutable size_t front, back;
    mutable std::mutex position_mutex;
    mutable std::atomic<bool> full;
    mutable std::mutex mutex;
};

template <typename T>
class DirectInput: public Input<T> {
public:
    typedef std::function<void(const T&)> callback_t;
    DirectInput(Engine& engine, const callback_t& callback):
        callback(callback)
    {}

private:
    void write(const T& data) override
    {
        callback(data);
    }

    callback_t callback;
};

template <typename T>
void connect(Output<T>& out, Input<T>& in)
{
    out.add_input(in);
}

} // namespace flow
