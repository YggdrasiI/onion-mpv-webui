SET(ONION_DIR "${PROJECT_SOURCE_DIR}/3rdparty/onion" CACHE STRING "Git folder of onion lib")
# SET(ONION_BUILD_DIR "${ONION_DIR}/build" CACHE STRING "build dir of onion lib")
SET(ONION_BUILD_DIR "${CMAKE_BINARY_DIR}/3rdparty/onion" CACHE STRING "build dir of onion lib")
SET(OTEMPLATE "${ONION_BUILD_DIR}/tools/otemplate/otemplate" CACHE STRING "binary compiled by onion lib")
SET(NATIVE_TOOLS false CACHE BOOL "Compile tools (otemplate, etc) native")
# TODO: Set path for installed 'libonion package'-case.
SET(OTEMPLATE_TAGSDIR "${ONION_BUILD_DIR}/tools/otemplate/templatetags" CACHE STRING "Path to libi18n.so")

MACRO (compile_template _name)
	STRING(REPLACE "." "_" _name2 "${_name}")
	add_custom_command(
		OUTPUT ${_name2}.c
		COMMAND ${OTEMPLATE} --no-libintl -t "${OTEMPLATE_TAGSDIR}" "${CMAKE_SOURCE_DIR}/templates/${_name}"
		"${CMAKE_CURRENT_BINARY_DIR}/${_name2}.c"
		DEPENDS "${CMAKE_SOURCE_DIR}/templates/${_name}"
		)
ENDMACRO()

MACRO (compile_localization _lang)
	add_custom_target(
		fo_${_lang} ALL
		COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/locale/LC_MESSAGES/
		COMMAND msgfmt  ${CMAKE_SOURCE_DIR}/po/${_lang}.po
		-o ${CMAKE_CURRENT_BINARY_DIR}/locale/LC_MESSAGES/${_lang}.mo
		DEPENDS ${CMAKE_SOURCE_DIR}/po/${_lang}.po
		)
ENDMACRO()


if(${BUILD_ONION})
	include_directories ("${ONION_DIR}/src/")
	link_directories("${ONION_BUILD_DIR}/src/onion")
else(${BUILD_ONION})
	# Use variables defined by find_* commands of target CMakeLists.txt
	include_directories ("${ONION_INCLUDE_DIR}")
	link_directories("${ONION_STATIC_LIB}")
endif(${BUILD_ONION})


