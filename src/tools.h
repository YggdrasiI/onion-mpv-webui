#pragma once

#include <mpv/client.h>

extern int log_debug;
#define LOG(...) { if (log_debug) { printf(__VA_ARGS__); } }

typedef struct option_t{
  char *key;
  char *val;
} option_t;

/* Returns
 *   [[ "key1", "val1" ]]
 *     … … …
 *   [[ NULL, NULL ]]  <- Empty key marks end
 *
 * Do not set val for empty keys (memory could leak.)
 */
option_t *split_options(
        const char *prefix,
        const char *_script_opts);


/* Split args like  [optkey1<{:|}>]val1[<{,;}>[optkey2]<{:|}>]val2[...]],
 *
 * Returns
 *   [[ "key1", "val1" ]]
 *     … … …
 *   [[ NULL, NULL ]]  <- Empty key marks end
 *
 * Do not set val for empty keys (memory could leak.)
 *
 * Example input:
 *   music:$HOME/Music;/some/other/path/albumX
 * Expamle output:
 *   [["music", "$HOME/Music"],
 *   ["", "/some/other/path/albumX"]]
 *
 */
option_t *split_shared_folder_str(
        const char *options);

void print_options(
        const option_t * const opts);

void free_options(
        option_t * const opts);

/* Checks if whole input string is Integer.
 *
 * Return value: 0 - No int
 *               1 -    Int
 *
 * Params:
 *      param: input , null terminated, but strlen(param) > len_param is ok.
 *  len_param: <= strlen(param). Range to check
 *       *out: parsed value, if out != NULL
 * */
int parse_int(
        const char *param,
        int len_param,
        int *out);

/* Checks if whole input string is Double.
 *
 * Return value: 0 - No double
 *               1 -    Double
 *
 * Params:
 *      param: input , null terminated, but strlen(param) > len_param is ok.
 *  len_param: <= strlen(param). Range to check
 *       *out: parsed value, if out != NULL
 * */
int parse_float(
        const char *param,
        int len_param,
        double *out);

int check_int_or_float(
        const char *param,
        char **msg);

int check_valid_param(
        const char *mode,
        const char **valid_list,
        char **msg);

/*
 * Returns 0 if one component of the path begins with '.'
 */
int path_available(
        const char *path);

/*
 * Remove pending '/' chars.
 *
 * Return 0 if path was not altered, and 1 otherwise.
 */
int rstrip_slash(
        char *path);

/* Normalize path/subfolder names:  "" or "/path…"
 *
 * ATTENTION: This function assumes that path points inside of bigger string and path[-1] exists.
 *
 * X := [^/]+
 * ""   => ""
 * "[/]+"  => ""
 * "[/]+X" => "/X"
 * "X"  => "/X"
 * "X[/]+" => "/X"
 */
char *left_slashed_string_if_not_empty(
        const char *path);
    
/*
 * Returns new string 'path/' if path is non-empty and '' otherwise.
 */
char *add_slash_if_notempty(
        const char *path);

char *add_slash_if_next_notempty(
        const char *path,
        const char *next);

char *regex_encapsule_chars(
        const char *word);

const char *consume_leading_slashs(
        const char *in);

/*
 */
int check_mpv_err(
        int status);

char *enumerated_name(
        int n);

int strstarts(
        const char *str,
        const char *prefix);

/* Checks if string begins with "(http|https)://"
 * (To rule out file:// and other potential problematic ways
 * to access arbitary files on the host system)
 */
int check_is_url(
        const char *url_or_path);

/* Checks if given path links to a file,
 * is not hidden nor placed in a hidden folder.
 */
int check_is_non_hidden_file(
        const char *path);

// File size in binary units (B, KiB, MiB, GiB) and for large files.
int print_filesize_BIN64(char *buf, size_t buf_size, off_t num_bytes);

// File size in SI units (B, kB, MB, GB) and just for 32 bit numbers.
int print_filesize_SI32(char *buf, size_t buf_size, int inum_bytes);
