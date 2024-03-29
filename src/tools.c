#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "tools.h"

int log_debug = 1;

static int inline split_options_valid_key(const char *keyname,
        const char *prefix, size_t len_prefix){
    if (0 == strcmp(keyname, ""))
        return 0;

    if (0 != strncmp(keyname, prefix, len_prefix))
        return 0;

    return 1;
}

/* Split args like  key1=val1[,;]key2=val2,
 * but ignore escaped variants of chars.
 *
 * [return value] Array of (key, val)-pairs. Option with empty key marks end.
 */
option_t *split_options(const char *prefix, const char *_script_opts){

    if (prefix == NULL) prefix = "";
    if (_script_opts == NULL) _script_opts = "";

    char *script_opts = strdup(_script_opts); // working copy
    size_t len_script_opts = strlen(script_opts);
    size_t len_prefix = strlen(prefix);
    size_t num_opts = 0;

    // (over-)estimate number of options
    if (len_script_opts > 0){ ++num_opts; }
    char *opt_end = strpbrk(script_opts, ",;");
    while( opt_end ){
        ++num_opts;
        opt_end = strpbrk(opt_end+1, ",;");
    }

    option_t *parsed_opts = (option_t *) calloc(num_opts + 1, sizeof(option_t));

    if (num_opts == 0){
        free(script_opts);
        return parsed_opts; // [ NULL, NULL ]
    }

    size_t i_opt = 0;  // < num_opts

    char *tok_start = script_opts;
    char *tok_end = strpbrk(script_opts, ",=;\\");

    // False for empty key ('=foo') or key not begins with 'prefix'
    int bValidKey = 0;

    while (tok_end) {

        // Handle escaped chars
        if( *tok_end == '\\' ){
            if (*(tok_end+1) == '\0'){ // String end reached
                //*tok_end = '\0';  // overwrite non-conforming escape char?!
                tok_end = NULL;
            }else{
                // Ignore this backslash by shifting the current string
                // one character backwards.
                for( char *c=tok_end; c>tok_start; --c){
                    *c = *(c-1);
                }
                *tok_start = '\0';
                ++tok_start;
                // Search again for token, but ignore if hit directly behind '\\'
                tok_end = strpbrk(tok_end+2, "=,;\\");
            }
            continue;
        }

        // Handle part before '=' (key)
        if (*tok_end == '=') {
            *tok_end = '\0';
            bValidKey = split_options_valid_key(tok_start,
                    prefix, len_prefix);
            if (bValidKey) {
                assert(parsed_opts[i_opt].key == NULL);
                parsed_opts[i_opt].key = strdup(tok_start + len_prefix);
            }

            // Prepare next token/loop
            tok_start = tok_end + 1;
            tok_end = strpbrk(tok_start, ",;\\"); //skip extra '='
            continue;
        }

        // Handle part after '=' (value)
        if (*tok_end == ';' || *tok_end == ',') {
            *tok_end = '\0';
            if( parsed_opts[i_opt].key != NULL ){
                if (bValidKey) {
                    parsed_opts[i_opt].val = strdup(tok_start);
                    ++i_opt;
                }
            }else{
                bValidKey = split_options_valid_key(tok_start,
                        prefix, len_prefix);
                if (bValidKey) {
                    parsed_opts[i_opt].key = strdup(tok_start + len_prefix);
                    parsed_opts[i_opt].val = strdup("yes");
                    ++i_opt;
                }
            }
        }

        // Prepare next token/loop
        tok_start = tok_end + 1;
        tok_end = strpbrk(tok_start, "=,;\\");
        continue;
    }

    // Handle part after last split token.
    if( parsed_opts[i_opt].key != NULL ){
        if (bValidKey) {
            parsed_opts[i_opt].val = strdup(tok_start);
            ++i_opt;
        }
    }else{
        bValidKey = split_options_valid_key(tok_start,
                prefix, len_prefix);
        if (bValidKey) {
            parsed_opts[i_opt].key = strdup(tok_start + len_prefix);
            parsed_opts[i_opt].val = strdup("yes");
            ++i_opt;
        }
    }


    free(script_opts);
    return parsed_opts;
}

/* Split args like  [optkey1<{:|}>]val1[<{,;}>[optkey2]<{:|}>]val2[...]],
 * but ignore escaped variants of chars.
 *
 * [return value] Array of (key, val)-pairs.
 */
