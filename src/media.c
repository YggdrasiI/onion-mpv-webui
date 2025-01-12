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



onion_connection_status list_share_page(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    // 1. Setup/Create relevant paths variables
    int ret = OCS_NOT_PROCESSED;

    char *uri_full_path = strdup(onion_request_get_fullpath(req));
    // Example: '/media/html/example/folder/subfolder/'
    //       or '/media/html/example/folder/subfolder'

    char *uri_rel_path = strdup(onion_request_get_path(req));
    // Example: 'folder/subfolder/'
    //       or 'folder/subfolder'

    const char * share_key = privdata->key;
    // Example: 'example'

    const char * share_path = privdata->media_path;
    // Example: '/some/dir'

    if( strcmp(".current", uri_rel_path) == 0 ){

      pthread_mutex_lock(&privdata->share_lock);
      free(uri_rel_path);
      uri_rel_path = strdup(privdata->last_requested_subdir);
      pthread_mutex_unlock(&privdata->share_lock);

      free(uri_full_path);
      asprintf(&uri_full_path, "/media/html/%s/%s",
          share_key, uri_rel_path);
    }


    // Holds file/folder destination of 'share_path/uri_rel_path', etc
    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);
    // Example: '/some/dir/folder/subfolder'

    // To resolve symbolic links/relative paths.
    char *real_local_path = NULL;

    // Strip '/' to normalize 'folder/' and 'folder' paths.
    strip_slash(uri_full_path);
    strip_slash(uri_rel_path);

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto media_share_html_end;
    }

    if ( !path_available(__tmp_path) ){
        /* File or parent folders is hidden .*/
        ret = OCS_INTERNAL_ERROR; 
        goto media_share_html_end;
    }

    real_local_path = realpath(__tmp_path, NULL);
    if (!real_local_path){
        ret = OCS_INTERNAL_ERROR;
        goto media_share_html_end;
    }

    // Save this subfolder as last used. 
    pthread_mutex_lock(&privdata->share_lock);
    free(privdata->last_requested_subdir);
    privdata->last_requested_subdir = strdup(uri_rel_path);
    pthread_mutex_unlock(&privdata->share_lock);

    /* 2. List, if real_local_path is directory.
     * Otherwise the the static file content will be returned,
     * see below.*/
    DIR *dir = opendir(real_local_path);
    if (dir) {
        // Fill the dictionary for html-template.
        onion_dict *d = onion_dict_new();
        onion_dict_add(d, "dirname", uri_full_path, 0);

        // Add "Go up" link. Using uri_rel_path because
        // uri_full_path is 'media/html/[share_name]'
        if (uri_rel_path[0] != '\0' && uri_rel_path[1] != '\0')
            onion_dict_add(d, "go_up", "true", 0);
        //onion_dict_add(d, "share_key", share_key, 0);
        //onion_dict_add(d, "uri_rel_path", uri_rel_path, 0);
        onion_dict_add(d, "share_key", add_slash_if_notempty(share_key),
            OD_FREE_VALUE);
        onion_dict_add(d, "uri_rel_path", add_slash_if_notempty(uri_rel_path),
            OD_FREE_VALUE);

        onion_dict *files = onion_dict_new();
        onion_dict_add(d, "files", files, OD_DICT | OD_FREE_VALUE);

        struct dirent *de;
        while ((de = readdir(dir))) {     // Fill one files.[filename] per file.
            // Skip ".", ".." and hidden files in listing
            if ( de->d_name[0] == '.' ) continue;

            int ps = snprintf(__tmp_path, __tmp_path_len,
                    "%s/%s",
                    real_local_path, de->d_name);

            if ( access(__tmp_path, R_OK) != 0 ){
              // file is not readable by this process. Skip it
              continue;
            }

            onion_dict *file = onion_dict_new();
            onion_dict_add(files, de->d_name, file,
                    OD_DUP_KEY | OD_DICT | OD_FREE_VALUE);

            onion_dict_add(file, "name", de->d_name, OD_DUP_VALUE);


            struct stat st;
            if( ps >= 0 && ps < __tmp_path_len ){
                stat(__tmp_path, &st);

                int s = st.st_size;

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", s);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "size", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "size", "snprintf fails", OD_DUP_VALUE);
                }

                // Human readable form
                if( s > 1E9 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f GB", ((double)s)/1.0E9 );
                } else if( s > 1E6 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f MB", ((double)s)/1E6 );
                } else if( s > 1E3 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f kB", ((double)s)/1E3 );
                } else {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%d B", s);
                }
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "size2", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "size2", "snprintf fails", OD_DUP_VALUE);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", (int)st.st_mtim.tv_sec);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "modified", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "modified", "snprintf fails", OD_DUP_VALUE);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", st.st_uid);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "owner", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "owner", "snprintf fails", OD_DUP_VALUE);
                }

                if (S_ISDIR(st.st_mode)){
                    onion_dict_add(file, "type", "dir", 0);
                }else{
                    onion_dict_add(file, "type", "file", 0);
                }

            }else{
                onion_dict_add(file, "size", "?", OD_DUP_VALUE);
                onion_dict_add(file, "modified", "?", OD_DUP_VALUE);
                onion_dict_add(file, "owner", "?", OD_DUP_VALUE);
                onion_dict_add(file, "type", "?", 0);
            }

        }
        closedir(dir);

        onion_response_set_header(res, "Content-Type",
                onion_mime_get("_.html"));
        ret = media_share_html_template(d, req, res); // this frees d!
        goto media_share_html_end;
    }

    // 3. Return file or print access denined message
    const char *ad = onion_dict_get(options, "allow_download_in_shares");
    if (ad && ad[0] != '0') {
        onion_response_set_header(res, "Content-Type", onion_mime_get(real_local_path));
        ret = onion_shortcut_response_file(real_local_path, req, res);
    }else{
        onion_response_set_header(res, "Content-Type",
                onion_mime_get("_.html"));
        ret = onion_shortcut_response(
                "<!DOCTYPE html>\n<head>\n"
                "<meta content=\"text/html; charset=UTF-8\" "
                "http-equiv=\"content-type\"/>\n</head>\n"
                "<body>Download not allowed</body></html>"
                , HTTP_FORBIDDEN, req, res);
    }

