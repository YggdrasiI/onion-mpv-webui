#define _GNU_SOURCE

/**
 * Collects all websocket clients and
 * 1. Sends status on connection.
 * 2. Sends status updates if somethings changes
 * 3. Tracks list of connected clients.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <stdarg.h>
#include <stdint.h>

// for pthread_cond_timedwait
#include <time.h>
#include <sys/time.h>

#include <onion/log.h>
#include <onion/onion.h>
#include <onion/codecs.h>
#include <onion/shortcuts.h>
#include <onion/low.h>

#include "defines.h"
#include "tools.h"
#include "onion_ws_status.h"
#include "webui_onion.h"

#include "utf8.h"

// ================== BEGIN onion releated part ===============
extern __clients_t *websockets;
extern __status_t *status;
#ifdef WITH_MPV
extern mpv_handle *mpv;
#else
mpv_handle *mpv = NULL;
#endif

enum onion_websocket_status_e {
    NORMAL_CLOSURE              = 1000,
    GOING_AWAY                  = 1001,
    PROTOCOL_ERROR              = 1002,
    UNSUPPORTED_DATA            = 1003,
    //  RESERVED                = 1004,
    NO_STATUS_RECEIVED          = 1005,
    ABNORMAL_CLOSURE            = 1006,
    INVALID_FRAME_PAYLOAD_DATA  = 1007,
    POLICY_VIOLATION            = 1008,
    MESSAGE_TOO_BIG             = 1009,
    MISSING_EXTENSION           = 1010,
    INTERNAL_ERROR              = 1011,
    SERVICE_RESTART             = 1012,
    TRY_AGAIN_LATER             = 1013,
    BAD_GATEWAY                 = 1014,
    TLS_HANDSHAKE               = 1015,
};
// convert into char[2] e.g.: STATUS(sname, NORMAL_CLOSURE)
#define STATUS_STR(NAME, NUM) char NAME[2] \
    = {((NUM>>8)&0xFF) , ((NUM)&0xFF)};

// Helper function
int __property_observe(__property_t *prop);
int __property_reobserve(__property_t *prop);


__clients_t *clients_init()
{
    __clients_t *pclients  = calloc(sizeof(__clients_t), 1);
    pclients->num_active_clients = 0;
    pthread_mutex_init(&pclients->lock, NULL);

    int i;
    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        pclients->clients[i].ws = NULL; // extern createded and weak reference
        pthread_mutex_init(&pclients->clients[i].lock, NULL);
    }
    return pclients;
}

void clients_close(
        __status_t *status,
        __clients_t *pclients)
{
    ONION_INFO("Closing websockets…");

    if (status) {
        pthread_cond_signal(&status->init_done); // as fallback if mpv thread not release condition. Not required
    }

    if (pclients == NULL){
        return;
    }
    int i, j;
    for( i=0, j=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket_t *pclient = &pclients->clients[i];
        if ( pclient->ws ){

            onion_websocket *ws = pclient->ws;
            j++;

#if 1
            ONION_INFO("Remove websocket pointer on position %d", i);
            pthread_mutex_lock(&pclients->lock);
            pthread_mutex_lock(&pclient->lock);
            onion_websocket_set_callback(pclient->ws, NULL); // otherwise a closing reply of the client will reach the current callback
            pclient->ws = NULL;
            pclients->num_active_clients--;
            pthread_mutex_unlock(&pclient->lock);
            pthread_mutex_unlock(&pclients->lock);
#endif

            int opcode = onion_websocket_get_opcode(ws);
            ONION_INFO("   Opcode: %d", opcode);

            if( opcode == OWS_CONNECTION_CLOSE ){
                ONION_INFO("Hey, it was already closed!");
            }else{
                STATUS_STR(byebye, NORMAL_CLOSURE);
                // This can block if connectios is already closed?!
                onion_websocket_close(ws, byebye);
            }
        }
    }
    ONION_INFO("… closed websockets: %d", j);
}

void clients_uninit(
        __clients_t *pclients)
{
    /* Send closing information and remove client. */
    if (pclients == NULL){
        return;
    }
    int i;
    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket_t *pclient = &pclients->clients[i];
        //pclient->ws = NULL; // weak reference (destroyed by libonion)
        if ( pclient->ws ){
            pthread_mutex_lock(&pclients->lock);
            pthread_mutex_lock(&pclient->lock);
            pclient->ws = NULL;
            pclients->num_active_clients--;
            ONION_INFO("Decrease active clients on %d",
                    pclients->num_active_clients);
            pthread_mutex_unlock(&pclient->lock);
            pthread_mutex_unlock(&pclients->lock);
        }
        pthread_mutex_destroy(&pclient->lock);
    }
    pthread_mutex_destroy(&pclients->lock);
}

