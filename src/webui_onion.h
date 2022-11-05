#pragma once

#include <onion/dict.h>

// To avoid permanent propagation of property changes
// this value in milliseconds defines a minimal
// time between two updates. 
// It is set for variables known for change during play,
// e.g. 'time-pos'
// Can be overwritten by ws_interval script
#define MINIMAL_TIME_BETWEEN_PROPERTY_UPDATE 500

int webui_onion_init(onion_dict *options);
void webui_onion_uninit(onion *o);

int webui_onion_is_running();
void webui_onion_set_running(int bRun);
void webui_onion_end_signal(int unused);

// for websocket interface
char *handle_command_p2(
        const char *api_args);
/*        const char *command,
        const char *param1,
        const char *param2);*/
