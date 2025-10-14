#include <inttypes.h>

#if 0
// Begin page /media.html, /media/html lists all shares
void __invert_key_value(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    onion_dict_add(to_json, value, key, 0);
}
// Map on {"name": key and "encoded: key_encoded}
void __invert_key_value2(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    onion_dict *d = onion_dict_new();
    onion_dict_add(d, "name", key, 0);
    onion_dict_add(d, "encoded", encodeURIComponent(key), OD_FREE_VALUE);
    onion_dict_add(to_json, value, d, OD_DICT | OD_FREE_VALUE);
}
#endif

onion_connection_status media_html_root(
        onion_dict *shared_folders,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    // The key->share_info map
    //onion_dict *shared_folders = (onion_dict *)privdata;

    // Derive key->key map for template
    //onion_dict *shares = onion_dict_new(); // free'd together with to_json
    //onion_dict_preorder(shared_folders, __invert_key_value2, shares);

    const char *ad = onion_dict_get(options, "allow_download_in_shares");

    // Setup variables for template.
    onion_dict *d = onion_dict_new();
    onion_dict_add(d, "page_title", onion_dict_get(options, "page_title"), 0);
    onion_dict_add(d, "shares", shared_folders, OD_DICT);
    onion_dict_add(d, "allow_download_in_shares",
            (ad && ad[0] != '0')?"1":"0", 0);

    onion_response_set_header(res, "Content-Type", onion_mime_get("_.html"));
    int ret = media_html_template(d, req, res); // this frees d!

    return ret;
}
// End page /media.html, /media/html lists all shares



// For /media/html/{share name}[/]*
//     =>  Template file: media_share.html
onion_connection_status list_share_page(
        __share_data_media_html_t *privdata,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    // 1. Setup/Create relevant paths variables
    int ret = OCS_NOT_PROCESSED;
#if TEMPLATES_WITH_SHORTEN_NAMES > 0
    const size_t max_display_filename_len = TEMPLATES_WITH_SHORTEN_NAMES;
#endif

    //char *uri_full_path = strdup(onion_request_get_fullpath(req));
    // Example: '/media/html/example/folder/subfolder/'
    //       or '/media/html/example/folder/subfolder'

    // Take from /media/html/example////folder… the substring
    //          "" or "/folder…"
    char *uri_rel_path = left_slashed_string_if_not_empty(onion_request_get_path(req));

    if( strcmp("/.current", uri_rel_path) == 0 ){
      const char *tmp = onion_dict_get((onion_dict *)privdata->share_info, "current_directory");
      if (tmp != NULL){
          pthread_mutex_lock(&privdata->share_lock);
          free(uri_rel_path);
          //uri_rel_path = onion_low_strdup(privdata->last_requested_subdir);
          uri_rel_path = onion_low_strdup(tmp);
          pthread_mutex_unlock(&privdata->share_lock);
      }else{
          uri_rel_path = onion_low_strdup("/");
      }
    }

    const char * share_key = privdata->share_info->key;
    const char * share_key_encoded = privdata->share_info->key_encoded;
    // Example: 'example'

    const char * share_path = privdata->share_info->path;;
    // Example: '/some/dir'


    // Holds file/folder destination of 'share_path/uri_rel_path', etc
    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);
    // Example: '/some/dir/folder/subfolder'

    // To resolve symbolic links/relative paths.
    char *real_local_path = NULL;

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto media_share_html_error;
    }

    if ( !path_available(__tmp_path) ){
        /* File or parent folders is hidden .*/
        ret = OCS_INTERNAL_ERROR; 
        goto media_share_html_error;
    }

    real_local_path = realpath(__tmp_path, NULL);
    if (!real_local_path){
        ret = OCS_INTERNAL_ERROR;
        goto media_share_html_error;
    }

    // Save this subfolder as last used. 
    pthread_mutex_lock(&privdata->share_lock);
    //free(privdata->last_requested_subdir);
    //privdata->last_requested_subdir = uri_rel_path;
    onion_dict_add((onion_dict *)privdata->share_info, "current_directory", uri_rel_path, OD_FREE_VALUE|OD_REPLACE);
    pthread_mutex_unlock(&privdata->share_lock);

    /* 2. List, if real_local_path is directory.
     * Otherwise the the static file content will be returned,
     * see below.*/
    DIR *dir = opendir(real_local_path);
    if (dir) {
        // Fill the dictionary for html-template.
        onion_dict *d = onion_dict_new();
        //onion_dict_add(d, "uri", uri_full_path, 0);
        onion_dict_add(d, "page_title", onion_dict_get(options, "page_title"), 0);

        const char *ad = onion_dict_get(options, "allow_download_in_shares");
        onion_dict_add(d, "allow_download_in_shares",
            (ad && ad[0] != '0')?"1":"0", 0);

        // Add "Go up" link. Using uri_rel_path because
        // uri_full_path is 'media/html/[share_name]'
        //if (uri_rel_path[0] != '\0' && uri_rel_path[1] != '\0')
        if (1)
            onion_dict_add(d, "go_up", "true", 0);

        onion_dict_add(d, "share_key", share_key, 0);
        onion_dict_add(d, "share_key_encoded", share_key_encoded, 0);
        //onion_dict_add(d, "uri_rel_path", uri_rel_path, 0);

        /* uri_rel_path ist immer noch schwierig zu behandeln:
         * 1. decode()-Prozess von onion entschlüsselt alle %-Encoding.
         * 2a. wenn ich es mit encodeURIComponent() encodiere erwische ich alle
         * Slashs.
         * 2b. wenn ich encodeURI() nehme, vergesse ich '+'.  
         * => Benutze encodeUnixPath(…)
         */
        
        if (uri_rel_path[0] == '\0' || uri_rel_path[1] == '\0') { // To avoid empty dir string "/"
            onion_dict_add(d, "dirname", share_key, 0);
            onion_dict_add(d, "uri_rel_path", "", 0);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
        onion_dict_add(d, "uri_rel_path_encoded", "", 0);
#endif
        }else{
            /* See comment in media_api_list(…) about usage of basename()
             * TLDR: Using it here is ok.*/
            onion_dict_add(d, "dirname", basename(uri_rel_path), OD_DUP_VALUE);
            onion_dict_add(d, "uri_rel_path", uri_rel_path, 0);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
        onion_dict_add(d, "uri_rel_path_encoded",
                encodeUnixPath(uri_rel_path), OD_FREE_VALUE);
#endif
        }

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

            const char *name = onion_low_strdup(de->d_name);
            onion_dict_add(file, "name", name, OD_FREE_VALUE);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
            // Encode filename
            onion_dict_add(file, "name_encoded",
                    encodeURIComponent(de->d_name), OD_FREE_VALUE);
#endif

            struct stat st;
            if( ps >= 0 && ps < __tmp_path_len && 0 == stat(__tmp_path, &st) )
            {
                int is_dir = S_ISDIR(st.st_mode);
                if (is_dir){
                    onion_dict_add(file, "type", "dir", 0);
                }else{
                    onion_dict_add(file, "type", "file", 0);
                }

                if (!is_dir){
                    ps = snprintf(__tmp_path, __tmp_path_len,
                            "%" PRIu64, st.st_size); //"%lld", st.st_size);
                    if( ps >= 0 && ps < __tmp_path_len ){
                        onion_dict_add(file, "size", __tmp_path, OD_DUP_VALUE);
                    }else{
                        onion_dict_add(file, "size", "snprintf fails", OD_DUP_VALUE);
                    }

                    ps = print_filesize_BIN64(__tmp_path, __tmp_path_len, st.st_size);
                    if( ps >= 0 && ps < __tmp_path_len ){
                        onion_dict_add(file, "size2", __tmp_path, OD_DUP_VALUE);
                    }else{
                        onion_dict_add(file, "size2", "snprintf fails", OD_DUP_VALUE);
                    }
                }else{
                    onion_dict_add(file, "size", "", 0);
                    onion_dict_add(file, "size2", "", 0);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", (int)st.st_mtim.tv_sec);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "modified", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "modified", "snprintf fails", 0);
                }

                ps = snprintf(__tmp_path, __tmp_path_len,
                        "%d", st.st_uid);
                if( ps >= 0 && ps < __tmp_path_len ){
                    onion_dict_add(file, "owner", __tmp_path, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "owner", "snprintf fails", 0);
                }

            }else{
                onion_dict_add(file, "size", "?", 0);
                onion_dict_add(file, "size2", "?", 0);
                onion_dict_add(file, "modified", "?", 0);
                onion_dict_add(file, "owner", "?", 0);
                onion_dict_add(file, "type", "?", 0);
            }

#if TEMPLATES_WITH_SHORTEN_NAMES
            // Add shorten name to avoid long lines
            if (strlen(de->d_name) > max_display_filename_len) {
                de->d_name[max_display_filename_len-1] = '\0';
                de->d_name[max_display_filename_len-2] = '.';
                de->d_name[max_display_filename_len-3] = '.';
                de->d_name[max_display_filename_len-4] = '.';
                onion_dict_add(file, "name_short", de->d_name, OD_DUP_VALUE);
            }else{
                onion_dict_add(file, "name_short", name, 0); // Second reference
            }
#endif
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
    goto media_share_html_end;

media_share_html_error:
    free(uri_rel_path); // In non-error case stored as
                        // => share_info["current_directory"]
media_share_html_end:  // cleanup allocations
    free(real_local_path);
    //free(uri_full_path);
    buffer_pool_release(path_buffers, __tmp_path);
    return ret;
}

