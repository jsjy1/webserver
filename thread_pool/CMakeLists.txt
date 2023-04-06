
add_library(thread_pool STATIC)

target_sources(thread_pool
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/thread_pool.hpp
)

target_link_libraries(thread_pool
PRIVATE
pthread
thread_synchr
http_user
)

target_include_directories(thread_pool
INTERFACE
${CMAKE_CURRENT_SOURCE_DIR}
)