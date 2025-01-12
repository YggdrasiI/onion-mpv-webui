cmake_minimum_required (VERSION 3.6)
cmake_policy(VERSION 3.6)
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
SET(ONION_USE_SSL FALSE CACHE BOOL "Compile with SSL support (libgcrypt libgnutls)")
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

if (${ONION_USE_SSL})
	# This requires -dev Packages, but we're just wanna know if libs are installed.
	#	find_package(GnuTLS)
	#	find_package(GCrypt)
	#	if (GNUTLS_FOUND AND GCRYPT_FOUND)
	#		set(GNUTLS_ENABLED true)
	#		message(STATUS "SSL support is compiled in.")
	#	else()
	#		message("Gnutls or gcrypt not found. SSL support is not compiled in.")
	#	endif()

	# Search *.so files. (Currently not needed but it synchronized the linking between libwebui and libonion.)
	find_library(GNUTLS_LIBRARIES NAMES gnutls libgnutls)  # '-IES' ending to get same var name as Onion's Cmake.
	find_library(GCRYPT_LIBRARIES NAMES gcrypt libgcrypt)
	if ( "${GNUTLS_LIBRARIES}" STREQUAL "GNUTLS_LIBRARIES-NOTFOUND" OR "${GCRYPT_LIBRARIES}" STREQUAL "GNUTLS_LIBRARIES-NOTFOUND")
		message(WARN "ONION_USE_SSL is set but gnutls or gcrypt not found.")
	else()
		set(GNUTLS_ENABLED true)
	endif()
	message(STATUS ${GNUTLS_LIBRARIES})
endif(${ONION_USE_SSL})


# For localization (not needed ATM)
find_program(HAVE_MSGFMT msgfmt)


add_subdirectory(3rdparty)
add_subdirectory(src)
