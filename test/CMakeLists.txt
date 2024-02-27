add_executable(asio_web_test
	server_test.cpp
	${CMAKE_SOURCE_DIR}/src/server.cpp
	${CMAKE_SOURCE_DIR}/src/connection.cpp
	${CMAKE_SOURCE_DIR}/src/connection_manager.cpp
	${CMAKE_SOURCE_DIR}/src/mime_types.cpp
	${CMAKE_SOURCE_DIR}/src/reply.cpp
	${CMAKE_SOURCE_DIR}/src/request_handler.cpp
	${CMAKE_SOURCE_DIR}/src/request_parser.cpp
)
target_link_libraries(asio_web_test PRIVATE Catch2::Catch2WithMain asio::asio)
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

# define tests
add_test(
	NAME catch_test
	COMMAND $<TARGET_FILE:asio_web_test> --success
)
