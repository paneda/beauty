if(NOT IDF_PATH)
	cmake_minimum_required(VERSION 3.24)
	project(beauty_library
		LANGUAGES CXX
		VERSION 1.0
	)

	enable_testing()

	file (GLOB beauty_sources CONFIGURE_DEPENDS "src/*.cpp")

	add_subdirectory(import)
	add_subdirectory(examples)
	add_subdirectory(test)

	add_library(${CMAKE_PROJECT_NAME} STATIC ${beauty_sources})

	target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE asio::asio)

	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
		CXX_STANDARD 11
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
	)
endif()

# --- ESP-IDF component support: only active when included as a component ---
if(IDF_PATH)
	file (GLOB beauty_sources "src/*.cpp")
	# Register as ESP-IDF component
	idf_component_register(
		SRCS ${beauty_sources}
		INCLUDE_DIRS "src"
		REQUIRES asio espressif__sock_utils 
	)
endif()
