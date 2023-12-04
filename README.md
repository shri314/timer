# timer

This is a C++ header only timer library - that allows scheduling of functions
to be excecuted at a future point in time.

*Creating the timer:*

    #include <shri314/timer/Timer.hpp>

    shri314::timer::Timer t;

One one needs to call `t.run()` on a separate thread. This function blocks
until someone stops the timer using a `t.request_stop()`.

*Scheduling a one-shot function:*

    auto token = t.schedule( 5s, []() { std::cout << "hello timer\n"; } );
                             ^^

This will cause the function provided to be executed after 5sec, as long as:
1. `t.run()` is stil running
1. `token` is kept alive and is in scope (not destroyed)
1. `token.cancel()` has not been called.
1. `token.request_stop()` has not been called.
1. the callback has not been already executed.

The `token` object can help keep the registration alive, as long as the object
is alive. it automatically issues a cancel() if it goes out of scope.

*Scheduling a repeating function:*

    auto token = t.schedule( 5s, []() { std::cout << "hello timer\n"; }, 10s );
                                                                         ^^^

The behavior is same as previous call, except, that at after the first call
subsequently, the same callback will be called every 10s.


#Prequisites to build:
 - A C++17 compliant compiler.
    - Tested with g++ 9.4.0       (Ubuntu20.04)
    - Tested with clang++ 14.0.0  (Ubuntu22.04)

 - cmake (a fairly recent version >= 3.12 will work)

 - Warning:
    - g++11 on Ubuntu22.04 has some problems in the thread sanitizer module
      running into false positives - See https://gcc.gnu.org/bugzilla//show_bug.cgi?id=101978

# Building

    git clone https://github.com/shri314/timer.git
    cd timer
    ( mkdir -p out && cd out && cmake .. )
    make -C out VERBOSE=1 -j4

the above will build release, debug, and sanitizer versions of the test binaries

# Running

    ./a.out.release
    ./a.out.tsan

