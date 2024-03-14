add_executable(asio_web_test
	server_test.cpp
	file_handler_test.cpp
	request_parser_test.cpp
	request_decoder_test.cpp
	url_parser_test.cpp
	${CMAKE_SOURCE_DIR}/src/server.cpp
	${CMAKE_SOURCE_DIR}/src/connection.cpp
	${CMAKE_SOURCE_DIR}/src/connection_manager.cpp
	${CMAKE_SOURCE_DIR}/src/mime_types.cpp
	${CMAKE_SOURCE_DIR}/src/reply.cpp
	${CMAKE_SOURCE_DIR}/src/request_handler.cpp
	${CMAKE_SOURCE_DIR}/src/request_parser.cpp
	${CMAKE_SOURCE_DIR}/src/request_decoder.cpp
	${CMAKE_SOURCE_DIR}/src/file_handler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_file_handler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_request_handler.cpp
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
