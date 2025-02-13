/*
 * Stores for each share the latest viewed path and
 * updates this value if mpv starts new file, too.
 *
 * Moreover, the latest used share-id can be set/get.
 *
 */
#define _GNU_SOURCE
#include <string.h>
#include <libgen.h>  // POSIX dirname, basename

//#include <limits.h>  // for PATH_MAX

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <onion/dict.h>
#include <onion/log.h>

#include "defines.h"
#include "tools.h"
#include "media.h"
#include "media_track_paths.h"


typedef struct media_track_paths_t {
    onion_dict *shares;
    //char *current_share; // "" or key from shares.
    share_info_t *current_share_info;
      // share_info["current_directory"] Stores relative paths to share root.
    char *current_directory;   // Absolute path in file system
    char *current_file;
    char *pending;  // updated by mpv_plugin thread
    pthread_mutex_t lock;
} media_track_paths_t;

#define STR_FREE_DUP(TARGET, SOURCE) free(TARGET); TARGET = strdup(SOURCE);

// Just for internal usage because new_path arg will be changed during call.
void _media_track_paths_update(
        media_track_paths_t *mtp,
        char *new_path,
        int split_filename);


media_track_paths_t *media_track_paths_new(
        onion_dict *shares)
{
    media_track_paths_t *this = calloc(1, sizeof(media_track_paths_t));
    pthread_mutex_init(&this->lock, NULL); /* Instead of PTHREAD_MUTEX_INITIALIZER which only works for static vars */
    media_track_paths_set_shares(this, shares);
    return this;
}

void media_track_paths_free(media_track_paths_t *mtp){
    FREE(mtp->current_directory);
    FREE(mtp->current_file);
}

void media_track_paths_set_shares(
        media_track_paths_t *mtp,
        onion_dict *shares)
{
    mtp->shares = shares;
}

/* Change prefix  
 *  Share s1:/a/b
 *  /a/b/c/d.mp3  => /c/d.mp3
 *  /a/b/c        => /c
 *  /a/b          => ""
 *
 *  Only call it after sucessfull strstarts
 */
char *translate_path(share_info_t *share_info, const char *path){
  const char *prefix = share_info_get_path(share_info);
  assert(prefix != NULL);
  if (path == NULL) return NULL;
  return strdup(path + strlen(prefix));
}

typedef struct __update_loop_t {
    media_track_paths_t *mtp;
    char *new_directory;
    char *new_file;
    int found;
} __update_loop_t;

// dict-loop handler
void __update(
        __update_loop_t *data,
        const char *_key, share_info_t *share_info, int flags){

    if (data->found) return;
    if (0 == strstarts(data->new_directory, share_info_get_path(share_info)))
    {
        data->found = 1;
        data->mtp->current_share_info = share_info;

        char *translated_directory = translate_path(data->mtp->current_share_info, data->new_directory);
        onion_dict_add((onion_dict *)share_info, "current_directory", 
                translated_directory, OD_FREE_VALUE|OD_REPLACE);
        onion_dict_add((onion_dict *)share_info, "current_file", 
                data->new_file, OD_DUP_VALUE|OD_REPLACE);
        // Update current_share
        STR_FREE_DUP(data->mtp->current_directory, data->new_directory);
        STR_FREE_DUP(data->mtp->current_file, data->new_file);
    }
}

/* Parses new_path and update "current_" infos in shares.
 *
 * split_filename=0 marking change of directory without providing
 *                  a filename inside of if (empty basename).
 *
 * Just for internal usage because new_path arg will be changed during call.
 */