__websocket_t *add_client(
        __clients_t *clients,
        onion_websocket *ws)
{
    /* Store in array of active clients */

    pthread_mutex_lock(&clients->lock);
    // Search free position
    int n;
    for( n=0;n<MAX_ACTIVE_CLIENTS;++n){
        if (clients->clients[n].ws == NULL){
            break;
        }
    }

    if (n >= MAX_ACTIVE_CLIENTS){
        ONION_ERROR("All %d websocket(s) already in use. Close connection to client...",
                MAX_ACTIVE_CLIENTS);
        pthread_mutex_unlock(&clients->lock);
        return NULL;
    }

    // Insert into clients array
    __websocket_t *client = &clients->clients[n];
    pthread_mutex_lock(&client->lock);
    ONION_INFO("Save websocket pointer on position %d", n);
    client->ws = ws;
    clients->num_active_clients++;
    ONION_INFO("New num active clients: %d", clients->num_active_clients);
    pthread_mutex_unlock(&client->lock);

    pthread_mutex_unlock(&clients->lock);
    return client;
}


int remove_client(
        __clients_t *clients,
        onion_websocket *ws)
{
    int n;
    for( n=0;n<MAX_ACTIVE_CLIENTS;++n){
        if (clients->clients[n].ws == ws){
            break;
        }
    }

    if (n >= MAX_ACTIVE_CLIENTS){
        ONION_INFO("No client found for onion_websocket instance.");
        // Note: If the server triggers the closing, we will not found
        return OCS_INTERNAL_ERROR;
    }

    __websocket_t *client = &clients->clients[n];
    pthread_mutex_lock(&clients->lock);
    pthread_mutex_lock(&client->lock);
    ONION_INFO("Remove websocket pointer on position %d", n);
    client->ws = NULL;
    clients->num_active_clients--;
    pthread_mutex_unlock(&client->lock);
    pthread_mutex_unlock(&clients->lock);
    return OCS_PROCESSED;
}

onion_connection_status ws_status_start(
        void *data,
        onion_request * req,
        onion_response * res)
{

    onion_websocket *ws = onion_websocket_new(req, res);
    if (!ws) {
        onion_response_write0(res,
                "<!DOCTYPE html>\n"
                "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head>\n"
                "<body style=\"color:#DDD;background:black;\">"
                "<h1>Test of Websocket CLI.</h1>"
                "<input type=\"text\" id=\"msg\" /><span style=\"width:2em;\"/>\n"
                "<button onclick='ws.close(1000);'>Close connection</button>"
                "<script>\ninit = function(){\n"
                "  msg=document.getElementById('msg')\nmsg.focus()\n\n"
                "  ws=new WebSocket('ws://'+window.location.host+'/ws')\n"
                "  ws.__unhandled_data = []\n\n"
                "  ws.onmessage=function(ev){\n "
                "  metadata = [\n"
                "    (ev.data.charCodeAt(1) << 8) + ev.data.charCodeAt(0),\n"
                "    (ev.data.charCodeAt(3) << 8) + ev.data.charCodeAt(2),\n"
                "  ]\n"
                "  ws.__unhandled_data[metadata[1]] = ev.data.slice(4)\n"
                "  // Skip handle of data if incomplete\n"
                "  if ( ws.__unhandled_data.length < metadata[0] ){\n"
                "    return\n"
                "  }\n"
                "  var data = ws.__unhandled_data.join('')\n"
                "  ws.__unhandled_data = []\n"
                ""
                "  //data = data.substring(0,2000)\n"
                "  if( data.includes(\"status_diff\") ) {\n"
                "  document.getElementById('update').textContent=data+'\\n';\n}"
                "  else if( data.includes(\"status_info\") ) {\n"
                "  document.getElementById('start').textContent=data+'\\n';\n}"
                "  else if( data.includes(\"status\") ) {\n"
                "  document.getElementById('full').textContent=data+'\\n';\n}"
                "  else {\n"
                "  document.getElementById('other').textContent=data+'\\n';\n}"
                "  };}\n"
                "  handle_input = function(ev){\n"
                "    if( ev.key == 'Enter'){\n"
                "      ws.send(ev.target.value)"
                "    }\n"
                "  }\n"
                "  window.addEventListener('load', init, false);\n"
                "  document.getElementById('msg').addEventListener('keyup', handle_input);\n"
                "</script>"
                "<pre id=\"start\"></pre>"
                "<pre id=\"other\"></pre>"
                "<pre id=\"update\"></pre>"
                "<pre id=\"full\" style=\"white-space:normal;\"></pre>"
                "</body></html>");

        return OCS_PROCESSED;

        return onion_shortcut_response(
                "{\"message\":\"Websocket url not avail with GET.\"}",
                HTTP_BAD_REQUEST, req, res);
    }
    ONION_INFO("Websocket started!");

    // Store in array of active clients
    __websocket_t *client = add_client(websockets, ws);
    if (client == NULL){
        __chunked_websocket_printf(ws, "{\"status_info\": \"-1\","
                "\"message\": \"Maximum of clients %d already reached. "
                "Close connection...\" }",
                MAX_ACTIVE_CLIENTS);
        onion_websocket_set_callback(ws, NULL); // otherwise a closing reply of the client will reach the current callback
        STATUS_STR(byebye, NORMAL_CLOSURE);
        onion_websocket_close(ws, byebye);

        return OCS_WEBSOCKET;
    }

    //(Debug) Push initial message (short 'hello client' before several locks)
    __chunked_websocket_printf(ws, "{\"status_info\": \"1\"}");

    assert( client->ws == ws );
    pthread_mutex_lock(&client->lock);

    pthread_mutex_lock(&websockets->lock);
    // Push status (maybe delayed ?!)
    if (websockets->num_active_clients == 1 ){
        // init for first client
        if (status_observe(mpv, status) ){
            // wait until new observation returns initial values
            pthread_mutex_lock(&status->init_in_process);
#ifdef WITH_MPV
            struct timeval now; gettimeofday(&now, NULL);
            struct timespec abstime = {0}; abstime.tv_sec = now.tv_sec + 5;
            // unlocked by mpv event loop
            //pthread_cond_wait(&status->init_done, &status->init_in_process);
            int err = pthread_cond_timedwait(&status->init_done,
                    &status->init_in_process, &abstime);
            if( err == ETIMEDOUT ){
                ONION_INFO("MPV does not returned all observed variables.");
                // TODO: Add info which variable hangs.
            }
#endif

            status_build_full_json(status);
            pthread_mutex_unlock(&status->init_in_process);

        }else{
            ONION_INFO("Multiple calls of status_observe().");
            assert(0);
        }
    }else{
      // update json from previous client to current status
      //ONION_INFO("Build status from existing props");
      status_build_full_json(status);
    }
    pthread_mutex_unlock(&websockets->lock);

    //ONION_INFO("Sends status to new client.");
    pthread_mutex_lock(&status->lock);
    assert( status->json != NULL );
    assert( client->ws != NULL );
    __chunked_websocket_printf(client->ws,
            "{\"status\": %s }", status->json);
    pthread_mutex_unlock(&status->lock);

    onion_websocket_set_callback(client->ws, ws_status_cont);
    pthread_mutex_unlock(&client->lock);

    return OCS_WEBSOCKET;
}

