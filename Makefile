ROOT_DIR=$(shell realpath .)
MPV_CONFIG_DIR="${HOME}/.config/mpv/script-opts"

APK_DIR=/dev/shm/

main: build
	cd build && make

build:
	test -e build || mkdir build
	cd build && cmake -DBUILD_ONION=TRUE ..

install: main
	cd build && make install

script_opts_folder:
	@test -e "$(MPV_CONFIG_DIR)" \
		|| mkdir -p "$(MPV_CONFIG_DIR)"

config: script_opts_folder
	@test -e "$(MPV_CONFIG_DIR)/libwebui.conf" \
		|| cp libwebui.conf.example "${HOME}/.config/mpv/script-opts/libwebui.conf"
	@grep 'libwebui.conf' "${HOME}/.config/mpv/mpv.conf" \
		|| $(shell which echo) -e '# Webui\ninclude="~~/script-opts/libwebui.conf"' \
		>> "${HOME}/.config/mpv/mpv.conf"
	@echo "Call mpv with '--profile=webui' to enable webinterface"

# Config über script-opts/libwebui.conf
run: config
	ONION_DEBUG=0 ONION_DEBUG0=onion_ws_status.c \
	mpv --profile=webui --quiet\
		"$${HOME}/Musik/jeder_tag.ogg" "$${HOME}/Musik/Die Planeten/"

bin:
	build/src/onion-webui.bin ./webui-page

nemiver:
	nemiver ./build/src/onion-webui.bin ./webui-page/

val: config
	valgrind --leak-check=full --log-file=/dev/shm/val.log \
		mpv --profile=webui --quiet\
		"$${HOME}/Musik/jeder_tag.ogg" "$${HOME}/Musik/Die Planeten/"

valB: config
	valgrind --leak-check=full \
		mpv --profile=webui --quiet\
		"$${HOME}/Musik/jeder_tag.ogg" "$${HOME}/Musik/Die Planeten/"


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
