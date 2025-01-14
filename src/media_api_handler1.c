// media_api_list_root
// For /media/api/list[/]*
//     =>  Json-reply generated from dict.

// media_api_list
// For /media/api/list/{sharename}[/{folder}[/]*]
//     =>  Template file: media_api_list.json
//     /media/api/list/{sharename}/{path_to_file}
//     TODO: Remove file download over api-URI. Its redunant to /media/html/...
onion_connection_status media_api_list(
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
    }

    // Holds file/folder destination of 'share_path/uri_rel_path', etc
    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);
    // Example: '/some/dir/folder/subfolder'

    // To resolve symbolic links/relative paths.
    char *real_local_path = NULL;

    // Strip '/' to normalize 'folder/' and 'folder' paths.
    strip_slash(uri_full_path);
    printf("BEFORE %s\n", uri_rel_path);
    strip_slash(uri_rel_path);
    printf("AFTER  %s\n", uri_rel_path);

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto media_api_list_json_end;
    }

    if ( !path_available(__tmp_path) ){
        /* File or parent folders is hidden .*/
        ret = OCS_INTERNAL_ERROR; 
        goto media_api_list_json_end;
    }

    real_local_path = realpath(__tmp_path, NULL);
    if (!real_local_path){
        ret = OCS_INTERNAL_ERROR;
        goto media_api_list_json_end;
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
        // TODO Adding slashes was no good idea. Just use normalized paths without
        // slashes and add slashes in template.
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
        ret = media_api_list_json_template(d, req, res); // this frees d!
        goto media_api_list_json_end;
    }

    // 3. Return file or print access denined message
    const char *ad = onion_dict_get(options, "allow_download_in_shares");
    if (ad && ad[0] != '0') {
        onion_response_set_header(res, "Content-Type", onion_mime_get(real_local_path));
        ret = onion_shortcut_response_file(real_local_path, req, res);
        goto media_api_list_json_end;
    }else{
        onion_response_set_header(res, "Content-Type",
                onion_mime_get("_.json"));
        ret = onion_shortcut_response(
                "{\"message\": \"Download not allowed\"}",
                HTTP_FORBIDDEN, req, res);
        goto media_api_list_json_end;
    }

media_api_list_json_end:  // cleanup allocations
    free(real_local_path);
    free(uri_rel_path);
    free(uri_full_path);
    buffer_pool_release(path_buffers, __tmp_path);
    return ret;
}

// Begin json /media/api/list json of all shares
void __list_share_func(
        onion_dict *to_json,
        const char *key, const void *value, int flags)
{
    char *api_path;
    asprintf(&api_path, "/media/api/list/%s", key);
    onion_dict_add(to_json, key, api_path, OD_FREE_VALUE);
}

int media_api_list_root(
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

