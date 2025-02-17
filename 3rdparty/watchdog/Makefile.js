BIN=js-bundle-watchdog
SOURCES=js-bundle-watchdog.c argparse.c hashmap.c
BUNDLE=/run/shm/webui.bundle.js
#BUNDLE=/dev/shm/webui.bundle.js

FILES_TO_WATCH=../../webui-page/webui.js \
							 ../../webui-page \
							 ../../webui-page/static/js/swiped-events.min.js \
							 ../../webui-page/test.js ../../webui-page/test.js

# Debug or Release
BUILD_TYPE?=Debug
CC?=gcc
RELEASE_FLAGS?=-std=c18 -O3 -D_NODEBUG
DEBUG_FLAGS?=-std=c18 -g


all:
	@(test -f .Release && make Release) \
			|| (test -f .Debug && make Debug) \
			|| make $(BUILD_TYPE)

Debug:
	@echo "Debug build"
	@rm .Release 2>/dev/null || true
	@BUILD_TYPE=Debug CCFLAGS="$(DEBUG_FLAGS)" make $(BIN) 

Release:
	@echo "Release build"
	@rm .Debug 2>/dev/null || true
	@BUILD_TYPE=Release CCFLAGS="$(RELEASE_FLAGS)" make $(BIN) 

# Dependency to force rebuild if build type changes
.$(BUILD_TYPE):
	@touch .$(BUILD_TYPE)

$(BIN): $(SOURCES) Makefile .$(BUILD_TYPE)
	$(CC) $(CCFLAGS) -o $(BIN) $(SOURCES)


# Run program
run: $(BIN)
	./$(BIN) -o "$(BUNDLE)" \
			$(FILES_TO_WATCH)


# Create bundle file and exit
oneshot: $(BIN)
	./$(BIN) -o "$(BUNDLE)" --oneshot \
			$(FILES_TO_WATCH)

# Compress bundle file and exit
min: $(BIN)
	./$(BIN) -o "$(BUNDLE)" --oneshot --terser="--compress --mangle reserved=['$$','require','exports']" \
			$(FILES_TO_WATCH)


# Debugging targets
valgrind: $(BIN)
	valgrind ./$(BIN) -o "$(BUNDLE)" \
			$(FILES_TO_WATCH)

nemiver: $(BIN)
	nemiver ./$(BIN) -o "$(BUNDLE)" \
			$(FILES_TO_WATCH)
