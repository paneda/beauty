project(beauty_example)

add_executable(${PROJECT_NAME}
	pc/main.cpp
	pc/file_io.cpp
	pc/my_file_api.cpp
	pc/my_router_api.cpp
)

# Additional streaming example
add_executable(beauty_streaming_example
	pc/streaming_example.cpp
	pc/file_io.cpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/examples/pc
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Streaming example configuration
include_directories(
    ${CMAKE_SOURCE_DIR}/examples/pc
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(beauty_streaming_example PRIVATE asio::asio ${CMAKE_PROJECT_NAME})
target_compile_options(beauty_streaming_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
