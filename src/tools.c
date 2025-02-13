#define _GNU_SOURCE // for strchrnul()

#define _LARGE_FILES // For longer file size on 32bit systems (e.g. armv7l)
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>

#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <dirent.h>

#include <onion/low.h>

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

int parse_int(const char *param, int len_param, int *out){
    int n = 0;

    /* %d      : Integer with leading space
     * %n      : Saves number of consumed chars at this position.
     * %*[ \t] : Space characters without output variable.
     *           Do not influence return value of sscanf
     *           In difference to %s it only matches nonempty sequence
     * %n      : Saves number of consumed chars at this position.
     *
     *
     * Two %n position needed to catch empty and nonempty sequences of
     * following whitespace chars.
     */
    const char *pattern = "%d%n%*[ \t]%n";
    int end_pos1 = -1;
    int end_pos2 = -1;
    int string_starts_as_int = (1 == sscanf(param, pattern, &n, &end_pos1, &end_pos2));
    int whole_input_consumed = (end_pos1 == len_param || end_pos2 >= len_param);
    if (string_starts_as_int && whole_input_consumed) {
        if (out != NULL) *out = n;
        return 1;
    }

    return 0;
}

int parse_float(const char *param, int len_param, double *out){
    double dbl = 0.0;

    /* %lf     : Double sized float
     * %n      : Saves number of consumed chars at this position.
     * %*[ \t] : Space characters without output variable.
     *           Do not influence return value of sscanf
     *           In difference to %s it only matches nonempty sequence
     * %n      : Saves number of consumed chars at this position.
     *
     *
     * Two %n position needed to catch empty and nonempty sequences of
     * following whitespace chars.
     */
    const char *pattern = "%lf%n%*[ \t]%n";
    int end_pos1 = -1;
    int end_pos2 = -1;
    int string_starts_as_float = (1 == sscanf(param, pattern, &dbl, &end_pos1, &end_pos2));
    int whole_input_consumed = (end_pos1 == len_param || end_pos2 >= len_param);
    if (string_starts_as_float && whole_input_consumed) {
        if (out != NULL) *out = dbl;
        return 1;
    }

    return 0;
}

