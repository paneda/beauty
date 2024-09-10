project(beauty_test)

add_executable(beauty_test
	server_test.cpp
	file_io_test.cpp
	request_parser_test.cpp
	multipart_parser_test.cpp
	request_decoder_test.cpp
	url_parser_test.cpp
	${CMAKE_SOURCE_DIR}/examples/pc/file_io.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_file_io.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_request_handler.cpp
	${Beauty_Sources}
)

target_link_libraries(beauty_test PRIVATE Catch2::Catch2WithMain asio::asio)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/examples/pc
)

# define tests
add_test(
	NAME beauty_test
	COMMAND $<TARGET_FILE:beauty_test> --success
)
