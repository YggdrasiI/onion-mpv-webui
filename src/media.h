#pragma once

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <onion/types.h>
#include <onion/dict.h>

#include "mpv_api_commands.h"

struct share_info_t;
typedef struct share_info_t share_info_t;

share_info_t *share_info_new(const char *key, const char *path, int i_share);

void share_info_free (
        share_info_t *share_info);

const char *share_info_get_path(share_info_t *share_info);

/* Default is ->key and it falls back on ->key_enumerated */
const char *share_info_get_preferred_key(share_info_t *share_info);

/* Private data of handler (Used for 'onion_handler_new') */
typedef struct __share_data_media_html_t {
    //char *key; // TODO: Redundant
    //char *key_encoded; // TODO: Redundant
    //const char *media_path; // TODO: Redundant

    pthread_mutex_t share_lock;
    char *last_requested_subdir; // TODO: Redundant, Wobei der Look wichtig ist beim Schreiben, da verschiedene Threads schreiben könnten.
    share_info_t *share_info;
} __share_data_media_html_t;

__share_data_media_html_t *__share_data_media_html_new(
        share_info_t *share_info);

void __share_data_media_html_free (
        __share_data_media_html_t *privdata);

typedef struct __share_data_media_api_t {
    //char *key;
    const char *media_path;
    char *command_name;
    char *arg1;
    char *arg2;
    CommandHandler cmd;
    share_info_t *share_info;
} __share_data_media_api_t;

__share_data_media_api_t *__share_data_media_api_new(
        const char *command_name,
        const char *arg1,
        const char *arg2,
        share_info_t *share_info);

void __share_data_media_api_free (
        __share_data_media_api_t *privdata);

// Generated from 'otemplate'

int media_share_html_template(onion_dict * context, onion_request * req,
                             onion_response * res);
int media_html_template(onion_dict * context, onion_request * req,
                             onion_response * res);
int media_api_list_json_template(onion_dict * context, onion_request * req,
                             onion_response * res);
//footer.include   style.include

/* Handlers for '/media' section */
// Return list of filenames, but not allow downloading them
onion_connection_status media_page(
        __share_data_media_html_t *privdata,
        onion_request * req,
        onion_response * res);

/* Handlers for '/media' section */
// Return list of filenames, but not allow downloading them
onion_connection_status list_json(
        __share_data_media_html_t *privdata,
        onion_request * req,
        onion_response * res);

// Fetch from url argument for playlist_play, playlist_add command
onion_connection_status api_loadfile(
        __share_data_media_api_t *privdata,
        onion_request * req,
        onion_response * res);

// List of commands provided by '/media/api'
onion_dict *setup_media_commands();

/* Creates for each share (sub-dict of options)
 * an onion uri listener.
 */
onion_connection_status webui_onion_share_media_folders(
        onion_url *media_urls,
        onion_dict *options);
