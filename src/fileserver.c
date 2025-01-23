#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <string.h>
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
#include <onion/low.h>
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
#include <onion/shortcuts.h>

#include "mpv_api_commands.h"
#include "mpv_script_options.h"
#include "defines.h"
#include "tools.h"
#include "buffer_pool.h"

#include "fileserver.h"
#include "percent_encoding.h"

extern buffer_pool_t *path_buffers;

__fileserver_page_data_t *__fileserver_page_new(
        const char *root_dir)
{
    __fileserver_page_data_t *data = calloc(1, sizeof(__fileserver_page_data_t));

    data->root_dir = strdup(root_dir);

    return data;
}

void __fileserver_page_data_free (
        __fileserver_page_data_t *privdata)
{
    FREE(privdata->root_dir);
    free(privdata);
}

/**
 * Serves a directory listing or return file.
 */
onion_connection_status fileserver_page(
        __fileserver_page_data_t *privdata,
        onion_request * req,
        onion_response * res)
{

    if ((onion_request_get_flags(req) & OR_METHODS) != OR_GET) {  // only get.
        return OCS_NOT_PROCESSED;
    }

    onion_connection_status ret = OCS_NOT_PROCESSED;
#if TEMPLATES_WITH_SHORTEN_NAMES > 0
    const size_t max_display_filename_len = TEMPLATES_WITH_SHORTEN_NAMES;
#endif

    const char * const basepath = privdata->root_dir;

    char *path = strdup(onion_request_get_path(req));
    // Strip '/' to normalize 'folder/' and 'folder' paths.
    rstrip_slash(path);

    ONION_DEBUG("PATH: '%s'\n", path);
    //ONION_DEBUG("FULLPATH: '%s'\n", path);

    const size_t tmp_path_len = path_buffers->buf_len;
    char * const dir_path = buffer_pool_aquire(path_buffers);
    char * const tmp_path2 = buffer_pool_aquire(path_buffers);
    char *realp = NULL;

    int ps = snprintf(dir_path, tmp_path_len, "%s/%s", basepath, path);
    if (ps < 0 || ps >= tmp_path_len){
        ret = OCS_INTERNAL_ERROR;  // snprintf failed
        goto fileserver_page_end;
    }

    realp = realpath(dir_path, NULL);
    ONION_DEBUG("REALPATH: '%s'\n", realp);
    if (!realp){
        ret = OCS_INTERNAL_ERROR;
        goto fileserver_page_end;
    }

    /* List directory. Otherwise the request will not handled here,
     * but the static file content returned, see below.*/
    DIR *dir = opendir(realp);
    if (dir) {                  // its a dir, fill the dictionary.
        onion_dict *d = onion_dict_new();
        onion_dict_add(d, "path", path, 0);
        if (path[0] != '\0' && path[1] != '\0')
            onion_dict_add(d, "go_up", "true", 0);
        onion_dict *files = onion_dict_new();
        onion_dict_add(d, "files", files, OD_DICT | OD_FREE_VALUE);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
        onion_dict_add(d, "path_encoded",
                encodeURIComponent(path), OD_FREE_VALUE);
#endif

        struct dirent *de;
        while ((de = readdir(dir))) {     // Fill one files.[filename] per file.
            // Skip ".", ".." and hidden files in listing
            if ( de->d_name[0] == '.' ) continue;

            onion_dict *file = onion_dict_new();
            onion_dict_add(files, de->d_name, file,
                    OD_DUP_KEY | OD_DICT | OD_FREE_VALUE);

            const char *name = onion_low_strdup(de->d_name);
            onion_dict_add(file, "name", de->d_name, OD_FREE_VALUE);
#ifdef TEMPLATES_WITH_ENCODED_NAMES
            // Encode filename
            onion_dict_add(file, "name_encoded",
                    encodeURIComponent(de->d_name), OD_FREE_VALUE);
#endif

            struct stat st;
            int ps = snprintf(tmp_path2, tmp_path_len, "%s/%s", realp, de->d_name);
            if( ps >= 0 && ps < tmp_path_len && 0 == stat(tmp_path2, &st))
            {

                ps = snprintf(tmp_path2, tmp_path_len, "%d", (int)st.st_size);
                if( ps >= 0 && ps < tmp_path_len ){
                    onion_dict_add(file, "size", tmp_path2, OD_DUP_VALUE);
                }else{
                    onion_dict_add(file, "size", "snprintf fails", OD_DUP_VALUE);
                }

                ps = snprintf(tmp_path2, tmp_path_len, "%d", st.st_uid);
                if( ps >= 0 && ps < tmp_path_len ){
                    onion_dict_add(file, "owner", tmp_path2, OD_DUP_VALUE);
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
                onion_dict_add(file, "owner", "?", OD_DUP_VALUE);
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
                onion_mime_get("fileserver.html"));
        ret = fileserver_html_template(d, req, res); // this frees d!
        goto fileserver_page_end;
    }

    // Return file
    onion_response_set_header(res, "Content-Type", onion_mime_get(realp));
    ret = onion_shortcut_response_file(realp, req, res);

fileserver_page_end:  // cleanup allocations
    buffer_pool_release(path_buffers, dir_path);
    buffer_pool_release(path_buffers, tmp_path2);
    free(realp);
    free(path);
    return ret;
}


void webui_onion_static_files(
        const char *root_dir,
        const char *pattern,
        onion_url *urls)
{
    __fileserver_page_data_t *privdata = __fileserver_page_new(
            root_dir);

    onion_handler *static_file_handler = onion_handler_new(
            (onion_handler_handler) fileserver_page,
            privdata,
            (onion_handler_private_data_free) __fileserver_page_data_free);

    onion_url_add_handler(urls, pattern, static_file_handler);
}
