


cmake_minimum_required(VERSION 3.10.2)

project(WEB CXX)

add_executable(main main.cpp)

add_subdirectory(thread_pool)
add_subdirectory(thread_synchr)
add_subdirectory(http_user)
add_subdirectory(timer_stop)

target_link_libraries(main
PRIVATE
thread_pool
thread_synchr
http_user
timer_stop
)