option_t *split_shared_folder_str(const char *options){

    if (options == NULL) options = "";
    int len_options = strlen(options);

    // 1. Prepare output array
    size_t num_opts = 0;
    // (over-)estimate number of options
    if (len_options > 0){ ++num_opts; }
    char *opt_end = strpbrk(options, ",;");
    while( opt_end ){
        ++num_opts;
        opt_end = strpbrk(opt_end+1, ",;");
    }

    option_t *parsed_opts = (option_t *) calloc(num_opts + 1, sizeof(option_t));
    if (num_opts == 0){
        return parsed_opts; // [ NULL, NULL ]
    }
    size_t i_opt = 0;  // < num_opts

    // 2. Fill output array
    char *key=NULL, *value=NULL;
    char delim[2] = "?";
    int n1=-1; // consumed chars after first token
    int n2=-1; // consumed chars after second token

    while(len_options > 0){

        /*
         *  %m[^|:,;]  First token (RegEx [^|:,;]* )
         *  %n         Position after first token
         *  %1[|:,;]   One char of the delimeters
         *  %m[^,;]    Second token
         *  %n         Position after thrid token
         * */
        int found = sscanf(options, "%m[^|:,;]%n%1[|:,;]%m[^,;]%n",
                &key, &n1, delim,
                &value, &n2);

        if (found >= 2){
            if (delim[0] == ',' || delim[0] == ';' ){
                // 'value'-case: no key given, but value
                free(value); value = key;
                key = strdup(""); // no key
                n2 = n1;  /* otherwise n2 is unset (string end)
                             or too big (next token pair consumed */
            }else if( value == NULL || value[0] == '\0'){
                // 'key|'-case: key, but value empty/wrong
                free(value);
                value = NULL; //strdup("no value");
                n2 = n1;  /* otherwise n2 is unset (string end)
                             or too big (next token pair consumed */
            }else{
                assert(found>=3);
            }

        }else if (found == 1) {
            // 'value'-case for latest entry
            value = key;
            key = strdup(""); // no key
        }

        //printf("\tShared folders | Key: '%s', '%s', Value: '%s'\n", key, delim, value);

        if ( key != NULL && value != NULL ){
            parsed_opts[i_opt].key = strdup(key);
            parsed_opts[i_opt].val = strdup(value);
            ++i_opt;
        }

        // Prepare next loop
        free(key); key = NULL;
        free(value); value = NULL;

        if (found == EOF || found == 0 || n2 <= 0){
            // Probably redundant, but secures parsing
            break;
        }
        //printf("\n");

        len_options -= n2;
        options += n2;
        while( options[0] == ',' || options[0] == ';'
                || options[0] == '|' || options[0] == ':'){
            ++options;
            --len_options;
        }
        n1 = -1;
        n2 = -1;

    }

    return parsed_opts;
}

void print_options(const option_t * const opts){
    const option_t *loop = opts;
    while( loop->key != NULL ){
        printf("Key: '%s' Value: '%s'\n", loop->key, loop->val);
        ++loop;
    }
}

void free_options(option_t * const opts){
    option_t *loop = opts;
    while( loop->key != NULL){
        free(loop->key);
        free(loop->val);
        ++loop;
    }
    free(opts);
}

int check_int_or_float(const char *param, char **msg){
    double dbl;
    int n;

    int is_int = (1 == sscanf( param, "%d", &n));
    if( is_int ) return 1;

    int is_dbl = (1 != sscanf( param, "%lf", &dbl));
    if( is_dbl ) return 2;

    // Parsing fails
    if (msg != NULL ){
        if( *msg != NULL) { free(*msg); *msg = NULL; }
        *msg = strdup("Parameter needs to be an integer or float.");
    }
    return 0;
}

int check_valid_param(const char *mode,
        const char **valid_list,
        char **msg){
    // Return 1 if 'mode' is in 'valid_list' */
    while( *valid_list != NULL ){
        if( 0 == strcmp(mode, *valid_list) ) return 1;
        ++valid_list;
    }

    if (msg != NULL ){
        if( *msg != NULL) { free(*msg); *msg = NULL; }
        *msg = strdup("Invalid parameter.");
    }
    return 0;
}

int path_available (const char *path){
    // strstr maybe overblocking some unicode paths.
    if (strstr(path, "/.")){
        return 0;
    }
#if 0 //redundant
    if (strstr(path, "/../")){
        return 0;
    }
#endif
  return 1;
}

void strip_slash(
        char *path)
{
    size_t p = strlen(path);
    while(p > 0 && path[p-1] == '/'){
        path[p-1] = '\0';
        --p;
    }
}

char *add_slash_if_notempty(
        const char *path)
{
   size_t path_len = strlen(path);
   if (path_len == 0) return strdup("");

   char *ret = malloc(sizeof(char) * (path_len+2)); // +2 for '/' and '\0'
   char *end = stpcpy(ret, path);
   *end++ = '/'; *end = '\0';

   return ret;
}

int check_mpv_err(
        int status)
{
    if( status < MPV_ERROR_SUCCESS ){
        printf("mpv API error %d: %s\n", status,
                mpv_error_string(status));
    }
    return status; // == 0 for MPV_ERROR_SUCCESS
}