onion_connection_status ws_status_cont(
        void *data,
        onion_websocket * ws,
        ssize_t data_ready_len)
{
    char tmp[256];  // TODO: Maybe too short for input of clients.
    if (data_ready_len > sizeof(tmp))
        data_ready_len = sizeof(tmp) - 1;

    int opcode = onion_websocket_get_opcode(ws);

    if( opcode == OWS_CONNECTION_CLOSE ){

        onion_connection_status ret = remove_client(websockets, ws);
        pthread_mutex_lock(&websockets->lock);
        if (websockets->num_active_clients == 0 ){
            // No further status data needed
            status_unobserve(mpv, status);
        }
        pthread_mutex_unlock(&websockets->lock);
        return ret;
    }

    // search matching client struct (TODO: Shift into privdata of handler?!)
    int n;
    for( n=0;n<MAX_ACTIVE_CLIENTS;++n){
        if (websockets->clients[n].ws == ws){
            break;
        }
    }

    __websocket_t *client = &websockets->clients[n];
    assert( ws == client->ws );
    pthread_mutex_lock(&client->lock);
    int len = onion_websocket_read(ws, tmp, data_ready_len);
    pthread_mutex_unlock(&client->lock);

    if (len <= 0) {
        //ONION_ERROR("Error reading data: %d: %s (%d)", errno, strerror(errno),
        //        data_ready_len);
        return OCS_NEED_MORE_DATA;
    }
    tmp[len] = 0;

    ONION_INFO("Read from websocket: %d: %s", len, tmp);

    // Handle input
    char *output = handle_command_p2(tmp);
    pthread_mutex_lock(&client->lock);

    // send feedback to source client
    __chunked_websocket_printf(ws, "{\"result\": %s}", output);
    FREE(output);
    pthread_mutex_unlock(&client->lock);


    // reply this client
#if 0
    ONION_INFO("Send to source client...");
    pthread_mutex_lock(&client->lock);
    __chunked_websocket_printf(ws, "{\"echo\": \"%s\"}", tmp);
    pthread_mutex_unlock(&client->lock);
#endif

    // send data to other clients
#if 0
    int n;
    for (n=0;n<MAX_ACTIVE_CLIENTS; ++n){
        if (ws_active[n] && ws_active[n] != ws ){
            ONION_INFO("Send to other clients...");

            pthread_mutex_lock(&ws_lock[n]);
            __chunked_websocket_printf(ws_active[n], "Other: %s", tmp);
            pthread_mutex_unlock(&ws_lock[n]);
        }
    }
#endif

    return OCS_NEED_MORE_DATA;
}

