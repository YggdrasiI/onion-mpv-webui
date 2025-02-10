#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <mpv/client.h>
extern mpv_handle *mpv;

#include "tools.h"
#include "mpv_api_commands.h"
#include "onion_ws_status.h"
extern __status *status;

#include <onion/dict.h>
extern onion_dict *options;

#define STR(s) ((s)?(s):"")
#define OBJ(s) ((s)?(s):"{}")

// Nesting for debugging output.
int _mpv_command(mpv_handle *ctx, const char **args){
    const char **parg = args;
    LOG("mpv_command: ");
    while (*parg != NULL) {
        LOG("%s ", *parg);
        ++parg;
    }
    LOG("\n");

    return  mpv_command(mpv, args);
}

int cmd_play(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    int pause = 0;
    int err = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_pause(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    int pause = 1;
    int err = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_toggle_pause(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    int pause;
    int err = mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    if (check_mpv_err(err)){
        return CMD_FAIL;
    }

    pause = pause?0:1;
    err = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_fullscreen(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    int fullscreen;
    int err = mpv_get_property(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
    if (check_mpv_err(err)){
        return CMD_FAIL;
    }

    fullscreen = fullscreen?0:1;
    err = mpv_set_property(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_seek(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"seek", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_position(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"seek", param1, "absolute", NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_prev(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    char *position = mpv_get_property_string(mpv, "time-pos");
    double fposition;
    int err;
    if (sscanf(STR(position), "%lf", &fposition) != 1){ fposition = 0.0; }
    if (fposition > 5.0) {
        char *cmd[] = {"seek", NULL, NULL};
        asprintf(&cmd[1], "%lf", -fposition); // Negative seek
        err = _mpv_command(mpv, (const char **)cmd);
        free(cmd[1]);
    }else{
        const char *cmd[] = {"playlist-prev", NULL};
        err = _mpv_command(mpv, cmd);
    }
    check_mpv_err(err);

    mpv_free(position);
    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_next(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"playlist-next", NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_jump(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    int err = mpv_set_property(mpv, "playlist-pos", MPV_FORMAT_STRING, &param1);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_remove(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"playlist-remove", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_move(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;
    if( !check_int_or_float(param2, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"playlist-move", param1, param2, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_move_up(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    int iposition;
    int err;
    if (sscanf(param1, "%d", &iposition) != 1){ iposition = 0; }
    if (iposition > 0){
        // Construct 'playlist-move p p-1'
        char *cmd[] = {"playlist-move", NULL, NULL, NULL};
        cmd[1] = strdup(param1);
        asprintf(&cmd[2], "%d", iposition - 1);

        err = _mpv_command(mpv, (const char **)cmd);
        free(cmd[1]);
        free(cmd[2]);
    }else{
        err = MPV_ERROR_SUCCESS;
    }
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_shuffle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"playlist-shuffle", NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_loop_file(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *valid[] = {"inf", "no", NULL};

    if (check_valid_param(param1, valid, pOutput_message)){
        int err = mpv_set_property(mpv, "loop-file", MPV_FORMAT_STRING, &param1);
        check_mpv_err(err);

        return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
    }
    perror("Hey, invalid input.\n");

    return CMD_FAIL;
}

int cmd_loop_playlist(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *valid[] = {"inf", "no", "force", NULL};

    if (check_valid_param(param1, valid, pOutput_message)){
        int err = mpv_set_property(mpv, "loop-playlist", MPV_FORMAT_STRING, &param1);
        check_mpv_err(err);

        return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
    }

    return CMD_FAIL;
}

int cmd_add_volume(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "volume", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_volume(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "volume", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "sub-delay", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "sub-delay", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "audio-delay", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "audio-delay", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_cycle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    //param2: Optional 'up' or 'down', but empty string ""
    //would fail. => Remove param2.
    if (param2[0] == '\0') param2 = NULL;

    const char *cmd[] = {"cycle", param1, param2, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    if ( 0 == strcmp("sub", param1)
            || 0 == strcmp("audio", param1)
            || 0 == strcmp("video", param1))
    {
        // Trigger update of ws status.
        property_reobserve("track-list");
    }

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_chapter(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "chapter", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_chapter(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "chapter", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

// mpv/options.c:    {"speed", OPT_DOUBLE(playback_speed), M_RANGE(0.01, 100.0)}
int cmd_increase_playback_speed(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    double current_speed;
    if (mpv_get_property(mpv, "speed", MPV_FORMAT_DOUBLE, &current_speed) < 0){
        perror("Can not fetch playback speed property.\n");
        return CMD_FAIL;
    }

    double fspeed_change;
    const char *_param1 = STR(param1);
    if (parse_float(_param1, strlen(_param1), &fspeed_change) != 1){
        fspeed_change = 1.0;
    }

    // For multiplacations < 1.0 adapt values to generate linear balance.
    // Thus input '0.9' inverts '1.1', etc
    if (fspeed_change - 1.0 < 0 ){
        fspeed_change = 1.0 / (2.0 - fspeed_change);
    }

    double fnew_speed = current_speed * fspeed_change;
    // Round on second digit.
    fnew_speed = ((int)(fnew_speed * 100 + 0.5)) / 100.0;

    // Convert parsed value back to string
    char *new_speed = NULL;
    if ( asprintf(&new_speed, "%lf", fnew_speed) < 0 ){
        perror("asprintf failed.\n");
        free(new_speed);
        return CMD_FAIL;
    }

    LOG("New SPEED: %s\n", new_speed);

    int err = 0;
    //const char *cmd[] = {"multiply", "speed", param1, NULL};
    //err = _mpv_command(mpv, cmd);
    // Range check
    if (fnew_speed < 0.01) {
        const char *cmd[] = {"set", "speed", "0.01", NULL};
        err = _mpv_command(mpv, cmd);
    }else if (fnew_speed > 100.0){
        const char *cmd[] = {"set", "speed", "100.0", NULL};
        err = _mpv_command(mpv, cmd);
    }else {
        const char *cmd[] = {"set", "speed", new_speed, NULL};
        err = _mpv_command(mpv, cmd);
    }


    check_mpv_err(err);
    free(new_speed);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_reset_playback_speed(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"set", "speed", "1.0", NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

// General set not allowed on webinterface.
int _cmd_set_int(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( 1 != check_int_or_float(param2, NULL )) {
        if( pOutput_message != NULL ){
            free(*pOutput_message);
            *pOutput_message = strdup("Parameter needs to be an integer");
        }
        return CMD_FAIL;
    }

    const char *cmd[] = {"set", param1, param2, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_subtitle(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
  int  ret = _cmd_set_int(name, "sub", param1, pOutput_message);
  // Trigger update of ws status.
  property_reobserve("track-list");
  return ret;
}

int cmd_set_audio(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
  int  ret = _cmd_set_int(name, "audio", param1, pOutput_message);
  // Trigger update of ws status.
  property_reobserve("track-list");
  return ret;
}

int cmd_set_video(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
  int  ret = _cmd_set_int(name, "video", param1, pOutput_message);
  // Trigger update of ws status.
  property_reobserve("track-list");
  return ret;
}

int cmd_quit(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if (param1[0] == '\0') { param1 = "0";
    }
    const char *cmd[] = {"quit", param1, NULL};
    int err = _mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_add(const char *name,
        const char *flags, const char *url_or_path,
        char **pOutput_message)
{
#define CHECK_MPV_ERR(VAR, LABEL) check_mpv_err(VAR); if (VAR < MPV_ERROR_SUCCESS) goto LABEL;

    int err = MPV_ERROR_GENERIC;

    if( strlen(url_or_path) == 0){
        free(*pOutput_message); *pOutput_message = strdup("Empty url/path");
        return CMD_FAIL;
    }

    int is_url = check_is_url(url_or_path);
    if (0 > is_url && '1' == onion_dict_get(options, "block_non_shared_files")[0])
    {
        free(*pOutput_message); *pOutput_message = strdup("Local files not allowed");
        return CMD_FAIL;
    }

    if (0 > is_url && 0 > check_is_non_hidden_file(url_or_path)){
        free(*pOutput_message); *pOutput_message = strdup("Input is invalid path");
        return CMD_FAIL;
    }

    if (flags == NULL){
        flags = "";
    }

    const char *cmd[] = {"loadfile", url_or_path, flags, NULL};
    err = _mpv_command(mpv, cmd);
    CHECK_MPV_ERR(err, cmd_playlist_add_fail);

    // Unpause in replace-case
    if (0 == strcmp("replace", flags)) {
        err = cmd_play(NULL, NULL, NULL, NULL);
        CHECK_MPV_ERR(err, cmd_playlist_add_fail);
    }

    // Unpause in append-case if in idle mode (playing latest/new entry in playlist)
    if (0 == strcmp("append", flags)) 
    {
        int idle_flag = check_idle();
        CHECK_MPV_ERR(err, cmd_playlist_add_fail);

        if (idle_flag > 0){
            // 1. Get length of playlist
            int64_t playlist_count = 0;
            err = mpv_get_property(mpv, "playlist/count", MPV_FORMAT_INT64, &playlist_count);
            CHECK_MPV_ERR(err, cmd_playlist_add_fail);

            // 2. Jump to last entry
            if (playlist_count < 1) goto cmd_playlist_add_fail;
            char buf[22];
            if (sizeof(buf) <= snprintf(buf, sizeof(buf), "%d", playlist_count-1)) goto cmd_playlist_add_fail;
            err = cmd_playlist_jump("playlist_jump", buf, "", pOutput_message);
            CHECK_MPV_ERR(err, cmd_playlist_add_fail);

            // 3. Unpause
            err = cmd_play(NULL, NULL, NULL, NULL);
            CHECK_MPV_ERR(err, cmd_playlist_add_fail);
        }
    }

    return CMD_OK;

cmd_playlist_add_fail:
    free(*pOutput_message); *pOutput_message = strdup(mpv_error_string(err));
    return CMD_FAIL;

#undef CHECK_MPV_ERR
}

int cmd_media_playlist_add(const char *name,
        const char *flags, const char *fullpath,
        char **pOutput_message)
{
#define CHECK_MPV_ERR(VAR, LABEL) check_mpv_err(VAR); if (VAR < MPV_ERROR_SUCCESS) goto LABEL;
    int err = MPV_ERROR_GENERIC;

    // Avoid hidden folders/files and all paths which goes up.
    if( strlen(fullpath) == 0 || strstr(fullpath, "/.") != NULL ){
        free(*pOutput_message); *pOutput_message = strdup("Wrong path?!");
        return CMD_FAIL;
    }

    if (flags == NULL){
        flags = "";
    }

    // Old API  < 0.38: loadfile <url> [<flags> [<options>]]
    // New API => 0.38: loadfile <url> [<flags> [<index> [<options>]]]
    // => Is using options-string we has to differ between versions.

    const char *cmd[] = {"loadfile", fullpath, flags, NULL};
    err = _mpv_command(mpv, cmd);
    CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);

    // Unpause in replace-case
    if (0 == strcmp("replace", flags)) {
        err = cmd_play(NULL, NULL, NULL, NULL);
        CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);
    }

    // Unpause in append-case if in idle mode (playing latest/new entry in playlist)
    if (0 == strcmp("append", flags)) 
    {
        int idle_flag = check_idle();
        CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);

        if (idle_flag > 0){
            // 1. Get length of playlist
            int64_t playlist_count = 0;
            err = mpv_get_property(mpv, "playlist/count", MPV_FORMAT_INT64, &playlist_count);
            CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);

            // 2. Jump to last entry
            if (playlist_count < 1) goto cmd_media_playlist_add_fail;
            char buf[22];
            if (sizeof(buf) <= snprintf(buf, sizeof(buf), "%d", playlist_count-1)) goto cmd_media_playlist_add_fail;
            err = cmd_playlist_jump("playlist_jump", buf, "", pOutput_message);
            CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);

            // 3. Unpause
            err = cmd_play(NULL, NULL, NULL, NULL);
            CHECK_MPV_ERR(err, cmd_media_playlist_add_fail);
        }
    }

    return CMD_OK;

cmd_media_playlist_add_fail:
    free(*pOutput_message); *pOutput_message = strdup(mpv_error_string(err));
    return CMD_FAIL;
#undef CHECK_MPV_ERR
}


/*
int cmd_(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    return CMD_OK;
}
*/

char *json_status_response(){

    char *json = NULL;

    // Note: LHS variable names with '_', but RHS properties with '-'
    char *filename = mpv_get_property_string(mpv, "filename");
    char *duration = mpv_get_property_string(mpv, "duration");
    char *position = mpv_get_property_string(mpv, "time-pos");
    char *speed = mpv_get_property_string(mpv, "speed");
    char *remaining = mpv_get_property_string(mpv, "playtime-remaining");
    char *metadata = mpv_get_property_string(mpv, "metadata");
    char *chapter_metadata = mpv_get_property_string(mpv, "chapter-metadata");
    char *volume = mpv_get_property_string(mpv, "volume");
    char *volume_max = mpv_get_property_string(mpv, "volume-max");
    char *playlist = mpv_get_property_string(mpv, "playlist");
    char *track_list = mpv_get_property_string(mpv, "track-list");
    char *chapter_list = mpv_get_property_string(mpv, "chapter-list");
    char *loop_file = mpv_get_property_string(mpv, "loop-file");
    char *loop_playlist = mpv_get_property_string(mpv, "loop-playlist");
    char *chapters = mpv_get_property_string(mpv, "chapters");

    char *sub_delay = mpv_get_property_osd_string(mpv, "sub-delay");
    char *audio_delay = mpv_get_property_osd_string(mpv, "audio-delay");
    // Note: lua-scripts clip last 3 chars of this to variables

    int pause;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pause) < 0){
        perror("Can not fetch pause property.\n");
        goto gen_status_error;
    }

    int fullscreen;
    if (mpv_get_property(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen) < 0){
        perror("Can not fetch fullscreen property.\n");
        goto gen_status_error;
    }

    int idle = check_idle();
    if (idle < 0){
        perror("Can not fetch idle property.\n");
        goto gen_status_error;
    }


    //char *chapter = "0";
    int64_t chapter = 0;
    if (chapters != NULL && chapters[0] != '0'){
        //chapter = mpv_get_property_string(mpv, "chapter");
        if (mpv_get_property(mpv, "chapter", MPV_FORMAT_INT64, &chapter) < 0){
            perror("Can not fetch chapter property.\n");
        }
    }

    // String parsings
    double fduration;
    if (sscanf(STR(duration), "%lf", &fduration) != 1){ fduration = 0; }
    double fposition;
    if (sscanf(STR(position), "%lf", &fposition) != 1){ fposition = 0; }
    double fspeed;
    if (sscanf(STR(speed), "%lf", &fspeed) != 1){ fspeed = 0; }
    double fremaining;
    if (sscanf(STR(remaining), "%lf", &fremaining) != 1){ fremaining = 0; }
    double fvolume;
    if (sscanf(STR(volume), "%lf", &fvolume) != 1){ fvolume = 0; }
    double fvolume_max;
    if (sscanf(STR(volume_max), "%lf", &fvolume_max) != 1){ fvolume_max = 0; }

    int isub_delay; // strip ' ms'
    if (sscanf(STR(sub_delay), "%d ms", &isub_delay) != 1){ isub_delay = 0; }
    int iaudio_delay; // strip ' ms'
    if (sscanf(STR(audio_delay), "%d ms", &iaudio_delay) != 1){ iaudio_delay = 0; }

    int printf_status = asprintf(&json
            , "{"
            "\"filename\":"           "\"%s\""
            ",\n\"duration\":"          "%d"
            ",\n\"position\":"          "%d"
            ",\n\"remaining\":"         "%d"
            ",\n\"metadata\":"          "%s"  // obj
            ",\n\"chapter-metadata\":"  "%s"  // obj
            ",\n\"volume\":"            "%d"
            ",\n\"volume-max\":"        "%d"
            ",\n\"playlist\":"          "%s"  // obj
            ",\n\"track-list\":"        "%s"  // obj
            ",\n\"chapter-list\":"      "%s"  // obj
            ",\n\"loop-file\":"       "\"%s\""
            ",\n\"loop-playlist\":"   "\"%s\""
            ",\n\"chapters\":"          "%s"  // obj

            ",\n\"sub-delay\":"         "%d"
            ",\n\"audio-delay\":"       "%d"

            ",\n\"pause\":"             "%d"
            ",\n\"fullscreen\":"        "%d"
            ",\n\"idle\":"              "%d"
            ",\n\"speed\":"             "%f"

            ",\n\"chapter\":"          "%ld"
            "}\n"

            , STR(filename)
            , (int)(fduration)
            , (int)(fposition)
            , (int)(fremaining)
            , OBJ(metadata)
            , OBJ(chapter_metadata)
            , (int)(fvolume)
            , (int)(fvolume_max)
            , OBJ(playlist)
            , OBJ(track_list)
            , OBJ(chapter_list)
            , STR(loop_file)
            , STR(loop_playlist)
            , OBJ(chapters)

            , isub_delay
            , iaudio_delay

            , pause
            , fullscreen
            , idle
            , fspeed

            , chapter
            );

    if (printf_status < 0){
        goto gen_status_error;
    }

    goto gen_status_end;

gen_status_error:
    free(json);
    json = strdup("{}");

gen_status_end:
    // Cleanup
    mpv_free(filename);
    mpv_free(duration);
    mpv_free(position);
    mpv_free(speed);
    mpv_free(remaining);
    mpv_free(metadata);
    mpv_free(chapter_metadata);
    mpv_free(volume);
    mpv_free(volume_max);
    mpv_free(playlist);
    mpv_free(track_list);
    mpv_free(chapter_list);
    mpv_free(loop_file);
    mpv_free(loop_playlist);
    mpv_free(chapters);

    mpv_free(sub_delay);
    mpv_free(audio_delay);
    //if (chapter) mpv_free(chapter);

    return json;

    /* This was the originial return:
       return '{"audio-delay":'..values.audio_delay:sub(1, -4)..',' ..
       '"chapter":' ..values.chapter..',' ..
       '"chapters":' ..values.chapters..',' ..
       '"duration":'..round(values.duration)..',' ..
       '"filename":"'..values.filename..'",' ..
       '"fullscreen":'..values.fullscreen..',' ..
       '"loop-file":"'..values.loop_file..'",' ..
       '"loop-playlist":"'..values.loop_playlist..'",' ..
       '"metadata":'..values.metadata..',' ..
       '"chapter-metadata":'..values.chapter_metadata..',' ..
       '"pause":'..values.pause..',' ..
       '"playlist":'..values.playlist..',' ..
       '"position":'..round(values.position)..',' ..
       '"speed":'..values.speed..',' ..
       '"remaining":'..round(values.remaining)..',' ..
       '"sub-delay":'..values.sub_delay:sub(1, -4)..',' ..
       '"track-list":'..values.track_list..',' ..
       '"chapter-list":'..values.chapter_list..',' ..
       '"volume":'..round(values.volume)..',' ..
       '"volume-max":'..round(values.volume_max)..'}'
       */
}

int check_idle(){
    int64_t pos;
    int err = mpv_get_property(mpv, "playlist-pos", MPV_FORMAT_INT64, &pos);
    if (err != MPV_ERROR_SUCCESS) return err;
    return (pos<0)?1:0;
}
