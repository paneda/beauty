project(asio_web)
add_executable(asio_web
	main.cpp server.cpp
	request_parser.cpp
	request_handler.cpp
	reply.cpp
	mime_types.cpp
	connection_manager.cpp
	connection.cpp)
target_link_libraries(asio_web PRIVATE asio::asio)