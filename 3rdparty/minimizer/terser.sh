#!/bin/bash
#set -euo pipefail

#ROOT_DIR=$(dirname $0)
ROOT_DIR=$(realpath $(dirname $0))

# Folder for terser npm packag
TERSER_ROOT="$ROOT_DIR/npm-global"
TERSER_BIN="not-found"


search_terser() {
	which "terser"
	if [ "0" = "$?" ] ; then
		TERSER_BIN="terser"
		return 0
	fi

	if [ -f "${TERSER_ROOT}/bin/terser" ] ; then
		TERSER_BIN="${TERSER_ROOT}/bin/terser"
		return 0
	fi

	ORIG_PREFIX=$(npm config get prefix)
	if [ -f "${ORIG_PREFIX}/bin/terser" ] ; then
		TERSER_BIN="${ORIG_PREFIX}/bin/terser"
		return 0
	fi

	return -1
}

install() {
	echo "Install terser…"

	ORIG_ROOT=$(npm root -g)
	test "$ORIG_PREFIX" = "" && ORIG_PREFIX=$(npm config get prefix)

	# Install terser with -g flag (for bin/-scripts)
	mkdir "$TERSER_ROOT"
	npm config set prefix "$TERSER_ROOT"
	npm install terser -g

	# Reset prefix
	if [ "$ORIG_PREFIX" = "$ORIG_ROOT/lib/node_modules" ] ; then
		npm config delete prefix
	else
		npm config set prefix "$ORIG_PREFIX"
	fi

	echo "… installed"
	TERSER_BIN="${TERSER_ROOT}/bin/terser"
}

if [ "$1" = "install" ] ; then
	search_terser || install || true
	echo "Run '${TERSER_BIN}' to minimize JS code."

elif [ "$1" = "path" ] ; then
	search_terser || true
	test "$TERSER_BIN" = "not-found" && echo "Error" && exit -1
	echo "$TERSER_BIN"

else
	search_terser || true
	test "$TERSER_BIN" = "not-found" && echo "Terser not found" && exit -1

	# Run terser
	"$TERSER_BIN" "$@"
fi

exit "$?"
