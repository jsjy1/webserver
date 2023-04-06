

add_library(timer_stop STATIC)

target_sources(timer_stop
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/timer_stop.cpp
)

target_link_libraries(timer_stop
PRIVATE
http_user
thread_synchr
)

target_include_directories(timer_stop
INTERFACE
${CMAKE_CURRENT_SOURCE_DIR}
)