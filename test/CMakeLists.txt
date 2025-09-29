project(beauty_test)

add_executable(${PROJECT_NAME}
	server_test.cpp
	file_io_test.cpp
	request_parser_test.cpp
	multipart_parser_test.cpp
	request_decoder_test.cpp
	url_parser_test.cpp
	http_result_test.cpp
	router_test.cpp
	base64_test.cpp
	${CMAKE_SOURCE_DIR}/examples/pc/file_io.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_file_io.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/utils/mock_request_handler.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE BEAUTY_ENABLE_TESTING)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain asio::asio cjson ${CMAKE_PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/examples/pc
)

# define tests
add_test(
	NAME ${PROJECT_NAME} 
	COMMAND $<TARGET_FILE:${PROJECT_NAME}> --success
)
