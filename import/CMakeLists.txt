include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# https://github.com/OlivierLDff/asio.cmake
set(ASIO_TAG asio-1-29-0)
option(ASIO_USE_CPM "Download Asio with CPM instead of FetchContent" OFF)
option(
	ASIO_CPM_FETCHCONTENT_COMPAT
	"Should asio be declared with FetchContent functions to be compatible. This doesn't not allow CPM cache to work."
	ON
)
# Download this repository
FetchContent_Declare(
	asiocmake
	GIT_REPOSITORY "https://github.com/OlivierLDff/asio.cmake"
	GIT_TAG        "main"
)
FetchContent_MakeAvailable(asiocmake)
