ROOT_DIR=$(shell realpath .)
MPV_CONFIG_DIR ?= ${HOME}/.config/mpv
PREFIX ?= $(MPV_CONFIG_DIR)/onion-mpv-webui
BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release

MPV_SCRIPT_DIR=$(MPV_CONFIG_DIR)/script-opts
APK_DIR=/dev/shm/

help:
	@echo -e " Type\n\t\
	• 'make all'       for all installation steps\n\t\
	• 'make configure' to (re-)configure with cmake\n\t\
	• 'make build'     to compile \n\t\
	• 'make install'   to copy plugin into mpv's config dir\n\t\
	• 'make config'    to add plugin settings into mpv's config dir\n\t\
\n\t\
	• 'make config_develop'  Directly use compiled plugin from\n\t\
	                   'build/src' and web stuff from './webui-page'\n\t\
\n\t\
	• 'make run'       for test run of mpv with plugin\n\t\
	• 'make bin'       testing standalone binary (webui without mpv)\n\t\
	"

all: build install
	@echo "Done"
	@echo "Call mpv with '--profile=webui' to enable webinterface"

configure:
	test -e "${BUILD_DIR}" || mkdir "${BUILD_DIR}"
	cd "${BUILD_DIR}" && cmake -DONION_BUILD=TRUE -DONION_USE_SSL=TRUE \
		-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
		-DCMAKE_INSTALL_PREFIX="${PREFIX}" \
		..

# Required for debug messages like ONION_DEBUG(..)
configure_debug:
	test -e "${BUILD_DIR}" || mkdir "${BUILD_DIR}"
	cd "${BUILD_DIR}" && cmake -DONION_BUILD=TRUE -DONION_USE_SSL=TRUE \
		-DCMAKE_BUILD_TYPE="Debug" \
		-DCMAKE_INSTALL_PREFIX="${PREFIX}" \
		..

${BUILD_DIR}/CMakeCache.txt:
	make configure

build: ${BUILD_DIR}/CMakeCache.txt
	cd "${BUILD_DIR}" && make

install:
	cd "${BUILD_DIR}" && make install
	make config

uninstall:
	@echo "This will delete '${PREFIX}'. Continue? y/N"
	read UNINSTALL \
		&& test "${UNINSTALL}" = "y" -o "${UNINSTALL}" = "Y" \
		&& rm -r "${PREFIX}"

script_opts_folder:
	@test -e "$(MPV_SCRIPT_DIR)" \
		|| mkdir -p "$(MPV_SCRIPT_DIR)"

# Create config file(s) from templates
.config_defaults: script_opts_folder
	@test -e "$(MPV_SCRIPT_DIR)/libwebui.conf" \
		|| SCRIPT_DIR="$(PREFIX)/lib/" HTTP_DIR="$(PREFIX)/share/" cat libwebui.conf.example | envsubst \
		> "$(MPV_SCRIPT_DIR)/libwebui.conf"
	@grep 'libwebui.conf' "$(MPV_CONFIG_DIR)/mpv.conf" >/dev/null 2>/dev/null \
		|| $(shell which echo) -e '# Webui\ninclude="~~/script-opts/libwebui.conf"' \
		>> "$(MPV_CONFIG_DIR)/mpv.conf"

# Setup mpv config files and link to target of 'make install'
config: .config_defaults
	@echo "Linking path in config to '$(PREFIX)'"
	@sed -i 's!\(script="\).*\(libwebui.so"\)!\1$(PREFIX)/lib/\2!' "$(MPV_SCRIPT_DIR)/libwebui.conf"
	@sed -i 's!\(script-opts-add=libwebui-root_dir="\).*\(webui-page"\)!\1$(PREFIX)/share/\2!' "$(MPV_SCRIPT_DIR)/libwebui.conf"

# Setup mpv config files and link to files of this repo
config_develop: .config_defaults
	@echo "Linking path in config to '$(ROOT_DIR)'"
	@sed -i 's!\(script="\).*\(libwebui.so"\)!\1$(ROOT_DIR)/$(BUILD_DIR)/src/\2!' "$(MPV_SCRIPT_DIR)/libwebui.conf"
	@sed -i 's!\(script-opts-add=libwebui-root_dir="\).*\(webui-page"\)!\1$(ROOT_DIR)/\2!' "$(MPV_SCRIPT_DIR)/libwebui.conf"

"$(MPV_SCRIPT_DIR)/libwebui.conf": .config_defaults

package:
	BUILD_DIR=build_package PREFIX=$(ROOT_DIR)/package make configure
	BUILD_DIR=build_package make build
	BUILD_DIR=build_package make install

# Assumes profile=webui in mpv.conf
run: "$(MPV_SCRIPT_DIR)/libwebui.conf"
	ONION_LOG=nodebug,noinfo \
		mpv --quiet --pause "$$(xdg-user-dir MUSIC)"

