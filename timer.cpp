#include <map>
#include <memory>
#include <chrono>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <iostream>

struct Timer
{
private:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = std::chrono::time_point<Clock>;

    struct Entry;

    using Map = std::multimap<TimePoint, std::shared_ptr<Entry>>;

    struct Entry : std::enable_shared_from_this<Entry>
    {
        template<class FuncT>
        explicit
        Entry(Timer* owner, FuncT&& callback, Duration every)
            : m_owner{owner}
            , m_callback{std::forward<FuncT>(callback)}
            , m_every{std::move(every)}
        {
        }

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
        Entry(Entry&&) = delete;
        Entry& operator=(Entry&&) = delete;

        void cancel_self()
        {
            m_owner->cancel_at(m_pos);
        }

        void link_self(Map::iterator pos)
        {
            m_pos = pos;
        }

        void invoke()
        {
            m_callback();

            if(m_every != Duration::zero())
            {
                m_owner->schedule_entry(m_every, std::move(this->shared_from_this()));
            }
        }

    private:
        Timer* m_owner;
        std::function<void()> m_callback;
        Duration m_every;
        Map::iterator m_pos;
    };

    struct [[nodiscard]] Token
    {
    public:
        friend class Timer;
        void cancel()
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

    private:
        Token(std::weak_ptr<Entry> entry_ref)
            : m_entry_wref(std::move(entry_ref))
        {
        }

    private:
        std::weak_ptr<Entry> m_entry_wref;
    };

public:
    friend struct Token;

    Timer() = default;

    template<class FuncT>
    Token schedule(Duration delay, FuncT&& func, Duration every = Duration::zero())
    {
        auto entry = std::make_shared<Entry>(
                this,
                std::forward<FuncT>(func),
                std::move(every)
            );

        return Token{ schedule_entry(delay, std::move(entry)) };
    }

    void run()
    {
        std::vector<Map::node_type> nodes;

        while(true)
        {
            nodes.clear();

            while(true)
            {
                std::unique_lock lock{m_mutex};

                m_cv.wait(
                    lock,
                    [&] { return m_stopped || !m_tasks.empty(); }
                );

                if(m_stopped)
                    return;

                TimePoint next_until = m_tasks.begin()->first;
                if( auto st = m_cv.wait_until(lock, next_until);
                    st == std::cv_status::timeout )
                {
                    auto [b, e] = m_tasks.equal_range(next_until);
                    for(auto i = b; i != e; ++i)
                    {
                        nodes.push_back( std::move(m_tasks.extract(i)) );
                    }

                    break;
                }
            }

            for(auto& n : nodes)
            {
                try
                {
                    // NOTE: do the callbacks outside of the lock
                    auto& entry = n.mapped();

                    entry->invoke();
                }
                catch(...)
                {
                    // FIXME: exception handling.
                    // muting the exception from the callback
                }
            }
        }
    }

    void stop()
    {
        m_stopped = true;
        m_cv.notify_one();
    }

private:
    std::weak_ptr<Entry> schedule_entry(Duration delay, std::shared_ptr<Entry>&& entry)
    {
        std::unique_lock lock{m_mutex};

        auto pos = m_tasks.emplace(
                Clock::now() + delay, std::move(entry)
            );

        pos->second->link_self(pos);

        if(pos == m_tasks.begin())
        {
            lock.unlock();
            m_cv.notify_one();
        }

        return pos->second;
    }

    void cancel_at(Map::iterator pos)
    {
        std::unique_lock lock{m_mutex};

        const bool notify = pos == m_tasks.begin();

        m_tasks.erase(pos);

        if (notify)
        {
            lock.unlock();
            m_cv.notify_one();
        }
    }

private:
    Map m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_stopped{false};
};


#include <thread>
#include <iostream>

struct JThread
{
    template<class... ArgTs>
    explicit
    JThread(ArgTs&&... args)
        : m_thread{ std::forward<ArgTs>(args)... }
    {
    }

    JThread() = delete;
    JThread(JThread&) = delete;
    JThread(const JThread&) = delete;
    JThread(JThread&&) = delete;
    JThread(const JThread&&) = delete;
    JThread& operator=(const JThread&) = delete;
    JThread& operator=(JThread&&) = delete;

    ~JThread()
    {
        m_thread.join();
    }

private:
    std::thread m_thread;
};


int main()
{
    using namespace std::literals::chrono_literals;

    Timer t;

    auto tok1 = t.schedule(2s, [i=1]() mutable { std::cout << time(0) << ": helloA:" << ++i; });
    auto tok2 = t.schedule(2s, [i=1]() mutable { std::cout << time(0) << ": helloR:" << ++i; }, 1s);
    auto tok3 = t.schedule(2s, [i=1]() mutable { std::cout << time(0) << ": helloB:" << ++i; });
    auto tok4 = t.schedule(4s, [i=1]() mutable { std::cout << time(0) << ": helloC:" << ++i; });

    JThread th{ [&]() { t.run(); } };

    for(int i = 0; i < 6; ++i)
    {
        std::cout << time(0) << ": i = " << i << "\n";
        std::this_thread::sleep_for(1s);
    }

    t.stop();
}
