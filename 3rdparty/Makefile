BUNDLE_NAME=onion-mpv-webui.bundle
TEMP_BRANCH=temp/with_patches

# This branch name has to be the same as in ../src/CMakeLists.txt
WEBUI_BRANCH=onion-mpv-webui

help:
	@echo "Dummy"

test:

apply_patches:
			cd onion && git checkout master && ( git branch -D ${TEMP_BRANCH} || true )\
				&& git checkout -b ${TEMP_BRANCH} \
				&& for i in ../*.patch ; do \
				echo "Apply '$$i'"; \
				git am "$$i" || exit -1; \
				done

apply_bundle:
			cd onion && git checkout master && ( git branch -D "${WEBUI_BRANCH}" || true ) \
				&& git checkout -b "${WEBUI_BRANCH}" \
				&& git pull "../${BUNDLE_NAME}" HEAD

# Re-Creating *.bundle from above patches
update_bundle:
	make apply_patches
	cd onion && git bundle create --version=3 "../${BUNDLE_NAME}" master..HEAD

