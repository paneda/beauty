if(NOT IDF_PATH)
	cmake_minimum_required(VERSION 3.24)
	project(beauty
		LANGUAGES CXX
		VERSION 1.0
	)

	enable_testing()

	file (GLOB Beauty_Sources CONFIGURE_DEPENDS "src/*.cpp")

	add_subdirectory(import)
	add_subdirectory(examples)
	add_subdirectory(test)

	# Note: Beauty_Sources requires C++11 only however examples/test
	# requires c++17
	set_target_properties(beauty_example beauty_test PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
	)
endif()

# --- ESP-IDF component support: only active when included as a component ---
if(IDF_PATH)
	file (GLOB Beauty_Sources "src/*.cpp")
	# Register as ESP-IDF component
	idf_component_register(
		SRCS ${Beauty_Sources}
		INCLUDE_DIRS "src"
		REQUIRES
	)
endif()