media_share_html_end:  // cleanup allocations
    free(real_local_path);
    free(uri_rel_path);
    free(uri_full_path);
    buffer_pool_release(path_buffers, __tmp_path);
    return ret;
}

onion_connection_status list_share_json(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    // 0. Change content type
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

    // 1. Setup/Create relevant paths variables
    int ret = OCS_NOT_PROCESSED;

    char *uri_full_path = strdup(onion_request_get_fullpath(req));
    // Example: '/media/api/list/example/folder/subfolder/'
    //       or '/media/api/list/example/folder/subfolder'

    char *uri_rel_path = strdup(onion_request_get_path(req));
    // Example: 'folder/subfolder/'
    //       or 'folder/subfolder'

    const char * share_key = privdata->key;
    // Example: 'example'

    const char * share_path = privdata->media_path;
    // Example: '/some/dir'

    if( strcmp(".current", uri_rel_path) == 0 ){
      pthread_mutex_lock(&privdata->share_lock);
      free(uri_rel_path);
      uri_rel_path = strdup(privdata->last_requested_subdir);
      pthread_mutex_unlock(&privdata->share_lock);

      free(uri_full_path);
      asprintf(&uri_full_path, "/media/api/list/%s/%s",
          share_key, uri_rel_path);

      //ONION_DEBUG("NEW uri_rel_path: %s", uri_rel_path);
      //ONION_DEBUG("NEW uri_full_path: %s", uri_full_path);
    }

    // Holds file/folder destination of 'share_path/uri_rel_path', etc
    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);
    // Example: '/some/dir/folder/subfolder'

    // To resolve symbolic links/relative paths.
    char *real_local_path = NULL;

    // Strip '/' to normalize 'folder/' and 'folder' paths.
    strip_slash(uri_full_path);
    strip_slash(uri_rel_path);

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto media_api_json_end;
    }

    if ( !path_available(__tmp_path) ){
        /* File or parent folders is hidden .*/
        ret = OCS_INTERNAL_ERROR; 
        goto media_api_json_end;
    }

    real_local_path = realpath(__tmp_path, NULL);
    if (!real_local_path){
        ret = OCS_INTERNAL_ERROR;
        goto media_api_json_end;
    }

    // Save this subfolder as last used. 
    pthread_mutex_lock(&privdata->share_lock);
    free(privdata->last_requested_subdir);
    privdata->last_requested_subdir = strdup(uri_rel_path);
    pthread_mutex_unlock(&privdata->share_lock);

    /* 2. List, if real_local_path is directory.
     * Otherwise the static file content will be returned,
     * see below.*/
    DIR *dir = opendir(real_local_path);
    if (dir) {
        // Fill the dictionary for html-template.
        onion_dict *d = onion_dict_new();
        onion_dict_add(d, "dirname", uri_full_path, 0);
        //onion_dict_add(d, "share_key", share_key, 0);
        //onion_dict_add(d, "uri_rel_path", uri_rel_path, 0);
        onion_dict_add(d, "share_key", add_slash_if_notempty(share_key),
            OD_FREE_VALUE);
        onion_dict_add(d, "uri_rel_path", add_slash_if_notempty(uri_rel_path),
            OD_FREE_VALUE);

        onion_dict *files = onion_dict_new();
        onion_dict_add(d, "files", files, OD_DICT | OD_FREE_VALUE);

        struct dirent *de;
        //int loop0=0;
        while ((de = readdir(dir))) {     // Fill one files.[filename] per file.
            // Skip ".", ".." and hidden files in listing
            if ( de->d_name[0] == '.' ) continue;

            int ps = snprintf(__tmp_path, __tmp_path_len,
                    "%s/%s",
                    real_local_path, de->d_name);

            if ( access(__tmp_path, R_OK) != 0 ){
              // file is not readable by this process. Skip it
              continue;
            }

            onion_dict *file = onion_dict_new();
            onion_dict_add(files, de->d_name, file,
                    OD_DUP_KEY | OD_DICT | OD_FREE_VALUE);

            onion_dict_add(file, "name", de->d_name, OD_DUP_VALUE);


            struct stat st;
            if( ps >= 0 && ps < __tmp_path_len ){
                stat(__tmp_path, &st);

                int s = st.st_size;

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", s);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "size", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "size", "snprintf fails", OD_DUP_VALUE);
                }

                // Human readable form
                if( s > 1E9 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f GB", ((double)s)/1.0E9 );
                } else if( s > 1E6 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f MB", ((double)s)/1E6 );
                } else if( s > 1E3 ) {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%.1f kB", ((double)s)/1E3 );
                } else {
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%d B", s);
                }

                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "size2", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "size2", "snprintf fails", OD_DUP_VALUE);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", (int)st.st_mtim.tv_sec);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "modified", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "modified", "snprintf fails", OD_DUP_VALUE);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", st.st_uid);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "owner", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "owner", "snprintf fails", OD_DUP_VALUE);
                }

                if (S_ISDIR(st.st_mode)){
                    onion_dict_add(file, "type", "dir", 0);
                }else{
                    onion_dict_add(file, "type", "file", 0);
                }

            }else{
                onion_dict_add(file, "size", "?", OD_DUP_VALUE);
                onion_dict_add(file, "modified", "?", OD_DUP_VALUE);
                onion_dict_add(file, "owner", "?", OD_DUP_VALUE);
                onion_dict_add(file, "type", "?", 0);
            }
            /*ps = snprintf(__tmp_path, __tmp_path_len,
                    "%d", loop0);
            onion_dict_add(file, "loop0", __tmp_path, OD_DUP_VALUE);
            loop0 += 1;*/

        }
        closedir(dir);

        onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
        ret = media_api_json_template(d, req, res); // this frees d!
        goto media_api_json_end;
    }

    // 3. Return file or print access denined message
    const char *ad = onion_dict_get(options, "allow_download_in_shares");
    if (ad && ad[0] != '0') {
        onion_response_set_header(res, "Content-Type", onion_mime_get(real_local_path));
        ret = onion_shortcut_response_file(real_local_path, req, res);
    }else{
        onion_response_set_header(res, "Content-Type",
                onion_mime_get("_.html"));
        ret = onion_shortcut_response(
                "<!DOCTYPE html>\n<head>\n"
                "<meta content=\"text/html; charset=UTF-8\" "
                "http-equiv=\"content-type\"/>\n</head>\n"
                "<body>Download not allowed</body></html>"
                , HTTP_FORBIDDEN, req, res);
    }