// ================== END onion releated part ===============

// ================== BEGIN mpv releated part ===============

__property_t property_init(
        const char *name,
        mpv_format format,
        mpv_format format_out,
        uint64_t userdata)
{
    assert( ( format_out == MPV_FORMAT_INT64
                || format_out == MPV_FORMAT_DOUBLE
                || format_out == MPV_FORMAT_FLAG
                || format_out == MPV_FORMAT_STRING )
            || ( format_out == MPV_FORMAT_NODE_MAP
                && format == MPV_FORMAT_NODE_MAP)
            || ( format_out == MPV_FORMAT_NODE_ARRAY
                && format == MPV_FORMAT_NODE_ARRAY)
          );

    __property_t ret;
    ret.node.name = name;
    ret.node.format = format;
    ret.node.data = NULL;
    //ret.initialized = 0;
    ret.format_out = format_out;
    ret.userdata = userdata;
    ret.updated = 0;
    ret.json = NULL;

    return ret;
}

void property_free(
        __property_t *property)
{
    free(property->json);
}

/* Converts input into json key-value-pair.  */
void property_update(
        __status_t *status,
        mpv_event_property *in,
        __property_t *out)
{
    char changed = 0;

    //ONION_INFO("\nFormat in: %d\nFormat out: %d\nFormat expected:%d\n",
    //        in->format, out->format_out, out->node.format);

    pthread_mutex_lock(&status->lock);

    if (in->format == MPV_FORMAT_NONE){
        // no data avail for this mpv property
        // return;
        /* Note 'chapter' has this format if 'chapters' is empty.
         * we can not simply ignore None-Nodes because
         * we wait on all properties if the observertion has started.
         */
        FREE(out->json);
        switch(out->format_out){
            case MPV_FORMAT_STRING:
                //asprintf(&out->json, "\"%s\": \"undefined\"", out->node.name);
                out->json = strdup("\"undefined\"");
                break;
            case MPV_FORMAT_NODE_MAP:
                //asprintf(&out->json, "\"%s\": []", out->node.name);
                out->json = strdup("[]");
                break;
            case MPV_FORMAT_NODE_ARRAY:
                //asprintf(&out->json, "\"%s\": {}", out->node.name);
                out->json = strdup("{}");
                break;
            case MPV_FORMAT_DOUBLE:
                //asprintf(&out->json, "\"%s\": 0.0", out->node.name);
                out->json = strdup("0.0");
                break;
            case MPV_FORMAT_INT64:
                //asprintf(&out->json, "\"%s\": 0", out->node.name);
                out->json = strdup("0");
                break;
            case MPV_FORMAT_FLAG:
            default:
                //asprintf(&out->json, "\"%s\": null", out->node.name);
                out->json = strdup("null");
                break;
        }
        changed = 1;

    }else if (in->format == MPV_FORMAT_NODE){
        mpv_node *result_node = (mpv_node *)(in->data);
        //ONION_INFO("\nNode: %d", result_node->format);

        if( result_node->format == out->format_out ){
            FREE(out->json);
            switch(result_node->format){
                case MPV_FORMAT_INT64:
                    {
                        int64_t ivalue = (result_node->u.int64);
                        //asprintf(&out->json, "\"%s\": %ld",
                        //        out->node.name, ivalue);
                        asprintf(&out->json, "%ld", ivalue);
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_FLAG:
                    {
                        int bvalue = (result_node->u.flag);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, bvalue?"yes":"no");
                        asprintf(&out->json, "\"%s\"", bvalue?"yes":"no");
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_DOUBLE:
                    {
                        double fvalue = (result_node->u.double_);
                        //asprintf(&out->json, "\"%s\": %lf",
                        //        out->node.name, fvalue);
                        asprintf(&out->json, "%lf", fvalue);
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        char *svalue = (result_node->u.string);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, svalue);
                        // Store escaped variant between quotes
                        out->json = utf8_quote_with_escape(svalue, '"');
                        changed = 1;
                        break;
                    }
            }
        }else{
          if ( result_node->format == MPV_FORMAT_STRING &&
                  out->format_out == MPV_FORMAT_FLAG ){
              // This will be reached if loop_playlist is set to "inf"
              // Maybe a wrong value for a flag?! (Copied bug)
              char *svalue = (result_node->u.string);
              out->json = utf8_quote_with_escape(svalue, '"');
              changed = 1;
          }else{
              ONION_INFO("mpv_node missmatch. Format in: %d, Format out: %d",
                      result_node->format, out->format_out );
          }
        }

    }else if (out->format_out == MPV_FORMAT_NODE_MAP
            || out->format_out == MPV_FORMAT_NODE_ARRAY)
    {
        assert( in->format == MPV_FORMAT_STRING );
        FREE(out->json);
        char *svalue = *(char **)(in->data);
        // Print lists and dicts without quotes
        //asprintf(&out->json, "\"%s\": %s",
        //        out->node.name, svalue);
        out->json = strdup(svalue);
        // NOTE: Array is already converted in a json-string
        // No further escaping required/useful.
        changed = 1;
    }else{
        assert( in->format == out->format_out );
        if( in->format == out->format_out ){
            FREE(out->json);
            switch(in->format){
                case MPV_FORMAT_INT64:
                    {
                        int64_t *ivalue = *(int64_t **)(in->data);
                        //asprintf(&out->json, "\"%s\": %ld",
                        //        out->node.name, *ivalue);
                        asprintf(&out->json, "%ld", *ivalue);
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_FLAG:
                    {
                        int *bvalue = *(int **)(in->data);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, bvalue?"yes":"no");
                        asprintf(&out->json, "\"%s\"", bvalue?"yes":"no");
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_DOUBLE:
                    {
                        double *fvalue = *(double **)(in->data);
                        //asprintf(&out->json, "\"%s\": %lf",
                        //        out->node.name, *fvalue);
                        asprintf(&out->json, "%lf",*fvalue);
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        char *svalue = *(char **)(in->data);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, svalue);
                        // Store escaped variant between quotes
                        out->json = utf8_quote_with_escape(svalue, '"');
                        changed = 1;
                        break;
                    }
            }
        }else{
          if ( in->format == MPV_FORMAT_STRING &&
                  out->format_out == MPV_FORMAT_FLAG ){
              // This will be reached if loop_playlist is set to "inf"
              // Maybe a wrong value for a flag?! (Copied bug)
              char *svalue = *(char **)(in->data);
              out->json = utf8_quote_with_escape(svalue, '"');
              changed = 1;
          }else{
              ONION_INFO("mpv_node missmatch. Format in: %d, Format out: %d",
                      in->format, out->format_out );
          }
        }
    }

    assert( out->json != NULL );

    if (changed){
        if (out->initialized == 0 ){
            status->num_initialized += 1;
            out->initialized = 1;
        }else if (out->updated == 0 ){
            status->num_updated += 1;
            out->updated = 1;
        }
    }

    if( status->num_updated == 0 ){
        ONION_INFO("Init: %d/%d", status->num_initialized, status->num_props);
    }

    if ( status->is_initialized  == 0
            && status->num_initialized >= status->num_props){
        pthread_mutex_unlock(&status->lock);
        status_set_initialized(status);
    }else{
        pthread_mutex_unlock(&status->lock);
    }
}


