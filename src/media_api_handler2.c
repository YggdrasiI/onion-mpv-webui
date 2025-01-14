
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

    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
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
