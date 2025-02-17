all:
	$(MAKE) -f Makefile.js
	$(MAKE) -f Makefile.css

%:
	$(MAKE) -f Makefile.js "$(MAKECMDGOALS)"
	$(MAKE) -f Makefile.css "$(MAKECMDGOALS)"
