#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mpv/client.h>

#include <onion/dict.h>

onion_dict *get_default_options();

void update_options(
        mpv_handle *mpv,
        const char *mpv_plugin_name,
        onion_dict *options);

/* Expands environment variables in given path.
 * 
 * Returns NULL if folder is not readable. */
char *expand_directory_path(
        const char *path);
