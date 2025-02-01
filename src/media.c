#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>  // POSIX's basename("abc/") => "abc", not "" 
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
//#include <libintl.h>
#include <assert.h>

#include <onion/onion.h>
#include <onion/log.h>
#include <onion/dict.h>
#include <onion/url.h>
#include <onion/low.h> // onion_low_strdup

#define HAVE_PTHREADS // to read correct struct sizes from types_internal.h
#include <onion/types_internal.h> // onion_dict_t for share_info_t needed
#include <onion/mime.h>
#include <onion/shortcuts.h>
#ifdef ONION_BUILD
#include <onion/handlers/exportlocal.h>
#include <onion/handlers/static.h>
#else
#include <onion/exportlocal.h>
#include <onion/static.h>
#endif

#include <onion/dict.h>
#include <onion/block.h>
#include <onion/shortcuts.h>

#include "defines.h"
#include "mpv_api_commands.h"
#include "mpv_script_options.h"
//#include "mpv_plugin.h"
#include "tools.h"
#include "buffer_pool.h"
#include "percent_encoding.h"

#include "media.h"
#include "media_track_paths.h"

extern onion_dict *commands;
extern onion_dict *media_commands;
extern onion_dict *options;
extern pthread_mutex_t mpv_lock;
extern pthread_mutex_t mpv_lock;
extern media_track_paths_t *mtp;

extern buffer_pool_t *path_buffers;

typedef struct share_info_t {
    //char __offset[sizeof(onion_dict)];
    onion_dict __dict;
    // Dict propagated to context dict in templates
    //onion_dict *dict;

    // Shortcuts to values of dict
    char *key;
    char *key_enumerated;
    char *key_encoded;
    char *key_regex;
    char *path;
} share_info_t;

/* onion_dict + X. This is a copy of onion_dict_new() */
share_info_t *onion_extended_dict_new() {
  assert(sizeof(onion_dict) < sizeof(share_info_t));
  //onion_dict *dict = onion_low_calloc(1, sizeof(onion_dict));
  onion_dict *dict = onion_low_calloc(1, sizeof(share_info_t));
#if 0
#define HAVE_PTHREADS
#ifdef HAVE_PTHREADS
  pthread_rwlock_init(&dict->lock, NULL);
  pthread_mutex_init(&dict->refmutex, NULL);
#endif
  dict->refcount = 1;
  dict->cmp = strcmp;
  ONION_DEBUG0("New %p, refcount %d", dict, dict->refcount);
#else
  onion_dict *tmp = onion_dict_new();
  memcpy(dict, tmp, sizeof(onion_dict)); // clone content
  free(tmp);
#endif
  return (share_info_t *) dict;
}

share_info_t *share_info_new(const char *key, const char *path, int i_share){
    //share_info_t *share_info = calloc(1, sizeof(share_info_t));
    share_info_t* share_info = onion_extended_dict_new();

    // First part of struct is occupied by onion_dict and can be threaten like that.
    // Change pointer type to use it as onion_dict.
    // By allocating space for both structures together, the implicit freeing of onion_dict
    // will freeing both parts.
    onion_dict *dict = (onion_dict *)share_info;

    if (key != NULL ){
        share_info->key = onion_low_strdup(key);
        onion_dict_add(dict, "key", share_info->key, OD_FREE_VALUE);
    }
    if (i_share > 0 ){
        share_info->key_enumerated = enumerated_name(i_share);
        onion_dict_add(dict, "key_enumerated", share_info->key_enumerated, OD_FREE_VALUE);
    }
    if (path != NULL ){
        share_info->path = onion_low_strdup(path);
        onion_dict_add(dict, "path", share_info->path, OD_FREE_VALUE);
    }

    // key_encoded can not evaluated here because key_enumerated may change.
    // It will be set after __swap_enumerated_share_names (in __add_handler_for_shares)
    return share_info;
}

void share_info_free (
        share_info_t *share_info)
{
    onion_dict_free((onion_dict *)share_info);
}

const char *share_info_get_path(share_info_t *share_info){
    return share_info->path;
}

const char *share_info_get_preferred_key(share_info_t *share_info){
    const char *key_named = share_info->key;
    const char *key_enumerated = share_info->key_enumerated;
    return (key_named && key_named[0])?key_named:key_enumerated;
}

__share_data_media_html_t *__share_data_media_html_new(
        //const char *key,
        //const char *value,
        share_info_t *share_info)
{
    __share_data_media_html_t *data = calloc(1, sizeof(__share_data_media_html_t));

    data->share_info = share_info;
    //data->key = strdup(key);
    //data->key_encoded = encodeURIComponent(key);
    //data->media_path = strdup(value);
    //data->media_path = onion_dict_get(share_info, "path");
    if( pthread_mutex_init(&data->share_lock, NULL) != 0) {
      ONION_ERROR("\n mutex init failed\n");
    }
    data->last_requested_subdir = strdup("/");
    return data;
}

