#pragma once

/* Like Lua's pcall: return of 1 indicates success, 0 error */
typedef enum cmd_error_t {
    CMD_FAIL = 0,
    CMD_OK   = 1
} cmd_error_t;

/* Function handlers for the API scheme
 *
 *       /[name]/[param1]/[param2]
 *
 */
typedef int (*CommandHandler)(
        const char *name,
        const char *param1,
        const char *param2,
        char **pOutput_message);

/* 
   [return value]:        cmd_error_t value

   [in] name:             Used command name in API.
                          (It is "XXX" if the function is not used
                          in multiple API calls.)
   [in] param1:           Argument 1 in API.
   [in] param2:           Argument 2 in API.

   [out] pOutput_message: If != NULL, some commands write status
                          messages. Free this string after usage.

   Please prepend cmd_ to function names.
   It's used for a macro expansion in webui_onion.c.


   Template:
   int cmd_XXX(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message){
          return CMD_OK;
        }
*/

/* Look into mpv's "List of Input Commands" to
 * get an overview about the argument handling of each command.*/

int cmd_play(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_pause(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_toggle_pause(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_fullscreen(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_seek(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_position(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_prev(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_next(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_jump(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_remove(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_move(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_move_up(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_playlist_shuffle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_loop_file(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_loop_playlist(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_add_volume(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_volume(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_add_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_add_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_cycle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_add_chapter(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_chapter(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_increase_playback_speed(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_reset_playback_speed(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

// Select track-list entries
int cmd_set_subtitle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_audio(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

int cmd_set_video(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

// quit [<code>]
int cmd_quit(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);

// This can load files given by uri and,
// local files if script options allows loading
// As default, loading of local files is disabled for
// security reason.
//
// Example: curl http://localhost:9000/api/playlist_add/replace/http://[…]
int cmd_playlist_add(const char *name,
        const char *flags, const char *url_or_path,
        char **pOutput_message);
// Flags same as mpv's loadfile command, e.g
// "replace", "append", "append-play"


/* Commands for /media/api/… 
 * should prefixed with 'cmd_media_'
 */

// This can add arbitary (local) files to mpv's playlist.
/* Note: I assume here that the check if this file is within a media share
 * was already done.
 */
int cmd_media_playlist_add(const char *name,
        const char *flags, const char *fullpath,
        char **pOutput_message);
// Flags same as mpv's loadfile command, e.g
// "replace", "append", "append-play"


char *json_status_response();

int check_mpv_err(int status);
