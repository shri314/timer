/**
 * main test code
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#include "shri314/timer/Timer.hpp"
#include "shri314/utils/ScopedExit.hpp"

#include "utils/DataChannel.hpp"
#include "utils/LogUtils.hpp"

#include <thread>
#include <iostream>

struct TestSpec
{
    const char* description;
    bool do_cancel;
    bool do_repeat;

    friend std::ostream& operator<<(std::ostream& os, const TestSpec& ts)
    {
        std::ostringstream oss;
        oss << std::boolalpha;
        oss << "{"
            <<    "desc:" << ts.description << ","
            <<    "do_cancel:" << ts.do_cancel << ","
            <<    "do_repeat:" << ts.do_repeat
            << "}";

        return os << oss.str();
    }
};


void run_test(const TestSpec& test_spec)
{
    using namespace std::literals::chrono_literals;

    auto test_tracer = FancyTracer(test_spec);

    utils::DataChannel<std::chrono::time_point<std::chrono::steady_clock>> ch;

    shri314::timer::Timer timer;

    std::thread thread{
        [&]()
        {
            ASSERT_EQ( timer.task_count(), 0u );
            ASSERT_EQ( timer.running(), false );

            timer.run();
        }
    };

    shri314::utils::ScopedExit ex{
        [&]()
        {
            timer.request_stop();

            ASSERT_EQ( timer.wait_stop(2s), true );

            thread.join();

            ASSERT_EQ( timer.running(), false );
        }
    };

    ASSERT_EQ( timer.wait_start(2s), true );
    ASSERT_EQ( timer.task_count(), 0u );
    ASSERT_EQ( timer.running(), true );

    const static auto schedule_after = 600ms;
    const static auto repeat_every   = 200ms;
    const static auto wait_midway    = schedule_after / 2;
    const static auto wait_fire      = 1s;

    auto start_time = std::chrono::steady_clock::now();

    auto tok = [&]()
    {
        if(test_spec.do_repeat)
        {
            return timer.schedule(
                    schedule_after,
                    [&ch]() mutable
                    {
                        auto fire_time = std::chrono::steady_clock::now();

                        auto trace = SimpleTracer("TASK EXEC");

                        ch.post_data(fire_time);
                    },
                    repeat_every
                );
        }
        else
        {
            return timer.schedule(
                    schedule_after,
                    [&ch]() mutable
                    {
                        auto fire_time = std::chrono::steady_clock::now();

                        auto trace = SimpleTracer("TASK EXEC");

                        ch.post_data(fire_time);
                    }
                );
        }
    }();

    {
        ASSERT_EQ( tok.expired(), false );

        TRACE(" => Task Scheduled to run after ", schedule_after);

        ASSERT_EQ( timer.task_count(), 1u );
    }

    auto check_exec_times = [](auto start_time, auto& exec_times)
    {
        for(size_t i = 0u; i < exec_times.size(); ++i)
        {
            if(i == 0u)
            {
                auto t1 = start_time;
                auto t2 = exec_times[i];

                ASSERT_GE( t2 - t1, schedule_after );
            }
            else
            {
                auto t1 = exec_times[i - 1];
                auto t2 = exec_times[i];

                ASSERT_GE( t2 - t1, repeat_every );
            }
        }
    };

    auto wait_until_data = [&](const char* desc, size_t th, auto duration)
    {
        TRACE(" => ", desc, " wait beg, for: ", duration, ", count_threshold: ", th);

        auto result = ch.wait_until_data(th, wait_midway);

        TRACE(" => ", desc, " wait end, got_data: ", result.first);

        return result;
    };

    (void)wait_until_data;

    {
        auto&& [got_data, exec_times] = wait_until_data("midway", 1u, wait_midway);

        ASSERT_EQ( got_data,           false );
        ASSERT_EQ( exec_times.size(),  0u    );
        ASSERT_EQ( timer.task_count(), 1u    );
        ASSERT_EQ( tok.expired(),      false );
    }

    if (test_spec.do_cancel)
    {
        tok.cancel();
    }

    {
        auto&& [got_data, exec_times] = wait_until_data("first", 1u, wait_fire);

        ASSERT_EQ( got_data,           test_spec.do_cancel ? false : true );
        ASSERT_EQ( exec_times.size(),  test_spec.do_cancel ? 0u    : 1u );
        ASSERT_EQ( timer.task_count(), test_spec.do_cancel ? 0u    : (test_spec.do_repeat ? 1u : 0u) );
        ASSERT_EQ( tok.expired(),      test_spec.do_cancel ? true  : (test_spec.do_repeat ? false : true) );

        check_exec_times(start_time, exec_times);
    }

    if (!test_spec.do_cancel && test_spec.do_repeat)
    {
        const size_t extra_reps = 3u;

        for(size_t i = 0u; i < extra_reps; ++i)
        {
            auto&& [got_data, exec_times] = wait_until_data("next", i + 2u, wait_fire);

            ASSERT_EQ( got_data,           true   );
            ASSERT_EQ( exec_times.size(),  i + 2u );
            ASSERT_EQ( timer.task_count(), 1u     );
            ASSERT_EQ( tok.expired(),      false  );
        }

        // stop yourself from running for ever
        tok.cancel();

        {
            auto&& [got_data, exec_times] = wait_until_data("last", extra_reps + 2u, wait_fire);

            ASSERT_EQ( got_data,           false           );
            ASSERT_EQ( exec_times.size(),  extra_reps + 1u );
            ASSERT_EQ( timer.task_count(), 0u              );
            ASSERT_EQ( tok.expired(),      true            );

            check_exec_times(start_time, exec_times);
        }
    }
}


int main()
{
    run_test({"ONE_SHOT", false, false});
    run_test({"ONE_SHOT", true, false});
    run_test({"REPEATING", false, true});
    run_test({"REPEATING", true, true});

    return 0;
}