void __share_data_media_html_free (
        __share_data_media_html_t *privdata)
{
    //FREE(privdata->key);
    //FREE(privdata->key_encoded);
    //FREE(privdata->media_path);

    pthread_mutex_lock(&privdata->share_lock);
    FREE(privdata->last_requested_subdir);
    pthread_mutex_unlock(&privdata->share_lock);
    pthread_mutex_destroy(&privdata->share_lock);

    free(privdata);
}
__share_data_media_api_t *__share_data_media_api_new(
        const char *command_name,
        share_info_t *share_info)
{
    __share_data_media_api_t *data = calloc(1, sizeof(__share_data_media_api_t));
    data->share_info = share_info;
    //data->key = strdup(key);
    //data->media_path = strdup(value);
    data->media_path = onion_dict_get((onion_dict *)share_info, "path");

    data->command_name = strdup(command_name);
    data->cmd = (CommandHandler) onion_dict_get(
            media_commands, command_name);
    return data;
}

void __share_data_media_api_free (
        __share_data_media_api_t *privdata)
{
    //FREE(privdata->key);
    //FREE(privdata->media_path);
    FREE(privdata->command_name);
    free(privdata);
}

onion_dict *setup_media_commands(){
  onion_dict *_commands = onion_dict_new();
  onion_dict_add(_commands, MEDIA_CMD(playlist_add), 0);
  onion_dict_add(_commands, MEDIA_CMD(playlist_play), 0);
  return _commands;
}


#include "media_api_handler1.c"
#include "media_api_handler2.c"
#include "media_html_handler.c"




// Just for loop in webui_onion_share_media_folders-function.
typedef struct __share_loop_t {
    onion_url *html;
    onion_url *api;
    int first; // Flag for first run of loop
} __share_loop_t;

/* Searching for share with "key_enumerated" value and swap */
void __swap_enumerated_share_names(
        onion_dict *shares,
        const char *key_index, share_info_t *info1, int flags)
{
    const char *key_named = info1->key;
    const char *key_enum = info1->key_enumerated;
    assert(key_named != NULL);
    if (NULL == key_named) {
        ONION_ERROR("Hey, info dict containing no named key for index '%s'", key_index);
        return;
    }
    assert(key_enum != NULL);
    if (NULL == key_enum) {
        ONION_ERROR("Hey, info dict containing no enumerated key for index '%s'", key_index);
        return;
    }

    share_info_t *info2 = (share_info_t *) onion_dict_get_dict(shares, key_named);
    if (NULL == info2) { // Ok, the named key is not used as enumerated key. No name collision.
        return;
    }

    const char *key_enum2;
    const char *key_index2 = key_named;
    // Due previous swap operations, the key_index and (info1)["key_enumerate"] can differ. Cycle if we 
    // found the info dict which is containing key_named as "key_enumerate"-value

    key_enum2 = info2->key_enumerated;
    while (info2 && key_enum2 && 0 != strcmp(key_enum2, key_named) ){
        key_index2 = key_enum2;
        info2 = (share_info_t *) onion_dict_get_dict(shares, key_index2);
        key_enum2 = info2->key_enumerated;
    }

    assert(info2 != NULL);
    assert(key_enum2 != NULL);
    if (info2 == NULL || key_enum2 == NULL){
        ONION_ERROR("Error during shared folder name generation for %s.", key_named);
        return;
    }
    // Swap values
    char *tmp2 = onion_low_strdup(key_enum2);
    char *tmp1 = onion_low_strdup(key_enum);
    onion_dict_add((onion_dict *)info2, "key_enumerated", tmp1, OD_FREE_VALUE|OD_REPLACE); // free's key_enum2
    info2->key_enumerated = tmp1;
    onion_dict_add((onion_dict *)info1, "key_enumerated", tmp2, OD_FREE_VALUE|OD_REPLACE);
    info1->key_enumerated = tmp2;
}

void __add_handler_for_shares_cont(
        const __share_loop_t *data,
        const char *key,
        const char *path,
        share_info_t *share_info);

void __add_handler_for_shares(
        __share_loop_t *data,
        const char *_, share_info_t *share_info, int flags)
{
    const char *path = share_info->path;
    const char *key_named = share_info->key;
    const char *key_enumerated = share_info->key_enumerated;

    ONION_DEBUG("Share '%s' as '%s'", path, (key_named && key_named[0])?key_named:key_enumerated);

    // Now, after the swap process, we can evaluate "key_encoded"-value
    if (key_named && key_named[0]) {
        share_info->key_encoded = encodeURIComponent(key_named);
        onion_dict_add((onion_dict *)share_info, "key_encoded", share_info->key_encoded, OD_FREE_VALUE);
    }else{
        share_info->key_encoded = share_info->key_enumerated;
        onion_dict_add((onion_dict *) share_info, "key_encoded", share_info->key_enumerated, 0);
    }

    if (key_named && key_named[0]) __add_handler_for_shares_cont(data, key_named, path, share_info);
    if (key_enumerated && key_enumerated[0]) __add_handler_for_shares_cont(data, key_enumerated, path, share_info);


    // Use first share as default one (.current)
    if (data->first){
        data->first = 0;
        if (mtp && NULL == media_track_paths_get_current_share(mtp)){
            media_track_paths_set_directory(mtp, share_info->path);
        }
    }
}

