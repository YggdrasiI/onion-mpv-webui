#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <mpv/client.h>
extern mpv_handle *mpv;

#include "mpv_api_commands.h"
#include "tools.h"

#define STR(s) ((s)?(s):"")
#define OBJ(s) ((s)?(s):"{}")

int check_mpv_err(int status){
    if( status < MPV_ERROR_SUCCESS ){
        printf("mpv API error %d: %s\n", status,
                mpv_error_string(status));
    }
    return status; // == 0 for MPV_ERROR_SUCCESS
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
        return 0;
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
        return 0;
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
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_position(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"seek", param1, "absolute", NULL};
    int err = mpv_command(mpv, cmd);
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
    if (fposition > 1.0) {
        char *cmd[] = {"seek", NULL, NULL};
        asprintf(&cmd[1], "%lf", -fposition); // Negative seek
        err = mpv_command(mpv, (const char **)cmd);
        free(cmd[1]);
    }else{
        const char *cmd[] = {"playlist-prev", NULL};
        err = mpv_command(mpv, cmd);
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
    int err = mpv_command(mpv, cmd);
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
    int err = mpv_command(mpv, cmd);
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
    int err = mpv_command(mpv, cmd);
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

        err = mpv_command(mpv, (const char **)cmd);
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
    int err = mpv_command(mpv, cmd);
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
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_volume(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "volume", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "sub-delay", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_sub_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "sub-delay", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "audio-delay", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_set_audio_delay(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"set", "audio-delay", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_cycle_sub(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"cycle", "sub", NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_cycle_audio(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"cycle", "audio", NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_cycle_audio_device(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    // TODO
    const char *cmd[] = {"cycle_values", "audio-device", "", NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_add_chapter(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    if( !check_int_or_float(param1, pOutput_message) ) return CMD_FAIL;

    const char *cmd[] = {"add", "chapter", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_quit(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message)
{
    const char *cmd[] = {"quit", param1, NULL};
    int err = mpv_command(mpv, cmd);
    check_mpv_err(err);

    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_uri_add(const char *name,
        const char *flags, const char *url,
        char **pOutput_message)
{
    int err = MPV_ERROR_GENERIC;

    // Avoid hidden folders/files and all paths which goes up.
    if( strlen(url) == 0 || strstr(url, "://") == NULL ){
        free(*pOutput_message); *pOutput_message = strdup("Wrong url?!");
        return CMD_FAIL;
    }

    if (flags == NULL){
        flags = "";
    }

    const char *cmd[] = {"loadfile", url, flags, NULL};
    err = mpv_command(mpv, cmd);
    check_mpv_err(err);
    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
}

int cmd_playlist_add(const char *name,
        const char *flags, const char *fullpath,
        char **pOutput_message)
{
    int err = MPV_ERROR_GENERIC;

    // Avoid hidden folders/files and all paths which goes up.
    if( strlen(fullpath) == 0 || strstr(fullpath, "/.") != NULL ){
        free(*pOutput_message); *pOutput_message = strdup("Wrong path?!");
        return CMD_FAIL;
    }

    if (flags == NULL){
        flags = "";
    }

    const char *cmd[] = {"loadfile", fullpath, flags, NULL};
    err = mpv_command(mpv, cmd);
    check_mpv_err(err);
    return (err == MPV_ERROR_SUCCESS)?CMD_OK:CMD_FAIL;
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
    char *remaining = mpv_get_property_string(mpv, "playtime-remaining");
    char *metadata = mpv_get_property_string(mpv, "metadata");
    char *volume = mpv_get_property_string(mpv, "volume");
    char *volume_max = mpv_get_property_string(mpv, "volume-max");
    char *playlist = mpv_get_property_string(mpv, "playlist");
    char *track_list = mpv_get_property_string(mpv, "track-list");
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
            ",\n\"volume\":"            "%d"
            ",\n\"volume-max\":"        "%d"
            ",\n\"playlist\":"          "%s"  // obj
            ",\n\"track-list\":"        "%s"  // obj
            ",\n\"loop-file\":"       "\"%s\""
            ",\n\"loop-playlist\":"   "\"%s\""
            ",\n\"chapters\":"          "%s"  // obj

            ",\n\"sub-delay\":"         "%d"
            ",\n\"audio-delay\":"       "%d"

            ",\n\"pause\":"             "%d"
            ",\n\"fullscreen\":"        "%d"

            ",\n\"chapter\":"          "%ld"
            "}\n"

            , STR(filename)
            , (int)(fduration)
            , (int)(fposition)
            , (int)(fremaining)
            , OBJ(metadata)
            , (int)(fvolume)
            , (int)(fvolume_max)
            , OBJ(playlist)
            , OBJ(track_list)
            , STR(loop_file)
            , STR(loop_playlist)
            , OBJ(chapters)

            , isub_delay
            , iaudio_delay

            , pause
            , fullscreen

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
    mpv_free(remaining);
    mpv_free(metadata);
    mpv_free(volume);
    mpv_free(volume_max);
    mpv_free(playlist);
    mpv_free(track_list);
    mpv_free(loop_file);
    mpv_free(loop_playlist);
    mpv_free(chapters);

    mpv_free(sub_delay);
    mpv_free(audio_delay);
    //if (chapter) mpv_free(chapter);

    return json;

    /*
       return '{"audio-delay":'..values.audio_delay:sub(1, -4)..',' ..
       '"chapter":' ..values.chapter..',' ..
       '"chapters":' ..values.chapters..',' ..
       '"duration":'..round(values.duration)..',' ..
       '"filename":"'..values.filename..'",' ..
       '"fullscreen":'..values.fullscreen..',' ..
       '"loop-file":"'..values.loop_file..'",' ..
       '"loop-playlist":"'..values.loop_playlist..'",' ..
       '"metadata":'..values.metadata..',' ..
       '"pause":'..values.pause..',' ..
       '"playlist":'..values.playlist..',' ..
       '"position":'..round(values.position)..',' ..
       '"remaining":'..round(values.remaining)..',' ..
       '"sub-delay":'..values.sub_delay:sub(1, -4)..',' ..
       '"track-list":'..values.track_list..',' ..
       '"volume":'..round(values.volume)..',' ..
       '"volume-max":'..round(values.volume_max)..'}'
       */
}

