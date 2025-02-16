MACRO(compile_template _name _target)
	STRING(REPLACE "." "_" _name2 "${_name}")
	if(TERMUX)
		# Without translation (for Bionic build)
		SET(LIBINTL --no-libintl)
	else()
		# With translation by gettext
	endif()
	add_custom_command(
		OUTPUT ${_name2}.c
		COMMAND ${OTEMPLATE} ${LIBINTL} -t "${OTEMPLATE_TAGSDIR}" "${CMAKE_SOURCE_DIR}/templates/${_name}"
		"${CMAKE_CURRENT_BINARY_DIR}/${_name2}.c"
		DEPENDS "${CMAKE_SOURCE_DIR}/templates/${_name}"
	)

	# Add generated file to sources of binary
	target_sources("${_target}" PRIVATE "${_name2}.c")
ENDMACRO()

MACRO(compile_localization _lang)
	add_custom_target(
		fo_${_lang} ALL
		COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/locale/LC_MESSAGES/
		COMMAND msgfmt  ${CMAKE_SOURCE_DIR}/po/${_lang}.po
		-o ${CMAKE_CURRENT_BINARY_DIR}/locale/LC_MESSAGES/${_lang}.mo
		DEPENDS ${CMAKE_SOURCE_DIR}/po/${_lang}.po
	)
ENDMACRO()


if(ONION_BUILD)
	include_directories ("${ONION_DIR}/src/")
	link_directories("${ONION_BUILD_DIR}/src/onion")
else()
	# Use variables defined by find_* commands of target CMakeLists.txt
	include_directories ("${ONION_INCLUDE_DIR}")
	link_directories("${ONION_STATIC_LIB}")
endif()


# For Android/Termux
# message("CMAKE_SYSTEM_NAME: '${CMAKE_SYSTEM_NAME}'")
if(NOT DEFINED TERMUX)  # Skips autodetection if manually set
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")  # True in Termux
		message("Set TERMUX flag")
		SET(TERMUX TRUE CACHE BOOL "Termux flag")
	else()
		SET(TERMUX FALSE CACHE BOOL "Termux flag")
	endif()
endif()

if(TERMUX)
	SET(TERMUX_EXTRA_OPTIONS "-DTERMUX=true" CACHE STRING "")
else()
	SET(TERMUX_EXTRA_OPTIONS "" CACHE STRING "")
endif()


#############################################################################
SET(SOURCE_FILES
	webui_onion.c
	defines.c
	buffer_pool.c
	fileserver.c
	media.c
	mpv_script_options.c
  media_track_paths.c
	tools.c
	percent_encoding.c
	onion_ws_status.c
	onion_ws_time.c
	onion_default_errors.c
	utf8.c
)
if(TERMUX)
	SET(SOURCE_FILES ${SOURCE_FILES}
		"include/wordexp.c")
endif()

# Plugin for mpv
add_library(webui SHARED mpv_plugin.c mpv_api_commands.c ${SOURCE_FILES} )
target_include_directories(webui PUBLIC "include")
target_link_libraries(webui onion_static m)
# target_compile_options(webui PUBLIC -shared -fPIC)
target_compile_options(webui PUBLIC -fPIC)
target_compile_options(webui PUBLIC -DWITH_MPV=1)
target_compile_options(webui PUBLIC -DMPV_PLUGIN_NAME=libwebui)  # mpv option prefix

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(webui PUBLIC -D__DEBUG__)
endif()

## Test executable without mpv dependecies.
#add_executable(onion-webui.bin standalone.c ${SOURCE_FILES})
#include_directories ("include")
#target_link_libraries(onion-webui.bin onion_static m)
#target_compile_options(onion-webui.bin PUBLIC -fPIC)

# Android dlopen test
add_library(simple SHARED simple.c)
target_include_directories(simple PUBLIC "include")
# target_compile_options(simple PUBLIC -shared -fPIC)
target_compile_options(simple PUBLIC -fPIC)


#############################################################################
if(TERMUX)
	target_include_directories(webui PUBLIC "include")
	# target_include_directories(onion-webui.bin PUBLIC "include")

	target_link_libraries(webui log)
	# target_link_libraries(onion-webui.bin log)
	target_link_libraries(simple log)
	# In this case, no linking on pthread.
	# Thread functions are buildin in Bionic libc
else()
	target_link_libraries(webui pthread )
	# target_link_libraries(onion-webui.bin pthread )
endif()


#############################################################################
if(ZLIB_FOUND)
	# target_link_libraries(onion-webui.bin ${ZLIB_LIBRARIES})
endif()

# This libs are required for websockets support
if(TERMUX)
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
	else()
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
	endif()

else()
	if(${GNUTLS_ENABLED})
		#SET(WS_LIBS gnutls gcrypt)
		SET(WS_LIBS ${GCRYPT_LIBRARIES} ${GNUTLS_LIBRARIES})
	else()
		SET(WS_LIBS )
	endif()

endif()
target_link_libraries(webui ${WS_LIBS})
# target_link_libraries(onion-webui.bin ${WS_LIBS})


if(ONION_BUILD)
	add_custom_target(compile_onion ALL DEPENDS "${ONION_BUILD_DIR}/.onion_build" )

	# Set flag for slightly different header file path
	target_compile_options(webui PUBLIC -DONION_BUILD=1)
	# target_compile_options(onion-webui.bin PUBLIC -DONION_BUILD=1)

	add_dependencies(webui compile_onion)
	# add_dependencies(onion-webui.bin compile_onion)
endif()


include(GNUInstallDirs)
# message("data: ${CMAKE_INSTALL_DATADIR}")

configure_file("webui.pc.in" "webui.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/webui.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

#############################################################################
# Templates
compile_template(style.include webui)
compile_template(js.include webui)
compile_template(footer.include webui)
compile_template(controls.include webui)
compile_template(fileserver.html webui)
compile_template(media.html webui)
compile_template(media_api_list.json webui)
compile_template(media_share.html webui)

if(HAVE_MSGFMT)
	# Localization
	#compile_localization(de)
	# compile_localization(es)
	# compile_localization(fr)
	# compile_localization(zh)
	# compile_localization(pl)
	#install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/locale" DESTINATION ${INSTALL_PATH})
	#	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/locale/*" DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/onion-mpv-webui")
endif()


#############################################################################
#set_target_properties(onion-webui.bin PROPERTIES OUTPUT_NAME "onion-webui.bin")
## set_target_properties(onion-webui.bin PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
## set_target_properties(webui PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

# install(TARGETS onion-webui.bin DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS webui EXPORT webui DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY ../webui-page DESTINATION "${CMAKE_INSTALL_DATADIR}")


#############################################################################
# add_subdirectory(tests)