int check_int_or_float(const char *param, char **msg){
    char *end = strchrnul(param, '/'); // position of next '/' or '\0'.
    int len_param = end-param; // >= 0
    if (len_param <= 0) return 0;

    if (parse_int(param, len_param, NULL)) return 1;
    if (parse_float(param, len_param, NULL)) return 2;

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

int rstrip_slash(
        char *path)
{
    if (*path == '\0') return 0;
    size_t p = strlen(path); // >= 1
    char *last_char = path+p-1;
    if (*last_char != '/') return 0;
    do {
        *last_char-- = '\0';
    }while(*last_char == '/' && last_char >= path);
    return 1;
}

char *left_slashed_string_if_not_empty(
        const char *path)
{
    if (path[0] == '\0' || path[0] == '/' && path[1] == '\0' ){
        return onion_low_calloc(1, sizeof(char));
    }
    // strip all leading /
    while (path[0] == '/') { ++path; };

    // Copy plus space for prefix '/'
    char *ret = onion_low_strdup(path-1);
    ret[0] = '/';

    // Remove tailing. (Can still lead to empty string)
    rstrip_slash(ret);

    return ret;
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

char *add_slash_if_next_notempty(
        const char *path,
        const char *next)
{
   if (*next == '\0') return strdup(path);
   size_t path_len = strlen(path);

   char *ret = malloc(sizeof(char) * (path_len+2)); // +2 for '/' and '\0'
   char *end = stpcpy(ret, path);
   *end++ = '/'; *end = '\0';

   return ret;
}

// Convert a₁a₂… into [a₁][a₂]…
// Does not respect utf-8...
char *regex_encapsule_chars(
        const char *word)
{
   size_t path_len = strlen(word);
   if (path_len == 0) return strdup("");

   char *ret = malloc(sizeof(char) * (3*path_len+1)); // +1 and '\0'
   if (ret == NULL) return NULL;

   size_t i, j;
   for (i=0,j=0; i<path_len; ++i){
       ret[j++] = '[';
       ret[j++] = word[i];
       ret[j++] = ']';
   }
   ret[j] = '\0';

   return ret;
}


const char *consume_leading_slashs(
        const char *in)
{
    if(in == NULL) return NULL;
    while (*in == '/' ){
        ++in;
    }
    return in;
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

// "shareX" or "/X"
char *enumerated_name(
        int n)
{
    char _name[20]; // share1, etc
    int written = snprintf(_name, sizeof(_name),
            "%s%d", "share", n);
    if (written > 0 && written < sizeof(_name)){
        return strdup(_name);
    }
    return NULL;
}

int strstarts(
        const char *str,
        const char *prefix)
{
     return strncmp(str, prefix, strlen(prefix));
}

int check_is_url(
        const char *url_or_path)
{
    if (0 == strstarts(url_or_path, "https://") ||
            0 == strstarts(url_or_path, "http://"))
        return 0;

    return -1;
}

int check_is_non_hidden_file(
        const char *path)
{
    /* Check if resolveable local path */
    char *realp = NULL;
    realp = realpath(path, NULL);
    if (!realp){
        return -1;
    }

    /* Check if hidden */
    if( strlen(realp) == 0 || strstr(realp, "/.") != NULL ){
        free(realp);
        return -3;
    }

    /* Check if dir */
    DIR *dir = opendir(realp);
    if (dir) {
        closedir(dir);
        free(realp);
        return -2;
    }

    return 0;
}

// Size in binary units
// Define _LARGE_FILES (and or D_FILE_OFFSET_BITS 64 ?!) for big files on 32bit Systems, and use off_t type, see
int print_filesize_BIN64(char *buf, size_t buf_size, off_t num_bytes){
    // num_bytes is signed. Interpret errors(directories?!) like zero bytes.
    num_bytes = (num_bytes < 0)?0:num_bytes;
    int ps;

    if( num_bytes > (1 << 30)){
        ps = snprintf(buf, buf_size, "%4lld.%03lld GiB",
                (num_bytes >> 30),
                1000 * ((num_bytes >> 20) & ((1 << 10)-1)) >> 10 ); /* Next 10 Bits scaled by 1000/1024 */
    }else if( num_bytes > (1 << 20)){
        ps = snprintf(buf, buf_size, "%4lld.%03lld MiB",
                (num_bytes >> 20),
                1000 * ((num_bytes >> 10) & ((1 << 10)-1)) >> 10 ); /* Next 10 Bits scaled by 1000/1024 */
    }else if( num_bytes >= (1 << 10)){
        ps = snprintf(buf, buf_size, "%4lld.%03lld KiB",
                (num_bytes >> 10),
                1000 * ((num_bytes >> 0) & ((1 << 10)-1)) >> 10 ); /* Next 10 Bits scaled by 1000/1024 */
    }else{
        ps = snprintf(buf, buf_size, "%u", num_bytes);
    }
    return ps;
}

// Size in SI units
int print_filesize_SI32(char *buf, size_t buf_size, int num_bytes){
    int ps;
    if( num_bytes > 1E9 ) {
        ps = snprintf(buf, buf_size,
                "%.1f GB", ((double)num_bytes)/1.0E9 );
    } else if( num_bytes > 1E6 ) {
        ps = snprintf(buf, buf_size,
                "%.1f MB", ((double)num_bytes)/1E6 );
    } else if( num_bytes > 1E3 ) {
        ps = snprintf(buf, buf_size,
                "%.1f kB", ((double)num_bytes)/1E3 );
    } else {
        ps = snprintf(buf, buf_size,
                "%d B", num_bytes);
    }
    return ps;
}
