#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <mpv/client.h>

mpv_handle *mpv = NULL;
pthread_mutex_t mpv_lock = PTHREAD_MUTEX_INITIALIZER;

#include <onion/onion.h>

#include "webui_onion.h"
#include "mpv_api_commands.h"
#include "mpv_script_options.h"
#include "onion_ws_status.h"

extern onion *o;
extern __status *status;
extern __clients *websockets;

int mpv_open_cplugin(mpv_handle *handle)
{
    if( !handle ){
        perror("Hey, mpv_handle is NULL\n");
        return EXIT_FAILURE;
    }

    int err=0;
    mpv = handle;
    const char *mpv_plugin_name = mpv_client_name(handle);
    printf("Webui C plugin loaded as '%s'!\n", mpv_plugin_name);

    onion_dict *options = get_default_options();
    update_options(mpv, mpv_plugin_name, options);

    if ('1' == onion_dict_get(options, "paused")[0]) {
        int pause = 1;
        err = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
        check_mpv_err(err);
    }

    // Daemon mode.
    if ('1' == onion_dict_get(options, "daemon")[0]) {
        int daemon = 1;
        err = mpv_set_property(mpv, "idle", MPV_FORMAT_FLAG, &daemon);
        check_mpv_err(err);
    }

    int error;
    error = webui_onion_init(options); // also init mutex mpv_lock.
    if( error < 0){
        perror("Initialization of onion object failed.\n");
        webui_onion_uninit(o);
        return EXIT_FAILURE;
    }

    error = onion_listen(o);
    if (error) {
        perror("Cant create the server.\n");
        onion_listen_stop(o);
        webui_onion_uninit(o);
        return EXIT_FAILURE;
    }
    //printf("After nonblocking server...\n");

    webui_onion_set_running(1);
    while (webui_onion_is_running()) {
        /* Do not use timeout < 0 to avoid infinite
         * waiting. Otherwise the ^C-signal handler
         * only stops the onion server, but not mpv.
         */
        mpv_event *event = mpv_wait_event(handle, 0.5);

        if (event->event_id == MPV_EVENT_NONE) {
            continue; // timeout triggered.
        }

        pthread_mutex_lock(&mpv_lock);

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            status_update(status, mpv, event);

            if (status->num_updated > 0) { // Null bei Initialisierung
                status_send_update(status, websockets, 0);
            }
        } else if (event->event_id == MPV_EVENT_PAUSE) {
            // Flush pending properties on websocket.
            if (status->num_updated > 0) { // Null bei Initialisierung
                status_send_update(status, websockets, 1);
            }
        } else {
            printf("Got event: %d\n", event->event_id);
        }

        // […]
        pthread_mutex_unlock(&mpv_lock);

        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            webui_onion_set_running(0);
            break;
        }

    }

    printf("Stop listening (2)\n"); // Redudant in SIGINT-case
    webui_onion_end_signal(0); // includes onion_listen_stop(o);

    webui_onion_uninit(o);
    onion_dict_free(options);

    // Returing do not quit the main mpv thread, but this plugin.
    // Quitting mpv is triggered in webui_onion_uninit()
    return EXIT_SUCCESS;
}