media_api_json_end:  // cleanup allocations
    free(real_local_path);
    free(uri_rel_path);
    free(uri_full_path);
    buffer_pool_release(path_buffers, __tmp_path);
    return ret;
}


void __invert_key_value(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    onion_dict_add(to_json, value, key, 0);
}

// Begin page /media/html lists all shares
int list_media_html(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    // The key->folder map
    onion_dict *shared_folders = (onion_dict *)privdata;

    // Derive key->key map for template
    onion_dict *shares = onion_dict_new(); // free'd together with to_json
    onion_dict_preorder(shared_folders, __invert_key_value, shares);

    const char *ad = onion_dict_get(options, "allow_download_in_shares");

    // Setup variables for template.
    onion_dict *d = onion_dict_new();
    onion_dict_add(d, "page_title", onion_dict_get(options, "page_title"), 0);
    onion_dict_add(d, "shares", shares, OD_DICT | OD_FREE_VALUE);
    onion_dict_add(d, "allow_download_in_shares",
            (ad && ad[0] != '0')?"1":"0", 0);

    onion_response_set_header(res, "Content-Type", onion_mime_get("_.html"));
    int ret = media_html_template(d, req, res); // this frees d!

    return ret;
}
// End page /media/html lists all shares


// Begin json /media/api/list json of all shares
void __list_share_func(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    char *api_path;
    asprintf(&api_path, "/media/api/list/%s", key);
    onion_dict_add(to_json, key, api_path, OD_FREE_VALUE);
}