void __add_handler_for_shares_cont(
        const __share_loop_t *data,
        const char *key,
        const char *path,
        share_info_t *share_info)
{
    // 3. Connect folder with handlers
    char *keychars_in_braces = regex_encapsule_chars(key); // + -> [+], etc

    // 1. /media/html/{sharename}
    // Create hander with privdata about shared dir.
    onion_handler *list_share = onion_handler_new(
            (onion_handler_handler) list_share_page,
            __share_data_media_html_new(share_info), // Privdata
            (onion_handler_private_data_free) __share_data_media_html_free);

    // Url pattern matches top-level of share
    char *url_pattern1=NULL;
    asprintf(&url_pattern1, "^[/]+%s", keychars_in_braces);
    ONION_DEBUG0("Connect url '%s' with '%s'", url_pattern1, path);
    onion_url_add_handler(data->html, url_pattern1, list_share);


    // 2. /media/api/list/{sharename}
    onion_handler *json_media_handler = onion_handler_new(
            (onion_handler_handler) media_api_list,
            __share_data_media_html_new(share_info),
            (onion_handler_private_data_free) __share_data_media_html_free);

    // Url pattern matches top-level of share
    char *url_pattern2=NULL;
    asprintf(&url_pattern2, "^[/]+list[/]+%s", keychars_in_braces);
    ONION_DEBUG0("Connect url '%s' with '%s'", url_pattern2, path);
    onion_url_add_handler(data->api, url_pattern2, json_media_handler);

    // 3. Other /media/api/ stuff
    // Url pattern to add file of share
    char *url_pattern3=NULL;
    onion_handler *add_media = onion_handler_new(
            (onion_handler_handler) media_api_loadfile,
            __share_data_media_api_new("playlist_add", share_info),
            (onion_handler_private_data_free) __share_data_media_api_free);

    asprintf(&url_pattern3, "^[/]+playlist_add[/]+%s", keychars_in_braces);
    onion_url_add_handler(data->api, url_pattern3, add_media);

    // Url pattern to add file of share
    char *url_pattern4=NULL;
    onion_handler *add_media4 = onion_handler_new(
            (onion_handler_handler) media_api_loadfile,
            __share_data_media_api_new("playlist_play", share_info),
            (onion_handler_private_data_free) __share_data_media_api_free);

    asprintf(&url_pattern4, "^[/]+playlist_play[/]+%s", keychars_in_braces);
    onion_url_add_handler(data->api, url_pattern4, add_media4);

    free(url_pattern4);
    free(url_pattern3);
    free(url_pattern2);
    free(url_pattern1);
    free(keychars_in_braces);
}

int webui_onion_share_media_folders(
        onion_url *media_urls,
        onion_dict *options)
{
    onion_dict *shared_folders = onion_dict_get_dict(options, "shared_folders");

    // 0. sub level urls: 
    onion_url *html = onion_url_new();
    onion_url *api = onion_url_new();
    onion_url_add_url(media_urls, "^[/]+html", html); //  /media/html/[…]
    onion_url_add_url(media_urls, "^[/]+api", api);    //  /media/api/[]

    // 1. root level: Overview over avail shares
    onion_url_add_with_data(media_urls, "", onion_shortcut_internal_redirect, "/media.html", NULL);
    onion_url_add_handler(media_urls,  ".html",
            onion_handler_new(
                (onion_handler_handler) media_html_root,
                shared_folders, NULL));

    onion_url_add_handler(html, "^[/]*$", 
            onion_handler_new(
                (onion_handler_handler) media_html_root,
                shared_folders, NULL));

    onion_url_add_handler(html, "^[/]+.current[/]*", 
            onion_handler_new(
                (onion_handler_handler) media_html_redirect_current,
                shared_folders, NULL));

    onion_url_add_handler(api,  "^[/]+list[/]*$", 
            onion_handler_new(
                (onion_handler_handler) media_api_list_root,
                shared_folders,
                NULL));

#if 1
    onion_url_add_handler(api, "^[/]+list[/]+.current[/]*", 
            onion_handler_new(
                (onion_handler_handler) media_api_list_redirect_current,
                shared_folders, NULL));
#endif
    /*
    // 2. /media/api/list/.current
    onion_handler *json_media_handler = onion_handler_new(
            (onion_handler_handler) media_api_list,
            __share_data_media_html_new(share_info),
            (onion_handler_private_data_free) __share_data_media_html_free);

    onion_url_add_handler(data->api,  "^[/]+list[/]+.current[/]*", json_media_handler);
    */


    // 3. Now create enummeration names for shares and then loop over shares
    // to add handlers for each of them.
    // 3a) preparition: Collisionsfree add of enumerated share names 'shareX'.
    onion_dict_preorder(shared_folders, __swap_enumerated_share_names, shared_folders);

    // 3b) Construct url->handler mappings
    __share_loop_t data = {.html = html, .api = api, .first = 1};
    onion_dict_preorder(shared_folders, __add_handler_for_shares, &data);

    return 0;
}