__status_t *status_init()
{
    __status_t *status = calloc(sizeof(__status_t), 1);

    status->props = calloc(sizeof(__property_t), NUM_PROPS);
#define ADD(NAME, TYPE, TYPE_OUT) if( pos<NUM_PROPS ) {\
    status->props[pos++] = property_init(NAME, TYPE, TYPE_OUT, REPLY_ID_OFFSET + pos); } \
    else { printf("Increase NUM_PROPS"); }
    //TYPE_OUT needs to be the internal type of the property
    //in mpv if TYPE is MPV_FORMAT_NODE.
    int pos = 0;
    //ADD("filename", MPV_FORMAT_NODE, MPV_FORMAT_STRING); // encoding problem?!
    ADD("filename", MPV_FORMAT_STRING, MPV_FORMAT_STRING);
    ADD("duration", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);
    ADD("time-pos", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);
    // time-remaining = duration - time-pos
    ADD("playtime-remaining", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);  //scaled by speed
    //ADD("playback-time", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);  // redundant and scaled by speed
    //ADD("metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_STRING);
    ADD("metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP);
    ADD("volume", MPV_FORMAT_NODE,  MPV_FORMAT_DOUBLE);
    ADD("volume-max", MPV_FORMAT_NODE,  MPV_FORMAT_DOUBLE);
    ADD("playlist", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP);
    ADD("track-list", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP);
    ADD("loop-file", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG);
    ADD("loop-playlist", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG);
    ADD("chapters", MPV_FORMAT_NODE, MPV_FORMAT_INT64);  // no list of  chapter titles, but number

    ADD("sub-delay", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);
    //ADD("sub-delay", MPV_FORMAT_DOUBLE, MPV_FORMAT_STRING); // not '[num] ms' but 0.00…
    ADD("audio-delay", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);
    ADD("speed", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE);

    ADD("pause", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG);
    ADD("fullscreen", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG);
    ADD("chapter", MPV_FORMAT_NODE, MPV_FORMAT_INT64);
    ADD("chapter-metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP); // like metadata, but rare. Usually chapter title only.
    status->num_props = pos;
#undef ADD

    status->num_initialized = 0;
    status->num_updated = 0;
    status->is_initialized = 0;
    status->is_observed = 0;
    status->json = NULL;
    pthread_mutex_init(&status->lock, NULL);
    pthread_mutex_init(&status->init_in_process, NULL);
    pthread_cond_init(&status->init_done, NULL);

    return status;
}

void status_free(
        __status_t *status)
{
    int pos;
    for( pos=0; pos<status->num_props; ++pos){
        property_free(&status->props[pos]);
    }
    FREE(status->props);
    FREE(status->json);
    pthread_mutex_destroy(&status->lock);
    pthread_mutex_destroy(&status->init_in_process);
    pthread_cond_destroy(&status->init_done);
}

int status_observe(
        mpv_handle *mpv,
        __status_t *status)
{
    if (status->is_observed) {
        return 0;
    }

    pthread_mutex_lock(&status->lock);
    int pos;
    for( pos=0; pos<status->num_props; ++pos){
        __property_t *prop = &status->props[pos];
        prop->initialized = 0;
        prop->updated = 0;
        FREE(prop->json);
        __property_observe(prop);
    }

    status->is_observed = 1;
    status->num_updated = 0;
#ifdef WITH_MPV
    status->is_initialized = 0;
    status->num_initialized = 0;
#else
    status->is_initialized = 1;
    status->num_initialized = status->num_props;
#endif
    pthread_mutex_unlock(&status->lock);

    return 1;
}


int __property_observe(__property_t *prop){
#ifdef WITH_MPV
    if ( prop->node.format == MPV_FORMAT_NODE
            &&  ( prop->format_out == MPV_FORMAT_STRING
                || prop->format_out == MPV_FORMAT_INT64
                || prop->format_out == MPV_FORMAT_FLAG
                || prop->format_out == MPV_FORMAT_DOUBLE)
       )
    {
        return mpv_observe_property(mpv, prop->userdata,
                prop->node.name,
                MPV_FORMAT_NODE); // no conversion by mpv
    }else if ( prop->format_out == MPV_FORMAT_NODE_MAP
            || prop->format_out == MPV_FORMAT_NODE_ARRAY )
    {
        // request result as string, but propagate it without
        // quotes into the status-json.
        return mpv_observe_property(mpv, prop->userdata,
                prop->node.name,
                MPV_FORMAT_STRING); // conversion by mpv
    }else{
        return mpv_observe_property(mpv, prop->userdata,
                prop->node.name,
                MPV_FORMAT_STRING); // conversion by mpv
    }
#else
    switch(prop->format_out){
        case MPV_FORMAT_STRING:
            //asprintf(&prop->json, "\"%s\": \"undefined\"", prop->node.name);
            prop->json = strdup("\"undefined\"");
            break;
        case MPV_FORMAT_NODE_MAP:
            //asprintf(&prop->json, "\"%s\": []", prop->node.name);
            prop->json = strdup("[]");
            break;
        case MPV_FORMAT_NODE_ARRAY:
            //asprintf(&prop->json, "\"%s\": {}", prop->node.name);
            prop->json = strdup("{}");
            break;
        case MPV_FORMAT_DOUBLE:
            //asprintf(&prop->json, "\"%s\": 0.0", prop->node.name);
            prop->json = strdup("0.0");
            break;
        case MPV_FORMAT_INT64:
            //asprintf(&prop->json, "\"%s\": 0", prop->node.name);
            prop->json = strdup("0");
            break;
        case MPV_FORMAT_FLAG:
        default:
            //asprintf(&prop->json, "\"%s\": null", prop->node.name);
            prop->json = strdup("null");
            break;
    }
    prop->initialized = 1;
    prop->updated = 0;
    return 0;
#endif
}

// To trigger fetching of data again.
int __property_reobserve(__property_t *prop){
    // Remove property first to avoid multiple observation
    // of same variable.

    //pthread_mutex_lock(&status->lock);
    int err;
    err = mpv_unobserve_property(mpv, prop->userdata);
    check_mpv_err(err);
    if (! (err < 0) ){
        err = __property_observe(prop);
        check_mpv_err(err);
    }
    //pthread_mutex_unlock(&status->lock);
    return err;
}

int property_reobserve(const char *prop_name){
    for( int pos=0; pos<status->num_props; ++pos){
        __property_t *prop = &status->props[pos];
        if (0 == strcmp(prop->node.name, prop_name)){
            return __property_reobserve(prop);
        }
    }
    ONION_INFO("No property '%s' observed.", prop_name);
    return -1;
}

void status_unobserve(
        mpv_handle *mpv,
        __status_t *status)
{
    pthread_mutex_lock(&status->lock);
    if (status->is_observed) {

        int pos;
        for( pos=0; pos<status->num_props; ++pos){
#ifdef WITH_MPV
            __property_t *prop = &status->props[pos];
            int u = mpv_unobserve_property(mpv, prop->userdata);
            assert(u == 1); // assume that just one observation for each userdata exists
#endif
        }
        status->num_updated = 0;
        status->num_initialized = 0;
        status->is_observed = 0;
        status->is_initialized = 0;

        //pthread_cond_signal(&status->init_done); // as fallback if mpv thread not release condition?!
    }

    pthread_mutex_unlock(&status->lock);
}


void status_update(
        __status_t *status,
        mpv_handle *mpv,
        mpv_event *event)
{
    uint64_t userdata = event->reply_userdata;
    mpv_event_property *prop_in =  (mpv_event_property *) (event->data);

    if (userdata < REPLY_ID_OFFSET
            || userdata >= REPLY_ID_OFFSET + status->num_props ){
        ONION_ERROR("observed property contain wrong reply_userdata value '%u'.",
                userdata);
        return;
    }

    int prop_index = userdata - REPLY_ID_OFFSET;
    //ONION_INFO("update observed property '%s'", prop_in->name);
    __property_t *prop_out = &status->props[prop_index];
    property_update(status, prop_in, prop_out);
}

void send_to_all_clients(
        __clients_t *pclients,
        __property_t *prop)
{
    int i;
    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket_t *pclient = &pclients->clients[i];
        if ( pclient->ws ){
            //ONION_INFO("Send websocket msg");
            pthread_mutex_lock(&pclient->lock);
            __chunked_websocket_printf(pclient->ws,
                    "{\"status_diff\": { \"%s\": %s }}",
                    prop->node.name, prop->json);
            pthread_mutex_unlock(&pclient->lock);
        }
    }
}


