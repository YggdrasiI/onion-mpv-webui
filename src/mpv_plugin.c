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
#include "tools.h"

extern onion *o;
extern __status *status;
extern __clients *websockets;

int ws_interval = MINIMAL_TIME_BETWEEN_PROPERTY_UPDATE;


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

    // Debug flag
    log_debug = ('0' != onion_dict_get(options, "debug")[0]); 

    // Websocket 
    int ws_interval_new;
    const char *const ws_interval_str = onion_dict_get(options, "ws_interval");
    if ( ws_interval_str && 
            sscanf(ws_interval_str,"%d", &ws_interval_new) == 1)
    {
        ws_interval = ws_interval_new;
        LOG("Set websocket lower bound for updates on %dms\n",
                ws_interval);
    }

    // Pause state
    int pause;
    if ('1' == onion_dict_get(options, "paused")[0]) {
        pause = 1;
        err = mpv_set_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
        check_mpv_err(err);
    }
    err = mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pause);
    check_mpv_err(err);

    // Daemon mode.
    if ('1' == onion_dict_get(options, "daemon")[0]) {
        int daemon = 1;
        err = mpv_set_property(mpv, "idle", MPV_FORMAT_FLAG, &daemon);
        check_mpv_err(err);
    }

    err = webui_onion_init(options); // also init mutex mpv_lock.
    if( err < 0){
        perror("Initialization of onion object failed.\n");
        webui_onion_uninit(o);
        return EXIT_FAILURE;
    }

    err = onion_listen(o);
    if (err) {
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

            if (status->num_updated > 0) {
                int force_sent_immediately = pause;
                status_send_update(status, websockets, force_sent_immediately);
            }
        } else if (event->event_id == MPV_EVENT_PAUSE) {
            pause = 1; // Triggers flush of pending properties on websocket.
            if (status->num_updated > 0) {
                /* Commented out because MPV_EVENT_PROPERTY_CHANGE for pause
                 * will be triggered, too.
                 */
                //status_send_update(status, websockets, 1);
            }
        } else if (event->event_id == MPV_EVENT_UNPAUSE) {
            pause = 0;
        } else {
            LOG("Got event: %d\n", event->event_id);
        }

        pthread_mutex_unlock(&mpv_lock);

        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            webui_onion_set_running(0);
            break;
        }

    }

    LOG("Stop listening (2)\n"); // Redudant in SIGINT-case
    webui_onion_end_signal(0); // includes onion_listen_stop(o);

    webui_onion_uninit(o);
    onion_dict_free(options);

    // Returing do not quit the main mpv thread, but this plugin.
    // Quitting mpv is triggered in webui_onion_uninit()
    return EXIT_SUCCESS;
}