int list_shares_json(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    //onion_dict *shared_folders = onion_dict_get_dict(
    //        (onion_dict *)privdata/* options */, "shared_folders");
    onion_dict *shared_folders = (onion_dict *)privdata;

    const char *ad = onion_dict_get(options, "allow_download_in_shares");

    // Init local vars
    int ret = OCS_NOT_PROCESSED;
    char to_json_length[22];
    onion_dict *to_json = onion_dict_new();
    onion_dict *shares = onion_dict_new(); // free'd together with to_json
    onion_dict_add(to_json, "shares", shares, OD_DICT | OD_FREE_VALUE);

    if (ad && ad[0] != '0') {
        onion_dict_add(to_json, "allow_download_in_shares", "1", 0);
    }else{
        onion_dict_add(to_json, "allow_download_in_shares", "0", 0);
    }

    // Collect keys from shared folders but not their local paths
    onion_dict_preorder(shared_folders, __list_share_func, shares);

    // Convert json into block and write it to output.
    onion_block *jresb = onion_dict_to_json(to_json);
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
    snprintf(to_json_length, 22, "%ld", onion_block_size(jresb));
    onion_response_set_header(res, "Content-Length",to_json_length);
    onion_response_write(res, onion_block_data(jresb), onion_block_size(jresb));
    ret = OCS_PROCESSED;


media_api_json_end:  // cleanup allocations
    onion_block_free(jresb);
    onion_dict_free(to_json);

    return ret;
}
// End json /media/api/list json of all shares


