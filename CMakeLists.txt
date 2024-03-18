cmake_minimum_required(VERSION 3.24)
project(asio_web
  LANGUAGES CXX
  VERSION 1.0
)

enable_testing()

add_subdirectory(import)
add_subdirectory(src)
add_subdirectory(test)

set_target_properties(asio_web asio_web_test PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)
