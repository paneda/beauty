project(beauty_example)

add_executable(${PROJECT_NAME}
	pc/main.cpp
	pc/file_io.cpp
	pc/my_file_api.cpp
	pc/my_router_api.cpp
)

include_directories(
	${CMAKE_SOURCE_DIR}/examples/pc
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

# WebSocket client example
add_executable(beauty_ws_client_example
	pc/ws_client_main.cpp
)

target_include_directories(beauty_ws_client_example PRIVATE
	${CMAKE_SOURCE_DIR}/examples/pc
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(beauty_ws_client_example PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(beauty_ws_client_example PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Interactive WebSocket chat client example
add_executable(beauty_ws_chat_client_example
	pc/ws_chat_client_main.cpp
)

target_include_directories(beauty_ws_chat_client_example PRIVATE
	${CMAKE_SOURCE_DIR}/examples/pc
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(beauty_ws_chat_client_example PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(beauty_ws_chat_client_example PRIVATE -Wall -Wextra -Wpedantic -Werror)

# HTTP client example
add_executable(beauty_http_client_example
	pc/http_client_main.cpp
)

target_include_directories(beauty_http_client_example PRIVATE
	${CMAKE_SOURCE_DIR}/examples/pc
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(beauty_http_client_example PRIVATE asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(beauty_http_client_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
