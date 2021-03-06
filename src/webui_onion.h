#pragma once

#include <onion/dict.h>

int webui_onion_init(onion_dict *options);
void webui_onion_uninit();

int webui_onion_is_running();
void webui_onion_set_running(int bRun);
void webui_onion_end_signal(int unused);

// for websocket interface
char *handle_command_p2(
        const char *api_args);
/*        const char *command,
        const char *param1,
        const char *param2);*/
