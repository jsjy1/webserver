

add_library(thread_synchr STATIC)

target_link_libraries(thread_synchr
PRIVATE
pthread
)

target_sources(thread_synchr
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/thread_synchr.cpp
)

target_include_directories(thread_synchr
INTERFACE
${CMAKE_CURRENT_SOURCE_DIR}
)