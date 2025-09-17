
onion_connection_status media_api_loadfile(
        __share_data_media_api_t *privdata,
        onion_request * req,
        onion_response * res)
{

    onion_connection_status ret = OCS_NOT_PROCESSED;

    // 0. Change content type
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

    // Note: Some arguments of fullpath are already consumed.
    // Do not parse (parts of) fullpath for portability,
    // but look back into 'privdata'.
    char *uri_rel_path = strdup(consume_leading_slashs(onion_request_get_path(req)));
    // Example: 'folder/subfolder/file.mp3'

    //const char * share_key = onion_dict_get((onion_dict *)privdata->share_info, "key");
    // Example: 'example'

    const char * share_path = onion_dict_get((onion_dict *)privdata->share_info, "path");
    // Example: '/some/dir'

    size_t __tmp_path_len = path_buffers->buf_len;
    char *__tmp_path = buffer_pool_aquire(path_buffers);

    // Get command
    CommandHandler cmd = privdata->cmd;
    //CommandHandler data->cmd = (CommandHandler) onion_dict_get(
    //        media_commands, priv_data->command_name);

    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
    if (!cmd){
        onion_response_set_code(res, HTTP_BAD_REQUEST);
        onion_response_printf(res,
                "{\"message\": \"No command defined for '%s'.\"}", privdata->command_name);
        ret = OCS_PROCESSED;
        goto media_api_loadfile_end;
    }

    /* Check data */
    if( uri_rel_path == NULL || strlen(uri_rel_path) == 0 ){
        ret = onion_shortcut_response(
                "{\"message\":\"Adding of whole share not supported.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto media_api_loadfile_end;
    }

    int ps = snprintf(__tmp_path, __tmp_path_len,
            "%s/%s",
            share_path, uri_rel_path);
    if (ps < 0 || ps >= __tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto media_api_loadfile_end;
    }

    /* Evaluate real path of share directory and file */
    if (NULL != strstr(__tmp_path, "/.")){
        ret = onion_shortcut_response(
                "{\"message\":\"Invalid path.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto media_api_loadfile_end;
    }

    if( access( __tmp_path, R_OK ) == -1 ) {
        // file doesn't exist or is not readable
        ret = onion_shortcut_response(
                "{\"message\":\"File not readable.\"}",
                HTTP_BAD_REQUEST, req, res);
        goto media_api_loadfile_end;
    }

    /* Handle data */

    // Run command
    char *output_message = NULL;
    pthread_mutex_lock(&mpv_lock);
    int cmd_status = (cmd)(privdata->command_name, privdata->arg1, __tmp_path, &output_message);
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


media_api_loadfile_end:  // cleanup allocations
    free(uri_rel_path);
    buffer_pool_release(path_buffers, __tmp_path);

    return ret;
}

