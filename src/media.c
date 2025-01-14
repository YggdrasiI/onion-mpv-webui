#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
//#include <libintl.h>
#include <assert.h>
#include <wordexp.h>

#include <onion/onion.h>
#include <onion/log.h>
#include <onion/dict.h>
#include <onion/url.h>
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

#include "mpv_api_commands.h"
#include "mpv_script_options.h"
#include "defines.h"
#include "tools.h"
#include "buffer_pool.h"

#include "media.h"
#include "percent_encoding.h"

extern onion_dict *commands;
extern onion_dict *media_commands;
extern onion_dict *options;
extern pthread_mutex_t mpv_lock;

extern buffer_pool_t *path_buffers;

__share_data_t *__share_data_new()
{
    __share_data_t *data = calloc(1, sizeof(__share_data_t));

    data->last_requested_subdir = strdup("");
    if( pthread_mutex_init(&data->share_lock, NULL) != 0) {
      ONION_ERROR("\n mutex init failed\n");
    }
    return data;
}

void __share_data_free (
        __share_data_t *privdata)
{
    FREE(privdata->key);
    FREE(privdata->media_path);

    pthread_mutex_lock(&privdata->share_lock);
    FREE(privdata->last_requested_subdir);
    pthread_mutex_unlock(&privdata->share_lock);
    pthread_mutex_destroy(&privdata->share_lock);

    free(privdata);
}

onion_dict *setup_media_commands(){
  onion_dict *_commands = onion_dict_new();
  onion_dict_add(_commands, MEDIA_CMD(playlist_add), 0);
  return _commands;
}


#include "media_api_handler1.c"
#include "media_api_handler2.c"
#include "media_html_handler.c"




// Just for loop in webui_onion_share_media_folders-function.
typedef struct __share_loop_t {
    onion_url *urls;
    int num_shares;
    int add_with_enumerated_key;
} __share_loop_t;

void __share_func(
        __share_loop_t *data,
        const char *key, const void *value, int flags)
{
    //ONION_DEBUG("Share '%s' as '%s'", value, key);

    // 1. Expand environment variables
    char *expanded_value;
    wordexp_t p;
    int expand_fail = wordexp( value, &p, WRDE_NOCMD);
    if (expand_fail || p.we_wordc < 1 ){
        expanded_value = strdup(value);
    }else{
        expanded_value = strdup(p.we_wordv[0]);
    }
    wordfree( &p );

    // 2. Check if folder exists
    char *media_folder = realpath(expanded_value, NULL);
    free(expanded_value);

    if (!media_folder) return;

    DIR *dir = opendir(media_folder);
    if (!dir) {
        free(media_folder);
        return;
    }
    // Fetch content of directory here if required.
    // […]
    closedir(dir);

    // 3. Connect folder with handlers

    /* Attention: value-pointer is not peristent(?!)
     *  onion_dict *shared_folders = onion_dict_get_dict(options, "shared_folders");
     *  const char *mf = onion_dict_get(options, key);
     *  assert( mf == value ); // fails
     *
     *  => Make copy of value or use reference on value in options dict.
     */

    // Url pattern to list share and it's subfolders 
    char *url_pattern1=NULL;
    __share_data_t *handler_data1 = __share_data_new();
    handler_data1->key = strdup(key);
    handler_data1->media_path = strdup(value);

    //asprintf(&url_pattern1, "^media/html/%s([/]+|$)", key);
    asprintf(&url_pattern1, "^media/html[/]+[^/]+([/]+|$)", key);
    ONION_DEBUG("Connect url '%s' with '%s'", url_pattern1, media_folder);

    onion_handler *list_share = onion_handler_new(
            (onion_handler_handler) list_share_page,
            handler_data1,
            (onion_handler_private_data_free) __share_data_free);
    onion_url_add_handler(data->urls, url_pattern1, list_share);

    // Url pattern to provide list of files as json
    char *url_pattern3=NULL;
    __share_data_t *handler_data3 = __share_data_new();
    handler_data3->key = strdup(key);
    handler_data3->media_path = strdup(value);

    //asprintf(&url_pattern3, "^media/api/list/%s([/]+|$)", key);
    asprintf(&url_pattern3, "^media/api/list[/]+[^/]+([/]+|$)", key);
    ONION_DEBUG("Connect url '%s' with '%s'", url_pattern3, media_folder);

    onion_handler *json_media = onion_handler_new(
            (onion_handler_handler) media_api_list,
            handler_data3,
            (onion_handler_private_data_free) __share_data_free);
    onion_url_add_handler(data->urls, url_pattern3, json_media);

    // Url pattern to add file of share
    char *url_pattern2=NULL;
    __share_data_t *handler_data2 = __share_data_new();
    handler_data2->key = strdup(key);
    handler_data2->media_path = strdup(value);

    asprintf(&url_pattern2, "^media/api/playlist_add[/]+[^/]+[/]+", key);
    //printf("Connect pattern '%s'\n", url_pattern2);

    onion_handler *add_media = onion_handler_new(
            (onion_handler_handler) api_playlist_add,
            handler_data2,
            (onion_handler_private_data_free) __share_data_free);
    onion_url_add_handler(data->urls, url_pattern2, add_media);

    // Url pattern to add file of share
    char *url_pattern4=NULL;
    __share_data_t *handler_data4 = __share_data_new();
    handler_data4->key = strdup(key);
    handler_data4->media_path = strdup(value);

    asprintf(&url_pattern4, "^media/api/playlist_play[/]+[^/]+[/]+", key);
    //printf("Connect pattern '%s'\n", url_pattern4);

    onion_handler *add_media4 = onion_handler_new(
            (onion_handler_handler) api_playlist_play,
            handler_data4,
            (onion_handler_private_data_free) __share_data_free);
    onion_url_add_handler(data->urls, url_pattern4, add_media4);

    free(url_pattern4);
    free(url_pattern3);
    free(url_pattern2);
    free(url_pattern1);
    free(media_folder);

    /* Allow access also over keys 'share1', 'share2, etc */
    int n_consumed = -1;
    sscanf(key, "share%*d%n", &n_consumed);
    int is_already_enummerated_key = (strlen(key) == n_consumed);

    if( data->add_with_enumerated_key && !is_already_enummerated_key)
    {
        data->add_with_enumerated_key = 0;
        char enumerated_name[20]; // share1, etc
        int written = snprintf(enumerated_name, sizeof(enumerated_name),
                "%s%d",
                "share", (data->num_shares+1));
        if (written > 0 && written < sizeof(enumerated_name)){
            __share_func(data, enumerated_name, value, flags);
        }
        data->add_with_enumerated_key = 1;
    }


    data->num_shares += 1;

}


int webui_onion_share_media_folders(
        onion_dict *options,
        onion_url *urls)
{
    onion_dict *shared_folders = onion_dict_get_dict(options, "shared_folders");
    __share_loop_t data = { urls, 0, 1};

    // sub levels: 
    onion_dict_preorder(shared_folders, __share_func, &data);

    // /media, /media/html and /media/api levels
    //onion_url *media = onion_url_new();

    // root level: Overview over avail shares
    onion_handler *media_html = onion_handler_new(
            (onion_handler_handler) media_html_root,
            shared_folders,
            NULL);
    onion_url_add_handler(urls,  "^media(.html|/html([/]*))$", media_html);

    onion_handler *shares_json = onion_handler_new(
            (onion_handler_handler) media_api_list_root,
            shared_folders,
            NULL);
    onion_url_add_handler(urls,  "^media/api/list([/]+|$)", shares_json);

    return 0;
}
