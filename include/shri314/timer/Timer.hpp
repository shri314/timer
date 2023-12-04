/**
 * shri314::timer::Timer
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include "shri314/utils/ScopedAction.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace shri314::timer
{

class Timer
{
public:
    class Token;
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = std::chrono::time_point<Clock>;

private:
    struct Entry;
    struct EntryLocator;
    using ScheduledTasks = std::multimap<TimePoint, Entry>;

private:
    struct Entry
    {
        //friend class Timer;

        template<class FuncT>
        explicit
        Entry(FuncT&& callback, Duration repeat_interval)
            : m_callback(std::move(callback))
            , m_repeat_interval(std::move(repeat_interval))
        {
        }

        Duration repeat_interval() const
        {
            return m_repeat_interval;
        }

        bool is_repeating() const
        {
            return m_repeat_interval != Duration::zero();
        }

        void link_locator(ScheduledTasks::iterator pos)
        {
            if (!m_locator)
            {
                m_locator = std::make_shared<EntryLocator>(pos);
            }
            else
            {
                m_locator->set_pos(pos);
            }
        }

        void del_locator()
        {
            m_locator->invalidate();
            m_locator.reset();
        }

        void safe_invoke()
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

        Token get_token(Timer* owner)
        {
            return Token{owner, m_locator};
        }

    private:
        std::function<void()> m_callback;
        Duration m_repeat_interval;
        std::shared_ptr<EntryLocator> m_locator;
    };



    struct EntryLocator
    {
    public:
        friend class Timer;

        explicit
        EntryLocator(ScheduledTasks::iterator pos)
            : m_pos{pos}
        {
        }

        EntryLocator(const EntryLocator&) = delete;
        EntryLocator& operator=(const EntryLocator&) = delete;
        EntryLocator(EntryLocator&&) = delete;
        EntryLocator& operator=(EntryLocator&&) = delete;

        void set_pos(ScheduledTasks::iterator pos)
        {
            // a lock is held
            m_pos = pos;
        }

        ScheduledTasks::iterator get_pos() const
        {
            // a lock is held
            return m_pos;
        }

        bool is_valid() const
        {
            return m_is_valid;
        }

        void invalidate()
        {
            m_is_valid = false;
        }

    private:
        ScheduledTasks::iterator m_pos;
        std::atomic_bool m_is_valid{true};
    };


public:
    class [[nodiscard]] Token
    {
    public:
        friend class Timer;

        inline bool cancel()
        {
            if (auto loc_sp = m_loc_wp.lock())
            {
                return m_owner->cancel_by(*loc_sp);
            }

            return false;
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
            if (auto loc_sp = m_loc_wp.lock())
            {
                return !loc_sp->is_valid();
            }

            return true;
        }

    private:
        explicit
        Token(Timer* owner, std::weak_ptr<EntryLocator> loc_wp)
            : m_owner(owner)
            , m_loc_wp(std::move(loc_wp))
        {
        }

    private:
        Timer* m_owner;
        std::weak_ptr<EntryLocator> m_loc_wp;
    };


public:
    Timer() = default;
    ~Timer() = default;

    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

    template<class FuncT>
    auto schedule(Duration delay, FuncT&& func, Duration repeat_interval = Duration::zero()) -> Token
    {
        std::unique_lock lock{m_tasks_mutex};

        auto pos = emplace_link(m_tasks, std::move(delay), std::move(func), std::move(repeat_interval));

        if(pos == m_tasks.begin())
        {
            lock.unlock();
            m_tasks_cv.notify_one();
        }

        return pos->second.get_token(this);
    }

    void run()
    {
        m_stop_req = false;

        std::vector<ScheduledTasks::node_type> nodes;

        while(true)
        {
            nodes.clear();

            utils::ScopedAction flipper{
                [&]() {
                    std::unique_lock lock{m_run_mutex};
                    this->m_running = true;

                    lock.unlock();
                    m_run_cv.notify_all();
                },
                [&]() {
                    std::unique_lock lock{m_run_mutex};
                    this->m_running = false;

                    lock.unlock();
                    m_run_cv.notify_all();
                }
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
                        nodes.push_back( m_tasks.extract(i) );
                    }

                    for (auto& nh : nodes)
                    {
                        auto entry = nh.mapped();

                        if(entry.is_repeating())
                        {
                            // re-arm internally for next repeat
                            emplace_link(m_tasks, entry.repeat_interval(), std::move(entry));
                        }
                        else
                        {
                            // explicitly delete the locator
                            entry.del_locator();
                        }
                    }

                    break;
                }
            }

            // invoke callback for each node
            for (auto& nh : nodes)
            {
                // NOTE: callback invoke()ed
                //       while no locks held
                nh.mapped().safe_invoke();
            }
        }
    }


    bool is_running() const
    {
        return m_running;
    }

    bool wait_start(Duration timeout)
    {
        std::unique_lock lock{m_run_mutex};

        return m_run_cv.wait_for(
                    lock,
                    timeout,
                    [&] { return m_running == true; }
                );
    }

    void request_stop()
    {
        std::unique_lock lock{m_tasks_mutex};
        m_stop_req = true;

        lock.unlock();
        m_tasks_cv.notify_one();
    }

    bool is_stop_requested() const
    {
        return m_stop_req;
    }

    bool wait_stop(Duration timeout)
    {
        std::unique_lock lock{m_run_mutex};

        return m_run_cv.wait_for(
                    lock,
                    timeout,
                    [&] { return m_running == false; }
                );
    }

    size_t task_count()
    {
        std::lock_guard lg{m_tasks_mutex};

        return m_tasks.size();
    }


private:
    template<class... Args>
    static auto emplace_link(ScheduledTasks& tasks, Duration delay, Args&&... args) -> ScheduledTasks::iterator
    {
        // should be called under lock

        auto pos = tasks.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(Clock::now() + delay),
                std::forward_as_tuple(std::forward<Args>(args)...)
            );

        pos->second.link_locator(pos);

        return pos;
    }

    bool cancel_by(EntryLocator& loc)
    {
        std::unique_lock lock{m_tasks_mutex};

        if( loc.is_valid() )
        {
            auto&& pos = loc.get_pos();

            const bool notify = pos == m_tasks.begin();

            m_tasks.erase(pos);

            loc.invalidate();

            if (notify)
            {
                lock.unlock();
                m_tasks_cv.notify_one();
            }

            return true;
        }

        return false;
    }

private:
    ScheduledTasks m_tasks;
    std::mutex m_tasks_mutex;
    std::condition_variable m_tasks_cv;
    std::atomic_bool m_stop_req{false};

    std::mutex m_run_mutex;
    std::condition_variable m_run_cv;
    std::atomic_bool m_running{false};
};



}
