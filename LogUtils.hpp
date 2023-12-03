#include "ScopedAction.hpp"

#include <chrono>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <iostream>
#include <cassert>
#include <ctime>

template<class Clock, class Duration>
std::ostream& operator<<(std::ostream& os, std::chrono::time_point<Clock, Duration> current_time_point)
{
    using namespace std::chrono;

    const time_t current_time = Clock::to_time_t(current_time_point);

    auto tm = std::tm{}; localtime_r(&current_time, &tm);

    const auto duration_since_epoch = current_time_point.time_since_epoch();
    const auto current_milliseconds = duration_cast<milliseconds>(duration_since_epoch).count() % 1000;

    std::ostringstream oss;
    oss << std::put_time(&tm, "%F %T") << "." << std::setw(3) << std::setfill('0') << current_milliseconds;
    return os << oss.str();
}

template<class Rep, class Period>
std::ostream& operator<<(std::ostream& os, std::chrono::duration<Rep, Period> dur)
{
    if constexpr (std::is_same_v<Period, std::nano>)
    {
        return os << dur.count() << "ns";
    }
    else if constexpr (std::is_same_v<Period, std::micro>)
    {
        return os << dur.count() << "us";
    }
    else if constexpr (std::is_same_v<Period, std::milli>)
    {
        return os << dur.count() << "ms";
    }
    else if constexpr (std::is_same_v<Period, std::ratio<1>>)
    {
        return os << dur.count() << "s";
    }
    else if constexpr (std::is_same_v<Period, std::ratio<60>>)
    {
        return os << dur.count() << "min";
    }
    else if constexpr (std::is_same_v<Period, std::ratio<3600>>)
    {
        return os << dur.count() << "hrs";
    }
    else
    {
        // not implemented
        static_assert( std::is_same_v<Period, void> );

        return os;
    }
}

std::mutex cerr_mutex;

template<class... T>
void TRACE(T&&... t)
{
    std::ostringstream oss;
    oss << std::boolalpha;
    ( (oss << std::chrono::system_clock::now() << "::") << ... << std::forward<T>(t) );

    std::lock_guard lg{cerr_mutex};
    std::cerr << oss.str() << std::endl;
}


template<class T>
auto FancyTracer(T x)
{
    return ScopedAction{
        [x]()
        {
            TRACE("-----------------------------------------------");
            TRACE(" BEG - ", x);
        },
        [x]()
        {
            TRACE(" END - ", x);
            TRACE("-----------------------------------------------");
            TRACE();
        }
    };
}


auto SimpleTracer(std::string name)
{
    return ScopedAction{
        [name]() { TRACE(" => ", name, " BEG"); },
        [name]() { TRACE(" => ", name, " END"); }
    };
}


#define ASSERT_EQ( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l == r)) \
        { \
            TRACE("FAILED: [", #a, "] == [", #b, "]"); \
            TRACE(" as in: [", l,  "] != [", r,  "]"); \
            assert(false); \
        } \
    }while(0)


#define ASSERT_NE( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l != r)) \
        { \
            TRACE("FAILED: [", #a, "] != [", #b, "]"); \
            TRACE(" as in: [", l,  "] == [", r,  "]"); \
            assert(false); \
        } \
    }while(0)


#define ASSERT_LT( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l < r)) \
        { \
            TRACE("FAILED: [", #a, "] < [", #b, "]"); \
            TRACE(" as in: [", l,  "] >= [", r,  "]"); \
            assert(false); \
        } \
    }while(0)


#define ASSERT_LE( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l <= r)) \
        { \
            TRACE("FAILED: [", #a, "] <= [", #b, "]"); \
            TRACE(" as in: [", l,  "] > [", r,  "]"); \
            assert(false); \
        } \
    }while(0)


#define ASSERT_GT( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l > r)) \
        { \
            TRACE("FAILED: [", #a, "] > [", #b, "]"); \
            TRACE(" as in: [", l,  "] <= [", r,  "]"); \
            assert(false); \
        } \
    }while(0)


#define ASSERT_GE( a, b ) \
    do { \
        auto l = (a); \
        auto r = (b); \
        if(!(l >= r)) \
        { \
            TRACE("FAILED: [", #a, "] >= [", #b, "]"); \
            TRACE(" as in: [", l,  "] < [", r,  "]"); \
            assert(false); \
        } \
    }while(0)