void status_build_full_json(
        __status_t *status)
{
    pthread_mutex_lock(&status->lock);

    //char __json_start[] = "{ \"status\": {";
    //char __json_end[] = "}}";
    const char __json_start[] = "{\n";
    const char __json_end[] = "}";
    const char __json_sep[] = ", \n";

    const char __prop_pre[] = "\"";
    const char __prop_mid[] = "\": ";
    const char __prop_post[] = "";

    size_t len = 0;
    int n, N=status->num_props;

    //len += strlen(__json_start) + strlen(__json_end);
    //len += N * strlen(__json_sep);
    len += sizeof(__json_start) + sizeof(__json_end) - 2;
    len += N * (sizeof(__json_sep)-1);
    for(n=0; n<N; ++n){
        if (status->props[n].initialized){
            //len += strlen(__prop_pre) + strlen(__prop_mid) + strlen(__prop_post);
            len += (sizeof(__prop_pre) + sizeof(__prop_mid)
                    + sizeof(__prop_post) - 3);
            len += strlen(status->props[n].node.name);
            len += strlen(status->props[n].json);
        }
    }

    //printf("LEN……………: %lu\n", len);
    char *json = malloc(len * sizeof(char) + 1);
    char * const END = json + len;
    if (json != NULL ){
        char *end = json;
        end = stpcpy(end, __json_start);
        for(n=0; n<N; ++n){
            __property_t *prop = &status->props[n];
            if (prop->initialized){
                if( n>0) {
                    end = stpcpy(end, __json_sep);
                }
                /* Prefix with '"prop-name": ' */
                *end++ = '"';
                end = stpcpy(end, prop->node.name);
                *end++ = '"'; *end++ = ':'; *end++ = ' ';
                end = stpcpy(end, prop->json);
            }
        }
        end = stpcpy(end, __json_end);
    }
    //printf("New json: %s\n", json);

    FREE(status->json);
    status->json = json;
    pthread_mutex_unlock(&status->lock);
}

