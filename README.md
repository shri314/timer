# timer

This is a C++ header only timer library - that allows scheduling of functions
to be excecuted after a specified duration in time has elapsed. The bundled
tests can be built and run.

# Prequisites for using library

 - A C++17 compliant compiler.
    - Tested with g++ 9.4.0       (Ubuntu20.04)
    - Tested with clang++ 14.0.0  (Ubuntu22.04)

# Prequisites for building Tests

 - cmake (a fairly recent version >= 3.12 will work)
 - make

*Warning:* On g++11 on Ubuntu22.04 has some problems in the thread sanitizer
module running into false positives. See https://gcc.gnu.org/bugzilla//show_bug.cgi?id=101978

# Building Tests

    #Get the source
    git clone https://github.com/shri314/timer.git

    cd timer
    ( mkdir -p out && cd out && cmake .. )
    make -C out VERBOSE=1 -j4

the above will build release, debug, and sanitizer versions of the test binaries

# Running Tests

    ./a.out.release
    ./a.out.tsan

# Library Documentation

*Creating the timer:*

    #include <shri314/timer/Timer.hpp>

    shri314::timer::Timer t;

One one needs to call `t.run()` on a separate thread. This function blocks
until someone stops the timer using a `t.request_stop()`.

*Scheduling a one-shot function:*

    auto token = t.schedule( 5s, []() { std::cout << "hello timer\n"; } );
                             ^^

This will cause the function provided to be executed after 5sec, as long as:
   - `t.run()` is stil running
   - `token` is kept alive and is in scope (not destroyed)
   - `token.cancel()` has not been called.
   - `token.request_stop()` has not been called.
   - the callback has not been already executed.

The `token` object helps keep the registration alive. when token goes out of
scope, it automatically issues a cancel(). Before the callback, the token
object can be used to cancel the registration. After the callback the token
object when queried will indicate that it has expired.

*Scheduling a repeating function:*

    auto token = t.schedule( 5s, []() { std::cout << "hello timer\n"; }, 10s );
                                                                         ^^^
The behavior is like the previous example, except, that at after the first call
subsequently, the same function will be called every 10s. Unlike the one shot
case, the token object will continue to indicate that is has not expired.