# Run 'make configure_debug' before
#	ONION_DEBUG=127 ONION_DEBUG0='onion_ws_status.c onion_default_errors.c media.c'
run_debug: "$(MPV_SCRIPT_DIR)/libwebui.conf"
	ONION_DEBUG=1 \
	mpv --quiet --pause \
		$$(grep "\S*profile=webui\S*" "$(MPV_CONFIG_DIR)/mpv.conf" >/dev/null 2>&1 \
			|| echo '--profile=webui') \
		"$$(xdg-user-dir MUSIC)"


bin:
	./$(BUILD_DIR)/src/onion-webui.bin ./webui-page

nemiver:
	nemiver ./$(BUILD_DIR)/src/onion-webui.bin ./webui-page/

val: config
	valgrind --leak-check=full --log-file=/dev/shm/val.log \
		mpv --profile=webui --quiet\
		"$${HOME}/Music"

valB: config
	valgrind --leak-check=full \
		mpv --profile=webui --quiet\
		"$${HOME}/Music"

# JS watchdog to update bundle file
js:
	cd 3rdparty/watchdog && make run
	
jsmin:
	cd 3rdparty/watchdog && make min

jsmin1:
	3rdparty/minimizer/terser.sh webui-page/*.js \
		--compress \
		> webui-page/static/js/webui.min.js

# Still ok
jsmin2:
	3rdparty/minimizer/terser.sh webui-page/*.js \
		--compress --mangle reserved=['$$','require','exports'] \
		--mangle-props regex=/^_/ \
		> webui-page/static/js/webui.min.js

# This wont work because it renames too much and I'm using hasOwnProperty...
jsmin3:
	3rdparty/minimizer/terser.sh webui-page/*.js \
		--compress --mangle reserved=['$$','require','exports'] \
		--mangle-props \
		> webui-page/static/js/webui.min.js

cssmin:
	3rdparty/minimizer/uglifycss.sh \
		--max-line-len 400 \
		webui-page/*.css \
		| sed -e "s/url('/url('..\/..\//g" -- \
		> webui-page/static/css/webui.min.css


# Integrate plugin into mpv-android package
# (Integrate this plugin into mpv on android without NDK)
# Assumption: Proper libwebui.so (Termux build) is in $(APK_DIR) stored.
# Read this stuff as sketch
apk_setup:
	@echo "Install required packages for keytool, etc"
	sudo apt install openjdk-14-jre-headless zipalign apksigner
	@echo "Generate debug key with default password 'android'"
	keytool -genkey -v -keystore "$(ROOT_DIR)/debug.keystore" -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000

apk_fetch_basis:
	cd "$(APK_DIR)" && wget -c -O "mpv-android.basis.apk" "https://github.com/mpv-android/mpv-android/releases/download/mpv-android-2020-11-16/mpv-android-2020-11-16.apk"

apk_unzip_basis:
	cd "$(APK_DIR)" && mkdir mpv-android
	cd "$(APK_DIR)/mpv-android" && unzip ../mpv-android.basis.apk
	rm -r "$(APK_DIR)/mpv-android/META-INF/"

apk_patching:
	cp -r "webui-page" "$(APK_DIR)/mpv-android/assets/."
	chmod 664 "$(APK_DIR)/libwebui.so"
	cp "$(APK_DIR)/simple.so" "$(APK_DIR)/mpv-android/lib/arm64-v8a/."

apk_build:
	rm -f "$(APK_DIR)/mpv-android-with-plugin.apk"
	cd "$(APK_DIR)/mpv-android" && zip -r ../mpv-android-with-plugin.not_aligned.apk *
	cd "$(APK_DIR)" && zipalign -v 4 mpv-android-with-plugin.not_aligned.apk mpv-android-with-plugin.apk \
		&& apksigner sign --ks "$(ROOT_DIR)/debug.keystore" --ks-pass "pass:android" \
		--min-sdk-version 21 \
		mpv-android-with-plugin.apk
	rm -f "$(APK_DIR)/mpv-android-with-plugin.not_aligned.apk"

# TV
apk_copy_config:
	sudo adb shell "test -e /sdcard/mpv || mkdir /sdcard/mpv"
	sudo adb push webui.androidtv.conf sdcard/mpv/webui.conf
	echo "Insert 'include="/sdcard/mpv/webui.conf"' into the mpv.conf file" \
		"on the target system."

# Smartphone
apk_copy_config2:
	sudo adb shell "test -e /sdcard/mpv || mkdir /sdcard/mpv"
	sudo adb push webui.androidtv2.conf sdcard/mpv/webui.conf
	echo "Insert 'include="/sdcard/mpv/webui.conf"' into the mpv.conf file" \
		"on the target system."

apk_copy_page:
	adb push webui-page sdcard/mpv/

mpv:
	vim -p \
		$$HOME/.config/mpv/script-opts/libwebui.conf \
		$$HOME/.config/mpv/mpv.conf

.PHONY: build