void status_set_initialized(
        __status_t *status)
{
    ONION_INFO("Mark status as initialized.");
    pthread_mutex_lock(&status->lock);
    if (status->is_initialized == 0 ){
      status->is_initialized = 1;
      pthread_cond_signal(&status->init_done);
    }
    pthread_mutex_unlock(&status->lock);

}

void status_send_update(
        __status_t *status,
        __clients_t *pclients)
{
    if (status->is_initialized == 0 ){
      ONION_INFO("Should not be reached");
      return;
    }

    // Send updates with single properties
    int n, N=status->num_props;
    for(n=0; n<N; ++n){
        if (status->props[n].updated){
            // send single property

            pthread_mutex_lock(&status->lock);
            send_to_all_clients(pclients, &status->props[n]);
            status->props[n].updated = 0;
            status->num_updated--;
            pthread_mutex_unlock(&status->lock);
        }
    }

}

// ================== END mpv releated part ===============

char * utf8_quote_with_escape(char *src, char quote_char){

    // 1. Count how often quote_char occurs in src.

    int i, j, d;
    int num_quote_char = 0;
    const int src_len = strlen(src);

    i = 0; j = 0;
    for( ; i<src_len; i=j)
    {
        u8_inc(src,&j), d=j-i;
        if( d == 1 && *(src+i) == quote_char){
            ++num_quote_char;
        }
    }

    // 2. Alloc enough space for result
    const int out_len = src_len + num_quote_char + 2;
    char *out = malloc(out_len * sizeof(char) + 1);
    if (out == NULL) return NULL;

    // 3. Fill out buffer
    if( num_quote_char == 0 ){
        // No escaping required. Just enclose with quotes.
        out[0] = quote_char;
        memcpy(out+1, src, src_len);
        out[out_len-1] = quote_char;
    }else{
        char *pos = out;
        i = 0; j = 0;

        *pos++ = quote_char;
        for( u8_inc(src,&j), d=j-i;
                i<src_len;
                i=j, u8_inc(src,&j), d=j-i )
        {
            // " ==> \"
            if( d == 1 && *(src+i) == quote_char){
                *pos++ = '\\';
            }

            // copy 1, 2, 3, or 4 bytes
            while( d-- > 0 ){
                *pos++ = *(src + i++);
            }
        }
        *pos++ = quote_char;
        assert( pos-out == out_len );
    }

    // Finallize string
    out[out_len] = '\0';

    return out;
}

