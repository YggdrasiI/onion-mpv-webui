#!/bin/bash
#set -euo pipefail

#ROOT_DIR=$(dirname $0)
ROOT_DIR=$(realpath $(dirname $0))

# Folder for uglifycss npm packag
UGLIFYCSS_ROOT="$ROOT_DIR/npm-global"
UGLIFYCSS_BIN="not-found"


search_uglifycss() {
	which "uglifycss" 2> /dev/null
	if [ "0" = "$?" ] ; then
		UGLIFYCSS_BIN="uglifycss"
		return 0
	fi

	if [ -f "${UGLIFYCSS_ROOT}/bin/uglifycss" ] ; then
		UGLIFYCSS_BIN="${UGLIFYCSS_ROOT}/bin/uglifycss"
		return 0
	fi

	ORIG_PREFIX=$(npm config get prefix)
	if [ -f "${ORIG_PREFIX}/bin/uglifycss" ] ; then
		UGLIFYCSS_BIN="${ORIG_PREFIX}/bin/uglifycss"
		return 0
	fi

	return -1
}

install() {
	echo "Install uglifycss…"

	ORIG_ROOT=$(npm root -g)
	test "$ORIG_PREFIX" = "" && ORIG_PREFIX=$(npm config get prefix)

	# Install uglifycss with -g flag (for bin/-scripts)
	mkdir "$UGLIFYCSS_ROOT"
	npm config set prefix "$UGLIFYCSS_ROOT"
	npm install uglifycss -g

	# Reset prefix
	if [ "$ORIG_PREFIX" = "$ORIG_ROOT/lib/node_modules" ] ; then
		npm config delete prefix
	else
		npm config set prefix "$ORIG_PREFIX"
	fi

	echo "… installed"
	UGLIFYCSS_BIN="${UGLIFYCSS_ROOT}/bin/uglifycss"
}

if [ "$1" = "install" ] ; then
	search_uglifycss || install || true
	echo "Run '${UGLIFYCSS_BIN}' to minimize JS code."

elif [ "$1" = "path" ] ; then
	search_uglifycss || true
	test "$UGLIFYCSS_BIN" = "not-found" && echo "not-found" && exit -1
	echo "$UGLIFYCSS_BIN"

else
	search_uglifycss || true
	test "$UGLIFYCSS_BIN" = "not-found" && echo "Uglifycss not found" && exit -1

	# Run uglifycss
	"$UGLIFYCSS_BIN" "$@"
fi

exit "$?"
