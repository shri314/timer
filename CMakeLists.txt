cmake_minimum_required(VERSION 3.12)
project(timer)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pthread")

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SRCS test/main.cpp)

add_executable(a.out.release         ${SRCS})
target_compile_options(a.out.release PRIVATE "-O3")

add_executable(a.out.debug         ${SRCS})
target_compile_options(a.out.debug PRIVATE "-g3" "-DDEBUG=1")

add_executable(a.out.asan         ${SRCS})
target_compile_options(a.out.asan PRIVATE "-fsanitize=address")
target_link_options(a.out.asan    PRIVATE "-fsanitize=address")

#add_executable(a.out.msan         ${SRCS})
#target_compile_options(a.out.msan PRIVATE "-fsanitize=memory")
#target_link_options(a.out.msan    PRIVATE "-fsanitize=memory")

add_executable(a.out.tsan         ${SRCS})
target_compile_options(a.out.tsan PRIVATE "-g3" "-fsanitize=thread")
target_link_options(a.out.tsan    PRIVATE "-g3" "-fsanitize=thread")

add_executable(a.out.usan         ${SRCS})
target_compile_options(a.out.usan PRIVATE "-fsanitize=undefined")
target_link_options(a.out.usan    PRIVATE "-fsanitize=undefined")