void _media_track_paths_update(
        media_track_paths_t *mtp,
        char *new_path,
        int split_filename)
{
    char *new_file;
    char *new_directory;

    if (new_path[0] == '\0') return;

    // 'Disadvantage': reaplath resolves symbolic links
    char *real_path = realpath(new_path, NULL);
    if (real_path == NULL) return;

    if (0 == split_filename){
        new_file = "";
        new_directory = real_path; //new_path;
    }else{
        /* In POSIX-variant (libgen.h)
         *
         * Both, dirname and basename can alter input argument.
         * (/a/b/c/\0 -(basename)-> /a/b/c\0\0  -(dirname)-> /a/b\0c\0\0 )
         * Thus, call basename() first, because dirname() will insert '\0' 
         * before the last (non-empty) component.
         *
         * Moreover, both can return pointer to other (static) strings, e.g. for
         * "", ".", "noslash" dirname() points to an (extern) ".".
         *
         * Never free return values of both functions.
         * */
        new_file = basename(real_path); // basename(new_path);
        new_directory = dirname(real_path); // dirname(new_path);
    }
    //printf("DIR: '%s'\nFILE: '%s'\n\n", new_directory, new_file);


    if (mtp->current_directory && 0 == strcmp(mtp->current_directory, new_directory)){
        // Simple, it's just a new file in the same dir
        onion_dict_add((onion_dict *)mtp->current_share_info, "current_file", 
                new_file, OD_DUP_VALUE|OD_REPLACE);
        STR_FREE_DUP(mtp->current_file, new_file);
        goto _media_track_paths_update_end;
    }

    if (mtp->current_share_info && 0 == strstarts(real_path /* new_path */,
                onion_dict_get((onion_dict*)mtp->current_share_info, "path"))){
        char *translated_directory = translate_path(mtp->current_share_info, new_directory);
        // New path is included in current share
        onion_dict_add((onion_dict *)mtp->current_share_info, "current_directory", 
                translated_directory, OD_FREE_VALUE|OD_REPLACE);
        onion_dict_add((onion_dict *)mtp->current_share_info, "current_file", 
                new_file, OD_DUP_VALUE|OD_REPLACE);
        STR_FREE_DUP(mtp->current_directory, new_directory);
        STR_FREE_DUP(mtp->current_file, new_file);
        goto _media_track_paths_update_end;
    }

    // Search matching share for new path/url
    __update_loop_t data = {mtp, new_directory, real_path /*new_file*/, 0};
    onion_dict_preorder(mtp->shares, __update, &data);

    if (data.found == 0){
        // Throw away new_directory, new_file, but keep current share selected.
        ONION_DEBUG("Starting file outside of shared folders.");
    }

_media_track_paths_update_end:
    free(real_path);
}

void media_track_paths_set(
        media_track_paths_t *mtp,
        const char *new_path)
{
    char *tmp = strdup(new_path);
    pthread_mutex_lock(&mtp->lock);
    free(mtp->pending);
    mtp->pending = tmp;
    pthread_mutex_unlock(&mtp->lock);
}

void media_track_paths_set_directory(media_track_paths_t *mtp, char *path){
    // Process pending value from mpv_plugin
    // This could e.g. change the current dir of a share != 'share(path)'.
    if (mtp->pending){
        pthread_mutex_lock(&mtp->lock);
        _media_track_paths_update(mtp, mtp->pending, 1);
        mtp->pending = NULL;
        pthread_mutex_unlock(&mtp->lock);
    }

    char *_path = strdup(path);
    _media_track_paths_update(mtp, _path, 0);
    free(_path);

}


share_info_t *media_track_paths_get_current_share(
        media_track_paths_t *mtp)
{
    if (mtp->pending){
        pthread_mutex_lock(&mtp->lock);
        _media_track_paths_update(mtp, mtp->pending, 1);
        mtp->pending = NULL;
        pthread_mutex_unlock(&mtp->lock);
    }

    return mtp->current_share_info;
}
/*
const char *media_track_paths_get_current_dict(
        media_track_paths_t *mtp)
{
    return mtp->current_directory;
}
const char *media_track_paths_get_current_file(
        media_track_paths_t *mtp)
{
    return mtp->current_file;
}
*/
