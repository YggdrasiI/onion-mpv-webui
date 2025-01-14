
// Begin page /media.html, /media/html lists all shares
void __invert_key_value(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    onion_dict_add(to_json, value, key, 0);
}

int media_html_root(
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
// End page /media.html, /media/html lists all shares



// For /media/html/{share name}[/]*
//     =>  Template file: media_share.html
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
#if TEMPLATES_WITH_SHORTEN_NAMES > 0
    const size_t max_display_filename_len = TEMPLATES_WITH_SHORTEN_NAMES;
#endif

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
        onion_dict_add(d, "dirname", basename(uri_full_path), // GNU's basename
                OD_DUP_VALUE|OD_FREE_VALUE);
        //onion_dict_add(d, "path", uri_rel_path, 0); // Not used, but key "uri_rel_path"
        //onion_dict_add(d, "uri", uri_full_path, 0);
        onion_dict_add(d, "page_title", onion_dict_get(options, "page_title"), 0);

        // Add "Go up" link. Using uri_rel_path because
        // uri_full_path is 'media/html/[share_name]'
        //if (uri_rel_path[0] != '\0' && uri_rel_path[1] != '\0')
        if (1)
            onion_dict_add(d, "go_up", "true", 0);

        onion_dict_add(d, "share_key", share_key, 0);
        onion_dict_add(d, "uri_rel_path", uri_rel_path, 0);
        // TODO Adding slashes was no good idea. Just use normalized paths without
        // slashes and add slashes in template.
        //onion_dict_add(d, "share_key", add_slash_if_notempty(share_key),
        //    OD_FREE_VALUE);
        //onion_dict_add(d, "uri_rel_path", add_slash_if_notempty(uri_rel_path),
        //    OD_FREE_VALUE);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
        onion_dict_add(d, "uri_rel_path_encoded",
                encodeURIComponent(uri_rel_path), OD_FREE_VALUE);
        //onion_dict_add(d, "uri_full_path_encoded",
        //        encodeURIComponent(uri_full_path), OD_FREE_VALUE);
#endif

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
#if TEMPLATES_WITH_SHORTEN_NAMES
            // Add shorten name to avoid long lines
            if (strlen(de->d_name) > max_display_filename_len) {
                de->d_name[max_display_filename_len-1] = '\0';
                de->d_name[max_display_filename_len-2] = '.';
                de->d_name[max_display_filename_len-3] = '.';
                de->d_name[max_display_filename_len-4] = '.';
            }
            onion_dict_add(file, "name_short", de->d_name, OD_DUP_VALUE);
#endif
#ifdef TEMPLATES_WITH_ENCODED_NAMES
            // Encode filename
            onion_dict_add(file, "name_encoded",
                    encodeURIComponent(de->d_name), OD_FREE_VALUE);
#endif


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