onion_connection_status media_html_redirect_current(
        onion_dict *shared_folders,
        onion_request * req,
        onion_response * res)
{
    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {
        // only get implemented.
        return OCS_NOT_PROCESSED;
    }

    share_info_t *share_info = media_track_paths_get_current_share(mtp);
    if (NULL == share_info){
        //onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
        return onion_shortcut_response(
                //"{\"message\":\"No shares defined.\"}",
                "<h1>No shares defined.</h1>",
                HTTP_BAD_REQUEST, req, res);
    }

    // Consumed prefix:  "/media/html/.current[/]*", 
    char *redirect_url=NULL;
    const char *uri_rel_path = onion_request_get_path(req);
    onion_connection_status ret = OCS_NOT_PROCESSED;
#if 1
    const char *prefix = share_info_get_preferred_key(share_info);
    if (-1 < asprintf(&redirect_url, "media/html/%s/%s", prefix, uri_rel_path)) {
        ONION_DEBUG("Internal redirect to '%s'\n", redirect_url);
        /* Differences between onion_shortcut_internal_redirect and onion_shortcut_redirect:
         *   1. newurl without leading slash...
         *   2. url in decoded state (decoding step skipped) */
        ret = onion_shortcut_internal_redirect(redirect_url, req, res);
        free(redirect_url);
    }
#else
    const char *prefix = share_info->key_encoded;
    char *tmp = encodeUnixPath(uri_rel_path);
    if (-1 < asprintf(&redirect_url, "/media/html/%s/%s", prefix, tmp)) {
        ret = onion_shortcut_redirect(redirect_url, req, res);
        free(redirect_url);
    }
    free(tmp);
#endif
    return ret;
}
