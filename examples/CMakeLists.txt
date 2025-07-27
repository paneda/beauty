project(beauty_example)

add_executable(${PROJECT_NAME}
	pc/main.cpp
	pc/file_io.cpp
	pc/my_file_api.cpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/examples/pc
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
