cmake_minimum_required (VERSION 3.6)  # below 3.19 no COMMAND_ERROR_IS_FATAL
cmake_policy(VERSION 3.6)

if(ONION_BUILD)
	## 1. Check if onion source code is already downloaded and patched
	if( "${CMAKE_CURRENT_SOURCE_DIR}/onion-mpv-webui.bundle" IS_NEWER_THAN "${ONION_DIR}/.onion_patched" )

		# Fetch source code of libonion (during configuration time)
		execute_process(
			COMMAND git clone --depth=1 "https://github.com/davidmoreno/onion.git"
			"${ONION_DIR}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)

		# Patch source of libonion (during configuration time)
		execute_process(
			COMMAND make apply_bundle
			WORKING_DIRECTORY "."
			RESULT_VARIABLE ONION_PATCHING_RESULT
			OUTPUT_VARIABLE ONION_PATCHING_OUTPUT
			ERROR_VARIABLE ONION_PATCHING_OUTPUT
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		)

		message("onion patching: ${ONION_PATCHING_OUTPUT}")
		if(ONION_PATCHING_RESULT)
			message(FATAL_ERROR "Patching of onion lib failed: ${ONION_PATCHING_RESULT}")
		endif()

		execute_process(
			COMMAND cmake -E touch .onion_patched
			WORKING_DIRECTORY "${ONION_DIR}"
		)

		# FILE(GLOB ONION_PATCH_FILES "${CMAKE_SOURCE_DIR}/3rdparty/*.patch")
		# (Note: files seperated by ';')

		message("libonion repo cloned in '${ONION_DIR}' and patched.")
	else()
		message("libonion repo in '${ONION_DIR}' already patched.")
	endif()


	## 2. Setup (minimal) onion lib
	execute_process(
		COMMAND mkdir -p "${ONION_BUILD_DIR}"
	)

	#	-DONION_USE_SSL=TRUE # Needed for websocket support
	execute_process(
		COMMAND cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
		-DONION_USE_SSL=TRUE
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
		-DCMAKE_C_FLAGS="-I\ ${PROJECT_SOURCE_DIR}/src/include -D_FILE_OFFSET_BITS=64"
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5
		${TERMUX_EXTRA_OPTIONS}
		${ONION_DIR}
		WORKING_DIRECTORY "${ONION_BUILD_DIR}"
	)

	message("libonion repo configured in '${ONION_BUILD_DIR}'.")

	## 3. Crosscompiling: Build onion tools on native plattform, too.
	if(NATIVE_TOOLS)

		message("Build native version for tools.")
		execute_process(
			COMMAND mkdir -p "${ONION_BUILD_DIR}/../native"
		)
		execute_process(
			COMMAND cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DCMAKE_C_COMPILER=cc
			-DCMAKE_CXX_COMPILER=c++
			-DONION_USE_SSL=false
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
			-DCMAKE_C_FLAGS=-I\ ${PROJECT_SOURCE_DIR}/src/include
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5
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

endif(ONION_BUILD)
