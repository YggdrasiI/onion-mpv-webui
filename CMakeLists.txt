cmake_minimum_required (VERSION 2.8)
cmake_policy(VERSION 2.8)
project(onion-mpv-webui)

SET(CMAKE_C_STANDARD 11)

# SET(INSTALL_PATH "${HOME}.config/mpv/scripts" CACHE STRING "Install path of plugin")
# SET(INSTALL_PATH "${PROJECT_SOURCE_DIR}/mpv/scripts" CACHE STRING "Install path of plugin")
# SET(INSTALL_PATH "/dev/shm/mpv/scripts" CACHE STRING "Install path of plugin")
SET(ONION_BUILD TRUE CACHE BOOL
	"Do not search installed onion lib but build in 3rdparty/onion.")

SET(ONION_DIR "${PROJECT_SOURCE_DIR}/3rdparty/onion" CACHE STRING "Git folder of onion lib")
# SET(ONION_BUILD_DIR "${ONION_DIR}/build" CACHE STRING "build dir of onion lib")
SET(ONION_BUILD_DIR "${CMAKE_BINARY_DIR}/3rdparty/onion" CACHE STRING "build dir of onion lib")
SET(NATIVE_TOOLS FALSE CACHE BOOL "Compile tools (otemplate, etc) native")
SET(OTEMPLATE_TAGSDIR "${ONION_BUILD_DIR}/tools/otemplate/templatetags" CACHE STRING "Path to libi18n.so")


# == Serach dependencies ==
if(NOT ONION_BUILD)
	find_path(ONION_INCLUDE_DIR "onion/exportlocal.h")
	find_library(ONION_STATIC_LIB_DIR onion.a)
	find_program(OTEMPLATE otemplate)

	message("Found ONION_INCLUDE_DIR path: ${ONION_INCLUDE_DIR}")
	message("Found ONION_STATIC_LIB_DIR path: ${ONION_STATIC_LIB_DIR}")
	message("Found OTEMPLATE path: ${OTEMPLATE}")
else()
	SET(OTEMPLATE "${ONION_BUILD_DIR}/tools/otemplate/otemplate" CACHE STRING "binary compiled by onion lib")
endif()

find_package(ZLIB)

# For localization (not needed ATM)
find_program(HAVE_MSGFMT msgfmt)


add_subdirectory(src)
