#
# This script patches the onion cmake script to
# get a working version on Termux (Android) environments.
#
ONION_PATH="$1"
EXTRA_INCLUDE_DIR="$2"
SYSTEM="$3"

PATCH="--- CMakeLists.txt
+++ CMakeLists.txt
@@ -19,6 +19,19 @@ SET(ONION_USE_BINDINGS_CPP true CACHE BOOL \"Compile the CPP bindings\")
 SET(ONION_POLLER default CACHE string \"Default poller to use: default | epoll | libev | libevent\")
 ########
 
+if(\"\${CMAKE_SYSTEM_NAME}\" EQUAL \"Android\")  # True in Termux
+       # Add folder with execinfo.h
+       include_directories(\"$2\")
+
+       # Link extra library log on all targets
+       macro (add_executable _name)
+               # invoke built-in add_executable
+               _add_executable(\${ARGV})
+               if (TARGET \${_name})
+                       target_link_libraries(\${_name} log)
+               endif()
+       endmacro()
+endif(\"\${CMAKE_SYSTEM_NAME}\" STREQUAL \"Android\")
 set(CMAKE_MODULE_PATH \${CMAKE_MODULE_PATH} \"\${CMAKE_SOURCE_DIR}/CMakeModules\")
 
 execute_process("

echo "System: $SYSTEM"
# echo "Patch: " ; echo "${PATCH}"

if [ -n "$ONION_PATH" -a -e "$ONION_PATH" ] ; then
        cd "${ONION_PATH}" && grep -q "Android" "CMakeLists.txt" \
                || patch -p0 <<< "$PATCH"
else
        echo "Patching of CMakeLists.txt of onion library failed. Folder not found."
        exit -1
fi
