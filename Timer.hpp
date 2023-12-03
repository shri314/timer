/**
 * Timer
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include "ScopedAction.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

struct Timer
{
public:
    struct Token;

private:
    struct Entry;

    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = std::chrono::time_point<Clock>;
    using ScheduledTasks = std::multimap<TimePoint, std::shared_ptr<Entry>>;

    struct Entry
    {
    public:
        friend class Timer;

        template<class FuncT>
        explicit
        Entry(Timer* owner, FuncT&& callback, Duration every);

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
        Entry(Entry&&) = delete;
        Entry& operator=(Entry&&) = delete;

        inline void link_self(ScheduledTasks::iterator pos);
        inline void cancel_self();
        inline void safe_invoke();

    private:
        Timer* m_owner;
        std::function<void()> m_callback;
        Duration m_every;
        ScheduledTasks::iterator m_pos;
    };

public:
    friend struct Token;

    Timer() = default;

    template<class FuncT>
    inline auto schedule(Duration delay, FuncT&& func, Duration every = Duration::zero()) -> Token;

    inline void run();
    inline bool running() const;
    inline void request_stop();
    inline bool stop_requested() const;

    inline bool wait_start(Duration timeout);
    inline bool wait_stop(Duration timeout);
    inline size_t task_count();

private:
    inline auto schedule_entry_locked(Duration delay, std::shared_ptr<Entry>&& entry) -> std::weak_ptr<Entry>;
    inline auto schedule_entry_direct(Duration delay, std::shared_ptr<Entry>&& entry, const std::unique_lock<std::mutex>&) -> ScheduledTasks::iterator;
    inline void cancel_at_locked(ScheduledTasks::iterator pos);

private:

private:
    ScheduledTasks m_tasks;
    std::mutex m_tasks_mutex;
    std::condition_variable m_tasks_cv;
    std::atomic_bool m_stop_req{false};

    std::mutex m_run_mutex;
    std::condition_variable m_run_cv;
    std::atomic_bool m_running{false};
};


struct [[nodiscard]] Timer::Token
{
public:
    friend class Timer;

    inline void cancel()
    {
        if (auto entry_sp = m_entry_wref.lock())
        {
            entry_sp->cancel_self();
        }
    }

    Token() = default;
    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;

    Token(Token&&) = default;
    Token& operator=(Token&&) = default;

    ~Token()
    {
        this->cancel();
    }

    bool expired() const
    {
        return m_entry_wref.expired();
    }

private:
    explicit
    Token(std::weak_ptr<Entry> entry_ref)
        : m_entry_wref(std::move(entry_ref))
    {
    }

private:
    std::weak_ptr<Entry> m_entry_wref;
};


template<class FuncT>
inline auto Timer::schedule(Duration delay, FuncT&& func, Duration every) -> Token
{
    auto entry = std::make_shared<Entry>(
                        this,
                        std::forward<FuncT>(func),
                        std::move(every)
                    );

    return Token{ schedule_entry_locked(delay, std::move(entry)) };
}


inline void Timer::run()
{
    m_stop_req = false;

    std::vector<ScheduledTasks::node_type> nodes;

    while(true)
    {
        ScopedAction flipper{
            [&]() { this->m_running = true; m_run_cv.notify_one(); },
            [&]() { this->m_running = false; m_run_cv.notify_one(); }
        };


        while(true)
        {
            std::unique_lock lock{m_tasks_mutex};

            m_tasks_cv.wait(
                lock,
                [&] { return m_stop_req || !m_tasks.empty(); }
            );

            if(m_stop_req)
            {
                return;
            }

            TimePoint next_until = m_tasks.begin()->first;
            if( auto st = m_tasks_cv.wait_until(lock, next_until);
                st == std::cv_status::timeout )
            {
                // NOTE: extract eligible nodes off m_tasks that at the top
                auto [beg, end] = m_tasks.equal_range(next_until);
                for(auto i = beg; i != end; ++i)
                {
                    nodes.push_back( std::move(m_tasks.extract(i)) );
                }

                // NOTE: re-arm as required
                for(auto& nh : nodes)
                {
                    auto entry = nh.mapped();

                    if(entry->m_every != Duration::zero())
                    {
                        this->schedule_entry_direct(entry->m_every, std::move(entry), lock);
                    }
                }

                break;
            }
        }

        // clear all nodes
        while(!nodes.empty())
        {
            auto nh = std::move(nodes.back());

            nodes.pop_back();

            // NOTE: callback invoke()ed
            //       while no locks held
            nh.mapped()->safe_invoke();
        }
    }
}


inline bool Timer::running() const
{
    return m_running;
}


inline bool Timer::stop_requested() const
{
    return m_stop_req;
}


inline void Timer::request_stop()
{
    m_stop_req = true;
    m_tasks_cv.notify_one();
}


inline bool Timer::wait_start(Duration timeout)
{
    std::unique_lock lock{m_run_mutex};
    bool b = m_run_cv.wait_for(
                lock,
                timeout,
                [&] { return m_running == true; }
            );
    return b;
}


inline bool Timer::wait_stop(Duration timeout)
{
    std::unique_lock lock{m_run_mutex};
    return m_run_cv.wait_for(
                lock,
                timeout,
                [&] { return m_running == false; }
            );
}


inline size_t Timer::task_count()
{
    std::lock_guard g{m_tasks_mutex};
    return m_tasks.size();
}


inline auto Timer::schedule_entry_locked(Duration delay, std::shared_ptr<Entry>&& entry) -> std::weak_ptr<Entry>
{
    std::unique_lock lock{m_tasks_mutex};

    auto pos = schedule_entry_direct(delay, std::move(entry), lock);

    if(pos == m_tasks.begin())
    {
        lock.unlock();
        m_tasks_cv.notify_one();
    }

    return pos->second;
}


inline auto Timer::schedule_entry_direct(Duration delay, std::shared_ptr<Entry>&& entry, const std::unique_lock<std::mutex>&) -> ScheduledTasks::iterator
{
    auto pos = m_tasks.emplace(
            Clock::now() + delay, std::move(entry)
        );

    pos->second->link_self(pos);

    return pos;
}


inline void Timer::cancel_at_locked(ScheduledTasks::iterator pos)
{
    std::unique_lock lock{m_tasks_mutex};

    const bool notify = pos == m_tasks.begin();

    m_tasks.erase(pos);

    if (notify)
    {
        lock.unlock();
        m_tasks_cv.notify_one();
    }
}


template<class FuncT>
Timer::Entry::Entry(Timer* owner, FuncT&& callback, Duration every)
    : m_owner{owner}
    , m_callback{std::forward<FuncT>(callback)}
    , m_every{std::move(every)}
{
}


inline void Timer::Entry::cancel_self()
{
    m_owner->cancel_at_locked(m_pos);
}


inline void Timer::Entry::link_self(ScheduledTasks::iterator pos)
{
    m_pos = pos;
}


inline void Timer::Entry::safe_invoke()
{
    try
    {
        m_callback();
    }
    catch(...)
    {
        // swallow exception from callbacks
    }
}
