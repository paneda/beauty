project(asio_web)
add_executable(asio_web
	main.cpp
	server.cpp
	request_parser.cpp
	multipart_parser.cpp
	request_decoder.cpp
	request_handler.cpp
	reply.cpp
	mime_types.cpp
	connection_manager.cpp
	connection.cpp
	file_handler.cpp
	file_storage_handler.cpp
)
target_link_libraries(asio_web PRIVATE asio::asio)