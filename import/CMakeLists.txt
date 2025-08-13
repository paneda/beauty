include(CPM.cmake)

CPMAddPackage("https://github.com/catchorg/Catch2.git#v3.5.2")

set(ASIO_TAG asio-1-29-0)
CPMAddPackage(
	NAME asiocmake
	GIT_REPOSITORY "https://github.com/OlivierLDff/asio.cmake"
	GIT_TAG "main"
)

# https://github.com/DaveGamble/cJSON/pull/935 
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
CPMAddPackage(
  NAME json
  GIT_REPOSITORY "https://github.com/DaveGamble/cJSON"
  GIT_TAG "v1.7.18"
  OPTIONS
	"ENABLE_CJSON_TEST OFF"
	"BUILD_SHARED_LIBS OFF"
)

# cJSON doesn't set include directories automatically, so we need to add them
# https://github.com/DaveGamble/cJSON/issues/816
if(json_ADDED)
  target_include_directories(cjson INTERFACE 
    $<BUILD_INTERFACE:${json_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
endif()
