/**
 * DataChannel
 *
 * MIT License
 * Copyright (c) 2023 Shriram V
 */
#pragma once

#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

template<class T>
struct DataChannel
{
    void post_data(T data)
    {
        std::unique_lock lock{m_mutex};

        m_data_seq.push_back(std::move(data));

        lock.unlock();
        m_cv.notify_one();
    }


    template<class Rep, class Period>
    std::pair<bool, std::vector<T>> wait_until_data(size_t threshold, const std::chrono::duration<Rep, Period>& timeout_duration)
    {
        std::unique_lock lock{m_mutex};

        bool b = m_cv.wait_for(
                        lock,
                        timeout_duration,
                        [&]()
                        {
                            return m_data_seq.size() >= threshold;
                        }
                    );

        return std::make_pair(b, m_data_seq);
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<T> m_data_seq;
};
