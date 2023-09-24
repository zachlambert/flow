#include <flow/engine.h>
#include <flow/signal.h>
#include <string>
#include <iostream>
#include <sstream>


// MessageViewer: Prints input message to stdout
// in_message (string): The message to display
class MessageViewer {
public:
    MessageViewer(flow::Engine& engine):
        in_message_(engine, std::bind(&MessageViewer::callback_message, this, std::placeholders::_1))
    {}

    flow::Input<std::string>& in_message() { return in_message_; }

private:
    void callback_message(const std::string& message)
    {
        std::cout << message << std::endl;
    }

    flow::DirectInput<std::string> in_message_;
};


// MessageGenerator: Creates a message representing the current inputs
// in_a (int): One input integer
// in_b (int): Another input integer
// out_message (string): Output message
class MessageGenerator {
public:
    MessageGenerator(flow::Engine& engine, double period):
        in_a_(engine),
        in_b_(engine)
    {
        engine.create_timer_callback(
            period,
            std::bind(&MessageGenerator::timer_callback, this, std::placeholders::_1)
        );
    }

    flow::Input<int>& in_a() { return in_a_; };
    flow::Input<int>& in_b() { return in_b_; };
    flow::Output<std::string>& out_message() { return out_message_; };

private:
    void timer_callback(const flow::TimePoint& time)
    {
        auto a = in_a_.get();
        if (!a) return;
        auto b = in_b_.get();
        if (!b) return;

        std::stringstream ss;
        ss << "a: " << *a << ", b: " << *b << ", sum: " << *a + *b;
        out_message_.write(ss.str());
    }

    flow::SampledInput<int> in_a_;
    flow::SampledInput<int> in_b_;
    flow::DirectOutput<std::string> out_message_;
};


// SequenceGenerator: Generates a sequence of integers
// out_value (int): Output integer
class SequenceGenerator {
public:
    SequenceGenerator(flow::Engine& engine, double period, int initial, int step):
        value(initial),
        step(step)
    {
        engine.create_timer_callback(
            period,
            std::bind(&SequenceGenerator::timer_callback, this, std::placeholders::_1)
        );
    }

    flow::Output<int>& out_value() { return out_value_; };

private:
    void timer_callback(const flow::TimePoint& time)
    {
        out_value_.write(value);
        value += step;
    }

    int value;
    const int step;
    flow::DirectOutput<int> out_value_;
};

class Timeout {
public:
    Timeout(flow::Engine& engine, double timeout):
        engine(engine),
        timeout(timeout)
    {
        engine.create_init_poll_callback(
            std::bind(&Timeout::init, this),
            std::bind(&Timeout::poll, this));
    }

private:
    bool init()
    {
        initial_time = engine.get_time();
        return true;
    }

    bool poll()
    {
        flow::TimePoint time = engine.get_time();
        flow::Duration duration = time - initial_time;
        if (duration.elapsed >= timeout) {
            engine.stop();
        }

        return true;
    }

    flow::Engine& engine;
    const double timeout;
    flow::TimePoint initial_time;
};


int main()
{
    flow::Engine engine;

    SequenceGenerator a_generator(engine, 1.0 / 20, 0, 1);
    SequenceGenerator b_generator(engine, 1.0 / 4, 0, -5);
    MessageGenerator message_generator(engine, 1.0 / 5);
    MessageViewer message_viewer(engine);
    Timeout timeout(engine, 5.0);

    flow::connect(a_generator.out_value(), message_generator.in_a());
    flow::connect(b_generator.out_value(), message_generator.in_b());
    flow::connect(message_generator.out_message(), message_viewer.in_message());

    engine.run();

    return 0;
}
