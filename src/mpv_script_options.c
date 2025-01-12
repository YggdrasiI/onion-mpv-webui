#define _GNU_SOURCE

#include <stdio.h>
#include <assert.h>

#include <onion/log.h>

#include "defines.h"
#include "mpv_script_options.h"
#include "mpv_api_commands.h"
#include "tools.h"

onion_dict *get_default_options(){
  onion_dict *opt = onion_dict_new();
  onion_dict *shared_folders = onion_dict_new();

  onion_dict_add(opt, "port", "9000", 0);
  onion_dict_add(opt, "hostname", "::", 0);
  onion_dict_add(opt, "root_dir", "./webui-page", 0);
  onion_dict_add(opt, "page_title", "MPV Webui", 0);

  onion_dict_add(opt, "paused", "1", 0);
  onion_dict_add(opt, "daemon", "1", 0);

  onion_dict_add(opt, "ws_interval", "500", 0); // minimal distance between property updates

  // Enable console output
  onion_dict_add(opt, "debug", "0", 0);

  // Restrict loading of files on local files from shares
  // and URI's.
  onion_dict_add(opt, "block_non_shared_files", "1", 0);

  // Media shares. Names should not contain '/'.
  //onion_dict_add(shared_folders, "share1", "/home/olaf/Music",
  //        OD_FREE_ALL|OD_DUP_ALL);

  onion_dict_add(opt, "shared_folders", shared_folders,
          OD_FREE_VALUE|OD_DICT); // free sub-dict with opt

  onion_dict_add(opt, "allow_download_in_shares", "0",
          OD_FREE_ALL|OD_DUP_ALL);

  return opt;
}

/*void read_options(const char *conf_file,
        onion_dict *opt){
}

void write_options(const char *conf_file,
        onion_dict *opt){
}*/

void transfer_options(const option_t *opts, onion_dict *dict){
    while( opts->key ){
        if (NULL == onion_dict_get(dict, opts->key)){
            ONION_WARNING("script-opts key '%s' has no effect.", opts->key);
        }else{
            onion_dict_add(dict, opts->key, opts->val,
                    OD_DUP_ALL|OD_FREE_ALL|OD_REPLACE);
        }
        ++opts;
    }
}


int __update_option(onion_dict *options, const char *plugin_prefix,
        const char *prefix_and_key, const char *value){

    if (plugin_prefix == NULL ){
        plugin_prefix = "";
    }
    if (value == NULL ){
        value = "";
    }
    if (!(prefix_and_key &&
                prefix_and_key == strstr(prefix_and_key, plugin_prefix) )) {

        return -1;
    }

    // prefix_and_key begins with prefix
    const char *key = prefix_and_key + strlen(plugin_prefix);

    if (NULL == onion_dict_get(options, key)){
        ONION_WARNING("script-opts key '%s' has no effect.", key);
        return -1;
    }

    onion_dict_add(options, key, value,
            OD_DUP_ALL|OD_FREE_ALL|OD_REPLACE);

    return 0;
}

/* Transfers content of 'libwebui-shared_folders-option'
 * into 'options.shared_folders'-dict
 * */
int __update_shared_folders(onion_dict *options,  const char *value,
        int extend_existing_list){

    onion_dict *shared_folders;

    if (extend_existing_list == 0){
        // Remove previous/default entries
        /*onion_dict_free(shared_folders);*/
        shared_folders = onion_dict_new();
        onion_dict_add(options, "shared_folders", shared_folders,
                OD_FREE_VALUE|OD_DICT|OD_REPLACE);
    }else{
        shared_folders = onion_dict_get_dict(options, "shared_folders");
    }

    option_t *opt_tuples = split_shared_folder_str(value);
    //print_options(opt_tuples);

    option_t *opts = opt_tuples;
    int n_share = 1 + onion_dict_count(shared_folders);
    // TODO: n_share is to big if 'name' and 'shareX' map on same folder.

    while( opts->key ){
        assert( opts->key != NULL );
        assert( opts->val != NULL );

        if (opts->key[0] != '\0'){
            // L.1 Add share over key name
            onion_dict_add(shared_folders, opts->key, opts->val,
                    OD_DUP_ALL|OD_FREE_ALL|OD_REPLACE);
        }else{
            // L.2 Add share over enumerated name
            char enumerated_name[20]; // share1, etc
            int written = snprintf(enumerated_name, sizeof(enumerated_name),
                    "%s%d",
                    "share", n_share);
            if (written > 0 && written < sizeof(enumerated_name)){
                onion_dict_add(shared_folders, enumerated_name, opts->val,
                        OD_DUP_ALL|OD_FREE_ALL|OD_REPLACE);
            }
        }
        ++n_share;
        ++opts;
    }
    free_options(opt_tuples);

    return 0;
}



void update_options(
        mpv_handle *mpv,
        const char *mpv_plugin_name,
        onion_dict *options)
{

#if WITH_MPV

#define xstr(s) str(s)
#define str(s) #s

// Loop over options to update default values
    mpv_node result;
    if (mpv_get_property(mpv, "options/script-opts",
                MPV_FORMAT_NODE, &result) < 0)
    { 
       ONION_ERROR("Option not defined?!\n");
       return;
    }
    //printf("format=%d\n", (int)result.format);

    if (result.format != MPV_FORMAT_NODE_MAP){
        ONION_DEBUG("mpv property has unexpected format.\n");
        return;
    }
    
    // Loop over list of options
    for (int n = 0; n < result.u.list->num; n++) {
        const char *key = result.u.list->keys[n];
        const char *value = "";
        //printf("  Key: %s\n", key);

        mpv_node *value_node; 
        value_node = &result.u.list->values[n];
        if (value_node->format != MPV_FORMAT_STRING){
            continue;
        }
        value = value_node->u.string;
        //printf("Value: %s\n", value);

        // Special handling for shared_folders option
        char *shared_folder_key;
        int n_bytes = asprintf(&shared_folder_key, "%s-%s", mpv_plugin_name, "shared_folders");
        if (n_bytes > 0 && 0 == strncmp(shared_folder_key, key, (size_t) n_bytes))
        {
            __update_shared_folders(options, value, 0);
            FREE(shared_folder_key);
            continue;
        }
        FREE(shared_folder_key);

        // Default case
        char *key_prefix;
        n_bytes = asprintf(&key_prefix, "%s-", mpv_plugin_name);
        if( n_bytes > 0 ){
            __update_option(options, xstr(MPV_PLUGIN_NAME) "-", key, value);
        }
        FREE(key_prefix);
    }

    mpv_free_node_contents(&result); // correct free'ing variant??

#endif

}
