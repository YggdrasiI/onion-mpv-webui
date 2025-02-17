all:
	$(MAKE) -f Makefile_js
	$(MAKE) -f Makefile_css


# Run both watchdogs parallel
run:
	./run.sh

%:
	$(MAKE) -f Makefile_js "$(MAKECMDGOALS)"
	$(MAKE) -f Makefile_css "$(MAKECMDGOALS)"

