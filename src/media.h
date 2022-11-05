#pragma once

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Private data of handler (Used for 'onion_handler_new') */
typedef struct __share_data_t {
    char *key;
    char *media_path;

    pthread_mutex_t share_lock;
    char *last_requested_subdir;
} __share_data_t;

__share_data_t *__share_data_new();
void __share_data_free (
        __share_data_t *privdata);

// Generated from 'otemplate'
int media_html_template(onion_dict * context, onion_request * req,
                             onion_response * res);

int media_json_template(onion_dict * context, onion_request * req,
                             onion_response * res);

/* Handlers for '/media' section */
// Return list of filenames, but not allow downloading them
onion_connection_status media_page(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res);

/* Handlers for '/media' section */
// Return list of filenames, but not allow downloading them
onion_connection_status list_json(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res);

// Fetch from url argument for playlist_add command
onion_connection_status api_playlist_add(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res);

// Fetch from url argument for playlist_add command
// similar to api_playlist_add, but switch playback to new file.
onion_connection_status api_playlist_play(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res);

// List of commands provided by '/media/api'
onion_dict *setup_media_commands();

/* Creates for each share (sub-dict of options)
 * an onion uri listener.
 */
onion_connection_status webui_onion_share_media_folders(
        onion_dict *options,
        onion_url *urls);
