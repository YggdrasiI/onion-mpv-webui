BUNDLE_NAME=onion-mpv-webui.bundle
TEMP_BRANCH=temp/with_patches

# This branch name has to be the same as in ../src/CMakeLists.txt
WEBUI_BRANCH=onion-mpv-webui

help:
	@echo "Script to update libonion with own patches."
	@echo -e "To update the patch bundle:\n" \
	"  1. Commit your changes. \n" \
	"  2. Create new patch files, see target 'create_patches' \n" \
	"  3. Stash uncommited changes and run 'make update_bundle. \n"

create_patches:
	echo 'Example: git format-patch --start-number 10 HEAD~3'

apply_patches:
			cd onion && git checkout master && ( git branch -D ${TEMP_BRANCH} || true )\
				&& git checkout -b ${TEMP_BRANCH} \
				&& git am -p1 ../*.patch

apply_patches_loop:
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
	cd onion && git bundle create "../${BUNDLE_NAME}" master..HEAD

