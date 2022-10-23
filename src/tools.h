#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mpv/client.h>

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
 */
void strip_slash(
        char *path);

/*
 * Returns new string 'path/' if path is non-empty and '' otherwise.
 */
char *add_slash_if_notempty(
        const char *path);

/*
 */
int check_mpv_err(
        int status);
