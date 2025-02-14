BIN=js-bundle-watchdog
SOURCES=js-bundle-watchdog.c argparse.c
BUNDLE=/run/shm/webui.bundle.js
#BUNDLE=/dev/shm/webui.bundle.js

FILES_TO_WATCH=../../webui-page/webui.js ../../webui-page ../../webui-page/static/js/swiped-events.min.js

all: $(BIN)
	@true

$(BIN): $(SOURCES) Makefile 
	gcc -std=c18 -g -o $(BIN) $(SOURCES)

run: $(BIN)
	./$(BIN) -o "$(BUNDLE)" \
			$(FILES_TO_WATCH)

# Hm detection on file base does not trigger?!
run2: $(BIN)
	./$(BIN) -o "$(BUNDLE)" ../../webui-page/webui.js ../../webui-page/*.js ../../webui-page/static/js/swiped-events.min.js

# Create bundle file and exit
oneshot: $(BIN)
	./$(BIN) -o "$(BUNDLE)" --oneshot \
			$(FILES_TO_WATCH)

# Compress bundle file and exit
min: $(BIN)
	./$(BIN) -o "$(BUNDLE)" --oneshot --terser="--compress --mangle reserved=['$$','require','exports']" \
			$(FILES_TO_WATCH)

