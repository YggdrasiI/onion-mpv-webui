#pragma once

typedef struct media_track_paths_t media_track_paths_t;

media_track_paths_t *media_track_paths_new(
        onion_dict *shares);

void media_track_paths_free(
        media_track_paths_t *mlp);

void media_track_paths_set_shares(
        media_track_paths_t *mtp,
        onion_dict *shares);

// Called from mpv_plugin loop.
void media_track_paths_set(
        media_track_paths_t *mtp,
        const char *new_path);

// Called from webui-requests
void media_track_paths_set_directory(
        media_track_paths_t *mlp,
        char *path);

share_info_t *media_track_paths_get_current_share(
        media_track_paths_t *mtp);
