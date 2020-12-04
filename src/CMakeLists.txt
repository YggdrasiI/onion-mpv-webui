SET(ONION_DIR "${PROJECT_SOURCE_DIR}/3rdparty/onion" CACHE STRING "Git folder of onion lib")
SET(ONION_BUILD_DIR "${ONION_DIR}/build" CACHE STRING "build dir of onion lib")
SET(OTEMPLATE "${ONION_BUILD_DIR}/tools/otemplate/otemplate" CACHE STRING "binary compiled by onion lib") 
# TODO: Set path for installed 'libonion package'-case.
SET(OTEMPLATE_TAGSDIR "${ONION_BUILD_DIR}/tools/otemplate/templatetags" CACHE STRING "Path to libi18n.so")

MACRO (compile_template _name)
	string(REPLACE "." "_" _name2 "${_name}")
	add_custom_command(
		OUTPUT ${_name2}.c
		COMMAND ${OTEMPLATE} -t "${OTEMPLATE_TAGSDIR}" "${CMAKE_SOURCE_DIR}/templates/${_name}"
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
	)
if (${TERMUX})
SET(SOURCE_FILES ${SOURCE_FILES}
	"include/wordexp.c")
endif (${TERMUX})

# Plugin for mpv
add_library(webui SHARED mpv_plugin.c mpv_api_commands.c ${SOURCE_FILES} )
target_link_libraries(webui onion_static m)
target_compile_options(webui PUBLIC -shared -fPIC)
target_compile_options(webui PUBLIC -DWITH_MPV=1)
target_compile_options(webui PUBLIC -DMPV_PLUGIN_NAME=libwebui)  # mpv option prefix

# Test executable without mpv dependecies.
add_executable(onion-webui.bin standalone.c  ${SOURCE_FILES})
target_link_libraries(onion-webui.bin onion_static m)
target_compile_options(onion-webui.bin PUBLIC -fPIC)

# Android dlopen test
add_library(simple SHARED simple.c)
target_include_directories(simple PUBLIC "include") 
target_compile_options(simple PUBLIC -shared -fPIC)


#############################################################################
if (${TERMUX})
	target_include_directories(webui PUBLIC "include") 
	target_include_directories(onion-webui.bin PUBLIC "include")

	target_link_libraries(webui log)
	target_link_libraries(onion-webui.bin log)
	target_link_libraries(simple log)
	# In this case, no linking on pthread.
	# Thread functions are buildin in Bionic libc
else (${TERMUX})
	target_link_libraries(webui pthread )
	target_link_libraries(onion-webui.bin pthread )
endif (${TERMUX})


#############################################################################
if (${ZLIB_FOUND})
   target_link_libraries(onion-webui.bin ${ZLIB_LIBRARIES})
endif(${ZLIB_FOUND})

# This libs are required for websockets support
if (${TERMUX})
	# Here, we linking the static libs because we want
	# share the file into a non-Termux environment later.
	# Grouping is required to resolve interleaving dependencies.
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
else (${TERMUX})
	SET(WS_LIBS gnutls gcrypt)
endif (${TERMUX})
target_link_libraries(webui ${WS_LIBS})
target_link_libraries(onion-webui.bin ${WS_LIBS})


if(${BUILD_ONION})
	# Build, setup and compile minimal onion lib
	add_custom_command(
		OUTPUT .onion_build
		# COMMAND test -d "${ONION_DIR}" || git clone --depth=1 https://github.com/davidmoreno/onion.git "${ONION_DIR}"
		COMMAND test -d "${ONION_DIR}" || 
		git clone --depth=1 --branch=unstable https://github.com/YggdrasiI/onion.git "${ONION_DIR}"
		COMMAND test -e "${ONION_DIR}/.onion_patched" || ( cd "${ONION_DIR}" && 
		git am ../0001-Termux-pthread-fix.patch &&
		cmake -E touch .onion_patched )
		COMMAND test -d "${ONION_DIR}/build" || mkdir "${ONION_DIR}/build"
		COMMAND cd "${ONION_DIR}/build" && cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
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
		..
		COMMAND cd "${ONION_DIR}/build" && make
		COMMAND cmake -E touch .onion_build 
		)

	add_custom_target(compile_onion ALL DEPENDS .onion_build )

	# Set flag for slightly different header file path
	target_compile_options(webui PUBLIC -DBUILD_ONION=1)
	target_compile_options(onion-webui.bin PUBLIC -DBUILD_ONION=1)

	add_dependencies(webui compile_onion)
	add_dependencies(onion-webui.bin compile_onion)
else(${BUILD_ONION})
endif(${BUILD_ONION})


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
	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/locale" DESTINATION ${INSTALL_PATH})
endif(HAVE_MSGFMT)


#############################################################################
set_target_properties(onion-webui.bin PROPERTIES OUTPUT_NAME "onion-webui.bin")
install(TARGETS onion-webui.bin	DESTINATION ${INSTALL_PATH})
install(TARGETS webui DESTINATION ${INSTALL_PATH})
install(DIRECTORY ../webui-page DESTINATION ${INSTALL_PATH})


#############################################################################
# add_subdirectory(tests)
