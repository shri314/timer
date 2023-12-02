/**
 * main test code
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#include "Timer.hpp"
#include "DataChannel.hpp"
#include "ScopedExit.hpp"
#include "LogUtils.hpp"

#include <thread>
#include <iostream>

enum class TEST_CASE
{
    ONE_SHOT,
    ONE_SHOT_CANCEL,
    REPEATING,
    REPEATING_CANCEL,
};


std::ostream& operator<<(std::ostream& os, TEST_CASE tc)
{
    switch(tc)
    {
        case TEST_CASE::ONE_SHOT:         return os << "TEST_CASE::ONE_SHOT";
        case TEST_CASE::ONE_SHOT_CANCEL:  return os << "TEST_CASE::ONE_SHOT_CANCEL";
        case TEST_CASE::REPEATING:        return os << "TEST_CASE::REPEATING";
        case TEST_CASE::REPEATING_CANCEL: return os << "TEST_CASE::REPEATING_CANCEL";
    }

    return os << "TEST_CASE::?";
}


void test_one_shot(TEST_CASE test_case)
{
    using namespace std::literals::chrono_literals;

    auto test_tracer = FancyTracer(test_case);

    DataChannel<std::chrono::time_point<std::chrono::steady_clock>> ch;

    Timer timer;

    std::thread thread{
        [&]()
        {
            ASSERT_EQ( timer.task_count(), 0u );
            ASSERT_EQ( timer.running(), false );

            timer.run();
        }
    };

    ScopedExit ex{
        [&]()
        {
            timer.request_stop();

            thread.join();

            ASSERT_EQ(timer.running(), false);
        }
    };

    std::this_thread::sleep_for(1s);
    ASSERT_EQ( timer.task_count(), 0u );
    ASSERT_EQ( timer.running(), true );

    const static auto schedule_after = 5000ms;
    const static auto repeat_every   = 3000ms;
    const static auto wait_midway    = 3000ms;
    const static auto wait_fire      = 5000ms;

    auto start_time = std::chrono::steady_clock::now();

    auto tok = [&]() -> Timer::Token
    {
        switch(test_case)
        {
            case TEST_CASE::ONE_SHOT:
                [[std::fallthrough]];

            case TEST_CASE::ONE_SHOT_CANCEL:
                return timer.schedule(
                        schedule_after,
                        [&ch]() mutable
                        {
                            auto fire_time = std::chrono::steady_clock::now();

                            auto trace = SimpleTracer("TASK EXEC");

                            ch.post_data(fire_time);
                        }
                    );

            case TEST_CASE::REPEATING:
                [[std::fallthrough]];

            case TEST_CASE::REPEATING_CANCEL:
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

        return {};
    }();

    {
        ASSERT_EQ( tok.expired(), false );

        TRACE(" => Task Scheduled");
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

    switch(test_case)
    {
        case TEST_CASE::ONE_SHOT:
            {
                auto&& [b, exec_times] = ch.wait_until_data(1u, wait_midway);

                TRACE(" => Midway Wait1");

                ASSERT_EQ( b, false ); // timed out
                ASSERT_EQ( exec_times.size(), 0u );
                ASSERT_EQ( timer.task_count(), 1u );
                ASSERT_EQ( tok.expired(), false );
            }

            {
                auto&& [b, exec_times] = ch.wait_until_data(1u, wait_fire);

                TRACE(" => Final Wait");

                ASSERT_EQ( b, true ); // not timed out
                ASSERT_EQ( exec_times.size(), 1u );
                ASSERT_EQ( timer.task_count(), 0u );
                ASSERT_EQ( tok.expired(), true );

                check_exec_times(start_time, exec_times);
            }

            break;

        case TEST_CASE::ONE_SHOT_CANCEL:
                [[std::fallthrough]];

        case TEST_CASE::REPEATING_CANCEL:
            {
                auto&& [b, exec_times] = ch.wait_until_data(1u, wait_midway);

                TRACE(" => Midway Wait1");

                ASSERT_EQ( b, false ); // timed out
                ASSERT_EQ( exec_times.size(), 0u );
                ASSERT_EQ( timer.task_count(), 1u );
                ASSERT_EQ( tok.expired(), false );
            }

            tok.cancel();

            {
                auto&& [b, exec_times] = ch.wait_until_data(1u, wait_fire);

                TRACE(" => Final Wait");

                ASSERT_EQ( b, false ); // timed out
                ASSERT_EQ( timer.task_count(), 0u );
                ASSERT_EQ( exec_times.size(), 0u );
                ASSERT_EQ( tok.expired(), true );

                check_exec_times(start_time, exec_times);
            }

            break;

        case TEST_CASE::REPEATING:
            {
                auto&& [b, exec_times] = ch.wait_until_data(1u, 3s);

                TRACE(" => Midway Wait1");

                ASSERT_EQ( b, false );
                ASSERT_EQ( exec_times.size(), 0u );
                ASSERT_EQ( timer.task_count(), 1u );
                ASSERT_EQ( tok.expired(), false );
            }

            size_t do_reps = 3u;
            for(size_t i = 0u; i < do_reps; ++i)
            {
                auto&& [b, exec_times] = ch.wait_until_data(i + 1u, wait_fire);

                TRACE(" => Next Wait: ", i + 1u);

                ASSERT_EQ( b, true ); // not timed out
                ASSERT_EQ( exec_times.size(), i + 1u );
                ASSERT_EQ( timer.task_count(), 1u );
                ASSERT_EQ( tok.expired(), false );
            }

            tok.cancel();

            {
                auto&& [b, exec_times] = ch.wait_until_data(do_reps + 1u, wait_fire);

                TRACE(" => Final Wait");

                ASSERT_EQ( b, false ); // timed out
                ASSERT_EQ( exec_times.size(), do_reps );
                ASSERT_EQ( timer.task_count(), 0u );
                ASSERT_EQ( tok.expired(), true );

                check_exec_times(start_time, exec_times);
            }

            break;
    }
}


int main()
{
    test_one_shot(TEST_CASE::ONE_SHOT);
    test_one_shot(TEST_CASE::ONE_SHOT_CANCEL);
    test_one_shot(TEST_CASE::REPEATING);
    test_one_shot(TEST_CASE::REPEATING_CANCEL);

    return 0;
}
