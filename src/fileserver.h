#pragma once

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct __fileserver_page_data_t {
    char *root_dir;

} __fileserver_page_data_t;

__fileserver_page_data_t *__fileserver_page_new(
        const char *root_dir);

void __fileserver_page_data_free (
        __fileserver_page_data_t *privdata); 

// Generated from 'otemplate'
// Print folder content. Just for debugging required.
int fileserver_html_template(onion_dict * context, onion_request * req,
                             onion_response * res);

// Return static files
int fileserver_page(
        __fileserver_page_data_t *privdata,
        onion_request * req,
        onion_response * res);

/* Handling of static files and directory listing */
int webui_onion_static_files(
        const char *root_dir,
        const char *pattern,
        onion_url *urls);