# For Android/Termux
# message("CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
if(NOT DEFINED TERMUX)  # Skips autodetection if manually set
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")  # True in Termux
		message("Set TERMUX flag")
		SET(TERMUX TRUE CACHE BOOL "Termux flag")
	else("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
		SET(TERMUX FALSE CACHE BOOL "Termux flag")
	endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
endif(NOT DEFINED TERMUX)

if (${TERMUX})
	SET(TERMUX_EXTRA_OPTIONS "-DTERMUX=true" CACHE STRING "")
else (${TERMUX})
	SET(TERMUX_EXTRA_OPTIONS "" CACHE STRING "")
endif (${TERMUX})


#############################################################################
SET(SOURCE_FILES
	webui_onion.c
	fileserver_html.c
	defines.c
	buffer_pool.c
	fileserver.c
	media.c
	media_html.c
	media_json.c
	mpv_script_options.c
	tools.c
	onion_ws_status.c
	onion_ws_time.c
	utf8.c
	)
if (${TERMUX})
SET(SOURCE_FILES ${SOURCE_FILES}
	"include/wordexp.c")
endif (${TERMUX})

# Plugin for mpv
add_library(webui SHARED mpv_plugin.c mpv_api_commands.c ${SOURCE_FILES} )
target_include_directories(webui PUBLIC "include")
target_link_libraries(webui onion_static m)
target_compile_options(webui PUBLIC -shared -fPIC)
target_compile_options(webui PUBLIC -DWITH_MPV=1)
target_compile_options(webui PUBLIC -DMPV_PLUGIN_NAME=libwebui)  # mpv option prefix

## Test executable without mpv dependecies.
#add_executable(onion-webui.bin standalone.c ${SOURCE_FILES})
#include_directories ("include")
#target_link_libraries(onion-webui.bin onion_static m)
#target_compile_options(onion-webui.bin PUBLIC -fPIC)

# Android dlopen test
add_library(simple SHARED simple.c)
target_include_directories(simple PUBLIC "include")
target_compile_options(simple PUBLIC -shared -fPIC)


#############################################################################
if (${TERMUX})
	target_include_directories(webui PUBLIC "include")
	# target_include_directories(onion-webui.bin PUBLIC "include")

	target_link_libraries(webui log)
	# target_link_libraries(onion-webui.bin log)
	target_link_libraries(simple log)
	# In this case, no linking on pthread.
	# Thread functions are buildin in Bionic libc
else (${TERMUX})
	target_link_libraries(webui pthread )
	# target_link_libraries(onion-webui.bin pthread )
endif (${TERMUX})


#############################################################################
if (${ZLIB_FOUND})
	# target_link_libraries(onion-webui.bin ${ZLIB_LIBRARIES})
endif(${ZLIB_FOUND})

# This libs are required for websockets support
if (${TERMUX})
	# Here, we linking the static libs because we want
	# share the file into a non-Termux environment later.
	# Grouping is required to resolve interleaving dependencies.

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
	SET(WS_LIBS
		-Wl,--start-group
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgcrypt.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgnutls.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgnutlsxx.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgpg-error.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libhogweed.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libnettle.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgmp.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libgmpxx.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libiconv.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libunistring.a
		${CMAKE_SOURCE_DIR}/3rdparty/static_libs/libidn2.a
		-Wl,--end-group
		)

	# This doesn't work due(?) security of androids library loading
	# target_link_libraries(webui "/data/app/is.xyz.mpv-xmZJB3QOZUmylqAcpwBIcA==/lib/arm64/libmpv.so")
else("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
	SET(WS_LIBS
		-Wl,--start-group
		/data/data/com.termux/files/usr/lib/libgcrypt.a
		/data/data/com.termux/files/usr/lib/libgnutls.a
		/data/data/com.termux/files/usr/lib/libgnutlsxx.a
		/data/data/com.termux/files/usr/lib/libgpg-error.a
		/data/data/com.termux/files/usr/lib/libhogweed.a
		/data/data/com.termux/files/usr/lib/libnettle.a
		/data/data/com.termux/files/usr/lib/libgmp.a
		/data/data/com.termux/files/usr/lib/libgmpxx.a
		/data/data/com.termux/files/usr/lib/libiconv.a
		/data/data/com.termux/files/usr/lib/libunistring.a
		/data/data/com.termux/files/usr/lib/libidn2.a
		-Wl,--end-group
		)
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")

else (${TERMUX})
	SET(WS_LIBS gnutls gcrypt)
endif (${TERMUX})
target_link_libraries(webui ${WS_LIBS})
# target_link_libraries(onion-webui.bin ${WS_LIBS})


if(${BUILD_ONION})

	# Fetch source
	execute_process(
		COMMAND git clone --depth=1 "https://github.com/davidmoreno/onion.git"
		"${ONION_DIR}"
		)
	# git clone --depth=1 --branch=unstable "https://github.com/YggdrasiI/onion.git"

	# Patch source of libonion
	execute_process(
		COMMAND git checkout -b onion-mpv-webui
		COMMAND git pull ../onion-mpv-webui_0001-0008.bundle HEAD
		WORKING_DIRECTORY "${ONION_DIR}"
		)

	# FILE(GLOB ONION_PATCH_FILES "${CMAKE_SOURCE_DIR}/3rdparty/*.patch")
	# (Note: files seperated by ';')

	#foreach(ONION_PATCH_FILE ${ONION_PATCH_FILES})
	#	message("Patch file: \n ${ONION_PATCH_FILE}")
	#	execute_process(
	#		COMMAND git am ${ONION_PATCH_FILE}
	#		WORKING_DIRECTORY "${ONION_DIR}"
	#		)
	#endforeach()

	# Setup (minimal) onion lib
	execute_process(
		COMMAND mkdir -p "${ONION_BUILD_DIR}"
		)
	execute_process(
		COMMAND cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
		-DONION_USE_SSL=true
		-DONION_USE_PAM=false
		-DONION_USE_PTHREADS=true
		-DONION_USE_PNG=false
		-DONION_USE_JPEG=false
		-DONION_USE_XML2=false
		-DONION_USE_SYSTEMD=false
		-DONION_USE_SQLITE3=false
		-DONION_USE_REDIS=false
		-DONION_USE_GC=false
		-DONION_USE_TESTS=false
		-DONION_EXAMPLES=false
		-DONION_USE_BINDINGS_CPP=false
		-DONION_POLLER=epoll
		${TERMUX_EXTRA_OPTIONS}
		${ONION_DIR}
		WORKING_DIRECTORY "${ONION_BUILD_DIR}"
		)

		# Compile onion lib (at build time)
	add_custom_command(
		OUTPUT .onion_build
		COMMAND make
		COMMAND cmake -E touch .onion_build
		WORKING_DIRECTORY "${ONION_BUILD_DIR}"
		)

	add_custom_target(compile_onion ALL DEPENDS .onion_build )

	# Set flag for slightly different header file path
	target_compile_options(webui PUBLIC -DBUILD_ONION=1)
	# target_compile_options(onion-webui.bin PUBLIC -DBUILD_ONION=1)

	add_dependencies(webui compile_onion)
	# add_dependencies(onion-webui.bin compile_onion)


	if(NATIVE_TOOLS)

	message("Build native version for tools.")
	execute_process(
		COMMAND mkdir -p "${ONION_BUILD_DIR}/../native"
		)
	execute_process(
		COMMAND cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_C_COMPILER=cc
		-DCMAKE_CXX_COMPILER=c++
		-DONION_USE_SSL=true
		-DONION_USE_PAM=false
		-DONION_USE_PTHREADS=true
		-DONION_USE_PNG=false
		-DONION_USE_JPEG=false
		-DONION_USE_XML2=false
		-DONION_USE_SYSTEMD=false
		-DONION_USE_SQLITE3=false
		-DONION_USE_REDIS=false
		-DONION_USE_GC=false
		-DONION_USE_TESTS=false
		-DONION_EXAMPLES=false
		-DONION_USE_BINDINGS_CPP=false
		-DONION_POLLER=epoll
		${ONION_DIR}
		WORKING_DIRECTORY "${ONION_BUILD_DIR}/../native"
		)
	execute_process(
		COMMAND make
		WORKING_DIRECTORY "${ONION_BUILD_DIR}/../native"
		)

	message("END Build native version for tools.")
	SET(OTEMPLATE "${ONION_BUILD_DIR}/../native/tools/otemplate/otemplate" CACHE STRING "binary compiled by onion lib" FORCE)
	message("Update path of otemplate to native binary: '${OTEMPLATE}'")
	endif(NATIVE_TOOLS)

else(${BUILD_ONION})
endif(${BUILD_ONION})



include(GNUInstallDirs)
# message("data: ${CMAKE_INSTALL_DATADIR}")

configure_file("webui.pc.in" "webui.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/webui.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

#############################################################################
# Templates
compile_template(fileserver.html)
compile_template(media.html)
compile_template(media.json)

if(HAVE_MSGFMT)
	# Localization
	compile_localization(de)
	# compile_localization(es)
	# compile_localization(fr)
	# compile_localization(zh)
	# compile_localization(pl)
	#install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/locale" DESTINATION ${INSTALL_PATH})
	#	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/locale/*" DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/onion-mpv-webui")
endif(HAVE_MSGFMT)


#############################################################################
#set_target_properties(onion-webui.bin PROPERTIES OUTPUT_NAME "onion-webui.bin")
## set_target_properties(onion-webui.bin PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
## set_target_properties(webui PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

# install(TARGETS onion-webui.bin	DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS webui EXPORT webui DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ../webui-page DESTINATION "${CMAKE_INSTALL_DATADIR}")


#############################################################################
# add_subdirectory(tests)