// Like onion_websocket_vprintf(onion_websocket * ws, const char *fmt, va_list args)
int __chunked_websocket_vprintf(onion_websocket * ws, const char *fmt, va_list args) {
#define SHORT_BUFFER_LEN 256 // Assume < MAX_FD_WRITE_SIZE + METADATA_LEN
#define METADATA_LEN sizeof(websocket_metadata)
  char md_temp[SHORT_BUFFER_LEN + METADATA_LEN];
  char *temp = md_temp + METADATA_LEN;

  va_list argz; // required for second vsnprintf call
  int l;
  va_copy(argz, args);
  l = vsnprintf(temp, SHORT_BUFFER_LEN, fmt, argz);
  va_end(argz);
  if (l < SHORT_BUFFER_LEN){
    // Prepend string with meta data
    websocket_metadata* md = (websocket_metadata *)md_temp;
    md->num_chunks = 1;
    md->chunk_id = 0;

    return onion_websocket_write(ws, md_temp, l + METADATA_LEN);
  } else {

    // Evaluate number of required chunks
#define ROUND_UP(NUM, DIV) (((NUM) + (DIV-1))/(DIV))
    const int num_chunks = ROUND_UP(l, MAX_FD_WRITE_SIZE - METADATA_LEN); 
    ssize_t s_sum = 0;
    char * const buf = onion_low_scalar_malloc(l + 1 + METADATA_LEN);
    if (!buf) {
      // this cannot happen, since onion_low_scalar_malloc
      // handles that error...
      ONION_ERROR("Could not reserve %d bytes", l + 1);
      return -1;
    }
    l = vsnprintf(buf + METADATA_LEN, l + 1 + METADATA_LEN, fmt, args);
    char *chunk = buf; // First bytes for metadata...
    int l_left = l;

    // Write buffer, but prepend each chunk with meta data
    for( int chunk_id=0; chunk_id<num_chunks; ++chunk_id){
      websocket_metadata* md = (websocket_metadata *)chunk;
      // Filling metadata will overwrite parts of previous chunk.
      md->num_chunks = num_chunks;
      md->chunk_id = chunk_id;
      const int num_bytes_to_write = (l_left>MAX_FD_WRITE_SIZE?MAX_FD_WRITE_SIZE:l_left) + METADATA_LEN;

      ssize_t s = onion_websocket_write(ws, chunk, num_bytes_to_write);

      if ( s<0 ){
        ONION_ERROR("Could not write chunk %d with %d bytes. Errno: %d",
            chunk_id, num_bytes_to_write, s);
      }else{
        s_sum += s - METADATA_LEN;
      }

      // Prepare next loop step
      l_left -= MAX_FD_WRITE_SIZE;
      chunk += MAX_FD_WRITE_SIZE; // here, without METADATA_LEN offset!
    }
    onion_low_free(buf);
    return s_sum;
  }
}

// Like onion_websocket_printf(onion_websocket * ws, const char *fmt, ...)
// but prepend metadata structure and limit size of written data
int __chunked_websocket_printf(onion_websocket * ws, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int l = __chunked_websocket_vprintf(ws, fmt, ap);
  va_end(ap);
  return l;
}
