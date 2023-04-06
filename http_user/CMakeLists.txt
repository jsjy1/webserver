
add_library(http_user STATIC)

target_sources(http_user
PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}/http_user.cpp
)

target_include_directories(http_user
INTERFACE
${CMAKE_CURRENT_SOURCE_DIR}
)