onion_connection_status api_playlist_add(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{

    onion_connection_status ret = OCS_NOT_PROCESSED;

    // 0. Change content type
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

    // Note: Some arguments of fullpath are already consumed.
    // Do not parse (parts of) fullpath for portability,
    // but look back into 'privdata'.
    char *uri_rel_path = strdup(onion_request_get_path(req));
    // Example: 'folder/subfolder/file.mp3'

    const char * share_key = privdata->key;
    // Example: 'example'

    const char * share_path = privdata->media_path;
    // Example: '/some/dir'

    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);

    // Get command
    const char * const command_name = "playlist_add";
    CommandHandler cmd = (CommandHandler) onion_dict_get(
            media_commands, command_name);

    if (!cmd){
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        onion_response_printf(res,
                "{\"message\": \"Command '%s' not defined.\"}",
                command_name);
        ret = OCS_PROCESSED;
        goto api_playlist_add_end;
    }


    /* Check data */
    if( uri_rel_path == NULL || strlen(uri_rel_path) == 0 ){
        ret = onion_shortcut_response(
                "{\"message\":\"Adding of whole share not supported.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_add_end;
    }

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto api_playlist_add_end;
    }

    /* Evaluate real path of share directory and file */
    if (NULL != strstr(__tmp_path, "/.")){
        ret = onion_shortcut_response(
                "{\"message\":\"Invalid path.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_add_end;
    }

    if( access( __tmp_path, R_OK ) == -1 ) {
        // file doesn't exist or is not readable
        ret = onion_shortcut_response(
                "{\"message\":\"File not readable.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_add_end;
    }

    /* Handle data */

    // Run command
    char *output_message = NULL;
    pthread_mutex_lock(&mpv_lock);
    int cmd_status = (cmd)(command_name,
             //"append-play", __tmp_path,
            /* Note: mpv's append-play seems not to start
             * the file as expected...
             * ...no difference to append */
             "append", __tmp_path,
            &output_message);
    pthread_mutex_unlock(&mpv_lock);

    if (cmd_status > 0){
        //onion_response_set_code(res, HTTP_OK); // default
        onion_response_printf(res,
                "{\"message\": \"success\"}");
        ret = OCS_PROCESSED;
    }else{
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        onion_response_printf(res,
                "{\"message\": \"%s\"}",
                output_message);
        ret = OCS_PROCESSED;
    }
    FREE(output_message);


api_playlist_add_end:  // cleanup allocations
    free(uri_rel_path);
    buffer_pool_release(path_buffers, __tmp_path);

    return ret;
}

onion_connection_status api_playlist_play(
        __share_data_t *privdata,
        onion_request * req,
        onion_response * res)
{

    onion_connection_status ret = OCS_NOT_PROCESSED;

    // 0. Change content type
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

    // Note: Some arguments of fullpath are already consumed.
    // Do not parse (parts of) fullpath for portability,
    // but look back into 'privdata'.
    char *uri_rel_path = strdup(onion_request_get_path(req));
    // Example: 'folder/subfolder/file.mp3'

    const char * share_key = privdata->key;
    // Example: 'example'

    const char * share_path = privdata->media_path;
    // Example: '/some/dir'

    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);

    // Get command
    const char * const command_name = "playlist_add";
    CommandHandler cmd = (CommandHandler) onion_dict_get(
            media_commands, command_name);

    if (!cmd){
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        onion_response_printf(res,
                "{\"message\": \"Command '%s' not defined.\"}",
                command_name);
        goto api_playlist_play_end;
    }


    /* Check data */
    if( uri_rel_path == NULL || strlen(uri_rel_path) == 0 ){
        ret = onion_shortcut_response(
                "{\"message\":\"Adding of whole share not supported.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_play_end;
    }

    /* Evaluate real path of share directory and file */
    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        goto api_playlist_play_end;
    }

    if (NULL != strstr(__tmp_path, "/.")){
        ret = onion_shortcut_response(
                "{\"message\":\"Invalid path.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_play_end;
    }

    if( access( __tmp_path, R_OK ) == -1 ) {
        // file doesn't exist or is not readable
        ret = onion_shortcut_response(
                "{\"message\":\"File not readable.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto api_playlist_play_end;
    }

    /* Handle data */

    // Run command
    char *output_message = NULL;
    pthread_mutex_lock(&mpv_lock);
    int cmd_status = (cmd)(command_name,
             //"append-play", __tmp_path,
            /* Note: mpv's append-play just starts file
             * if no other one is running
             * Thus, no difference to append in most cases. */
             "replace", __tmp_path,
            &output_message);
    pthread_mutex_unlock(&mpv_lock);

    if (cmd_status > 0){
        //onion_response_set_code(res, HTTP_OK); // default
        onion_response_printf(res,
                "{\"message\": \"success\"}");
        ret = OCS_PROCESSED;
    }else{
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        onion_response_printf(res,
                "{\"message\": \"%s\"}",
                output_message);
        ret = OCS_PROCESSED;
    }
    FREE(output_message);


api_playlist_play_end:  // cleanup allocations
    free(uri_rel_path);
    buffer_pool_release(path_buffers, __tmp_path);

    return ret;
}


onion_dict *setup_media_commands(){
  onion_dict *_commands = onion_dict_new();
  onion_dict_add(_commands, MEDIA_CMD(playlist_add), 0);
  return _commands;
}


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

    asprintf(&url_pattern1, "^media/html/%s([/]+|$)", key);
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
    asprintf(&url_pattern3, "^media/api/list/%s([/]+|$)", key);
    ONION_DEBUG("Connect url '%s' with '%s'", url_pattern3, media_folder);

    onion_handler *json_media = onion_handler_new(
            (onion_handler_handler) list_share_json,
            handler_data3,
            (onion_handler_private_data_free) __share_data_free);
    onion_url_add_handler(data->urls, url_pattern3, json_media);

    // Url pattern to add file of share
    char *url_pattern2=NULL;
    __share_data_t *handler_data2 = __share_data_new();
    handler_data2->key = strdup(key);
    handler_data2->media_path = strdup(value);

    asprintf(&url_pattern2, "^media/api/playlist_add/%s[/]+", key);
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

    asprintf(&url_pattern4, "^media/api/playlist_play/%s[/]+", key);
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

    onion_dict_preorder(shared_folders, __share_func, &data);

    // Overview over avail shares
    onion_handler *media_html = onion_handler_new(
            (onion_handler_handler) list_media_html,
            shared_folders,
            NULL);
    onion_url_add_handler(urls,  "^media(.html|/html([/]*))$", media_html);

    onion_handler *shares_json = onion_handler_new(
            (onion_handler_handler) list_shares_json,
            shared_folders,
            NULL);
    onion_url_add_handler(urls,  "^media/api/list([/]+|$)", shares_json);

    return 0;
}
