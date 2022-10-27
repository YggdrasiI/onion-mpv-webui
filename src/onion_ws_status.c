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
#include <onion/websocket.h>
#include <onion/types_internal.h>

#include "defines.h"
#include "tools.h"
#include "onion_ws_status.h"
#include "webui_onion.h"

#include "utf8.h"

// ================== BEGIN onion releated part ===============
extern __clients *websockets;
extern __status *status;
#ifdef WITH_MPV
extern mpv_handle *mpv;
#else
mpv_handle *mpv = NULL;
#endif
extern int ws_interval;

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
int __property_observe(__property *prop);
int __property_reobserve(__property *prop);


__clients *clients_init()
{
    __clients *pclients  = calloc(sizeof(__clients), 1);
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
        __status *status,
        __clients *pclients)
{
    ONION_DEBUG("Closing websockets…");

    if (status) {
        pthread_cond_signal(&status->init_done); // as fallback if mpv thread not release condition. Not required
    }

    if (pclients == NULL){
        return;
    }
    int i, j;
    for( i=0, j=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket *pclient = &pclients->clients[i];
        if ( pclient->ws ){

            onion_websocket *ws = pclient->ws;
            j++;

#if 1
            ONION_DEBUG("Remove websocket pointer on position %d", i);
            pthread_mutex_lock(&pclients->lock);
            pthread_mutex_lock(&pclient->lock);
            onion_websocket_set_callback(pclient->ws, NULL); // otherwise a closing reply of the client will reach the current callback
            pclient->ws = NULL;
            pclients->num_active_clients--;
            pthread_mutex_unlock(&pclient->lock);
            pthread_mutex_unlock(&pclients->lock);
#endif

            int opcode = onion_websocket_get_opcode(ws);
            //ONION_DEBUG("   Opcode: %d", opcode);

            if( opcode == OWS_CONNECTION_CLOSE ){
                ONION_ERROR("Hey, it was already closed!");
            }else{
                STATUS_STR(byebye, NORMAL_CLOSURE);
                // This can block if connectios is already closed?!
                onion_websocket_close(ws, byebye);
            }
        }
    }
    ONION_DEBUG("… closed websockets: %d", j);
}

void clients_uninit(
        __clients *pclients)
{
    /* Send closing information and remove client. */
    if (pclients == NULL){
        return;
    }
    int i;
    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket *pclient = &pclients->clients[i];
        //pclient->ws = NULL; // weak reference (destroyed by libonion)
        if ( pclient->ws ){
            pthread_mutex_lock(&pclients->lock);
            pthread_mutex_lock(&pclient->lock);
            pclient->ws = NULL;
            pclients->num_active_clients--;
            ONION_DEBUG("Decrease active clients on %d",
                    pclients->num_active_clients);
            pthread_mutex_unlock(&pclient->lock);
            pthread_mutex_unlock(&pclients->lock);
        }
        pthread_mutex_destroy(&pclient->lock);
    }
    pthread_mutex_destroy(&pclients->lock);
}

__websocket *add_client(
        __clients *clients,
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
    __websocket *client = &clients->clients[n];
    pthread_mutex_lock(&client->lock);
    ONION_DEBUG("Save websocket pointer on position %d", n);
    client->ws = ws;
    clients->num_active_clients++;
    ONION_DEBUG("New num active clients: %d", clients->num_active_clients);
    pthread_mutex_unlock(&client->lock);

    // Put information about index 'n' into userdata
    ws_userdata *userdata = ws_userdata_new(n);
    onion_websocket_set_userdata(ws, userdata, ws_userdata_free);

    pthread_mutex_unlock(&clients->lock);
    return client;
}

ws_userdata *ws_userdata_new(
        size_t index)
{
  ws_userdata *ret = malloc(sizeof(ws_userdata));
  ret->index = index;

  return ret;
};

void ws_userdata_free(
        void * data)
{
  ws_userdata *userdata = (ws_userdata *) data; 
  //__websocket *client = &clients->clients[userdata->index];

  remove_client(websockets, userdata->index);
  pthread_mutex_lock(&websockets->lock);
  free(userdata);

  // TODO: Bad position here?!
  if (websockets->num_active_clients == 0 ){
      // No further status data needed
      status_unobserve(mpv, status);
  }
  pthread_mutex_unlock(&websockets->lock);

}

int remove_client(
        __clients *clients,
        size_t index)
{
    if (index > MAX_ACTIVE_CLIENTS) {
        ONION_ERROR("Wrong client index %u", index);
        return -1;
    }

    pthread_mutex_lock(&clients->lock);
    __websocket *client = &clients->clients[index];
    pthread_mutex_lock(&client->lock);
    ONION_DEBUG("Remove websocket pointer on position %d", index);
    client->ws = NULL;
    pthread_mutex_unlock(&client->lock);
    clients->num_active_clients--;
    pthread_mutex_unlock(&clients->lock);

    return 0;
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
    ONION_DEBUG("Websocket started!");

    // Store in array of active clients
    __websocket *client = add_client(websockets, ws);
    if (client == NULL){
        onion_websocket_set_opcode(ws, OWS_TEXT);
        __chunked_websocket_printf(ws, "{\"status_info\": \"-1\","
                "\"message\": \"Maximum of clients %d already reached. "
                "Close connection...\" }",
                MAX_ACTIVE_CLIENTS);
        onion_websocket_set_callback(ws, NULL); // otherwise a closing reply of the client will reach the current callback
        STATUS_STR(byebye, NORMAL_CLOSURE);
        onion_websocket_close(ws, byebye);

        return OCS_WEBSOCKET;
    }

    assert( client->ws == ws );
    pthread_mutex_lock(&client->lock);

    //(Debug) Push initial message (short 'hello client' before several locks)
    onion_websocket_set_opcode(ws, OWS_TEXT);
    __chunked_websocket_printf(ws, "{\"status_info\": \"1\"}");

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
                ONION_DEBUG("MPV does not returned all observed variables.");
                // TODO: Add info which variable hangs.
            }
#endif

            status_build_full_json(status);
            pthread_mutex_unlock(&status->init_in_process);

        }else{
            ONION_ERROR("Multiple calls of status_observe().");
            assert(0);
        }
    }else{
      // update json from previous client to current status
      //ONION_DEBUG("Build status from existing props");
      status_build_full_json(status);
    }
    pthread_mutex_unlock(&websockets->lock);

    //ONION_DEBUG("Send status to new client.");
    pthread_mutex_lock(&status->lock);
    assert( status->json != NULL );
    assert( client->ws != NULL );
    onion_websocket_set_opcode(ws, OWS_TEXT);
    __chunked_websocket_printf(client->ws,
            "{\"status\": %s }", status->json);
    pthread_mutex_unlock(&status->lock);

    onion_websocket_set_callback(client->ws, ws_status_cont);
    pthread_mutex_unlock(&client->lock);

    return OCS_WEBSOCKET;
}

// Call onion_websocket_read if new data is available 
// and terminate string with '\0' .
int __read_string(
        __websocket *client,
        char *tmp,
        ssize_t data_ready_len)
{
    int len = 0;
    // Avoid read in case of zero data! It would fetch header on empty buffer.
    if (data_ready_len > 0){
        pthread_mutex_lock(&client->lock);
        len = onion_websocket_read(client->ws, tmp, data_ready_len);
        pthread_mutex_unlock(&client->lock);
    }

    // Add termination char.
    tmp[(len<0?0:len)] = '\0';
    return len;
}

onion_connection_status ws_status_cont(
        void *data,
        onion_websocket * ws,
        ssize_t data_ready_len)
{
    int opcode = onion_websocket_get_opcode(ws);

    ws_userdata *userdata = (ws_userdata *) ws->user_data;
    assert (0 <= userdata->index && userdata->index < MAX_ACTIVE_CLIENTS);
    assert (ws == websockets->clients[userdata->index].ws);
    __websocket *client = &websockets->clients[userdata->index];

    // Handle closing message
    if( opcode == OWS_CONNECTION_CLOSE ){
        //Freeing memory was shifted in userdata_free handler
        return OCS_PROCESSED;
    }

    // Setup buffer for reading
    onion_connection_status ret = OCS_NEED_MORE_DATA;
    char _tmp[256];  // Maybe too short for input of clients.
    char *tmp = _tmp; // Allocation of more memory if needed.

    if (data_ready_len > WS_MAX_BUFFER_SIZE){
        data_ready_len = WS_MAX_BUFFER_SIZE;
        ONION_DEBUG("Really big input requested. Just read first %u bytes", data_ready_len);
    }
    if (data_ready_len > sizeof(_tmp)){
        tmp = malloc(data_ready_len + 1);
    }


    // Handle pong message
    if( opcode == OWS_PONG ){
        ONION_DEBUG("Got PONG from client!");

        // Consume message payload, if needed (max 125 bytes)
        int len = __read_string(client, tmp, data_ready_len);
        if (len > 0) {
            ONION_DEBUG("PONG payload: '%s'", tmp);
        }

        goto cleanup_and_end;
    }

    if( opcode == OWS_PING ){
        // libonion already send pong reply.
        // Ignore this messages here.
        goto cleanup_and_end;
    }

    // Other messages (assume OWS_TEXT or OWS_BINARY)
    int len = __read_string(client, tmp, data_ready_len);

    if (len < 0) {
        ONION_ERROR("Error reading data: %d: %s (%d)",
                errno, strerror(errno), data_ready_len);
        goto cleanup_and_end;
    }

    ONION_DEBUG("Read from websocket: %d: %s", len, tmp);

    // Handle input
    char *output = handle_command_p2(tmp);
    pthread_mutex_lock(&client->lock);

    // send feedback to source client
    onion_websocket_set_opcode(ws, OWS_TEXT);
    __chunked_websocket_printf(ws, "{\"result\": %s}", output);
    pthread_mutex_unlock(&client->lock);
    FREE(output);

cleanup_and_end:
    if (tmp != _tmp ){ // clean dyn. allocated mem
        free(tmp);
    }
    return ret; // OCS_NEED_MORE_DATA
}

// ================== END onion releated part ===============

// ================== BEGIN mpv releated part ===============

__property property_init(
        const char *name,
        mpv_format format,
        mpv_format format_out,
        uint32_t update_interval_ms,
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

    __property ret;
    ret.node.name = name;
    ret.node.format = format;
    ret.node.data = NULL;
    ret.value = (mpv_node){0};
    //ret.initialized = 0;
    ret.format_out = format_out;
    ret.userdata = userdata;
    ret.updated = 0;
    ret.last_update_time = (struct timespec){.tv_sec=0, .tv_nsec=0};
    ret.next_update_time = (struct timespec){.tv_sec=0, .tv_nsec=0};
    ret.minimal_update_diff = (struct timespec){
        .tv_sec=update_interval_ms/1000,         // seconds
        .tv_nsec=(update_interval_ms%1000) * 1E6 // nanoseconds
    };
    ret.json = NULL;

    return ret;
}

void property_free(
        __property *prop)
{
    if (prop->value.format == MPV_FORMAT_STRING) {
        if (prop->value.u.string != prop->json) {
            free(prop->value.u.string);
        }
    }
    free(prop->json);
}


void parse_value(
        __property *out){


    if (out->value.format == MPV_FORMAT_STRING) {
        if (out->json == out->value.u.string) {
            ONION_DEBUG("Double parsing of same string. Probably some logic is broken.")
            return;
        }

        // Node is already parsed. Just map pointer;
        FREE(out->json);
        out->json = out->value.u.string;
        return;
    }

    FREE(out->json);
    if (out->value.format == MPV_FORMAT_NONE) {
        // no data avail for this mpv property
        /* Note 'chapter' has this format if 'chapters' is empty.
         * we can not simply ignore None-Nodes because
         * we wait on all properties if the observertion has started.
         */
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
    } else if (out->value.format == MPV_FORMAT_NODE) {
        if( out->value.format == out->format_out ){
            switch(out->value.format){
                case MPV_FORMAT_INT64:
                    {
                        int64_t ivalue = (out->value.u.int64);
                        //asprintf(&out->json, "\"%s\": %ld",
                        //        out->node.name, ivalue);
                        asprintf(&out->json, "%ld", ivalue);
                        out->value.format = MPV_FORMAT_STRING;
                        break;
                    }
                case MPV_FORMAT_FLAG:
                    {
                        int bvalue = (out->value.u.flag);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, bvalue?"yes":"no");
                        asprintf(&out->json, "\"%s\"", bvalue?"yes":"no");
                        out->value.format = MPV_FORMAT_STRING;
                        break;
                    }
                case MPV_FORMAT_DOUBLE:
                    {
                        double fvalue = (out->value.u.double_);
                        //asprintf(&out->json, "\"%s\": %lf",
                        //        out->node.name, fvalue);
                        asprintf(&out->json, "%lf", fvalue);
                        out->value.format = MPV_FORMAT_STRING;
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        out->json = out->value.u.string;
                        break;
                    }
            }
        }else{
            // case already handled in property_update
            out->json = out->value.u.string;
        }
    }else if (out->format_out == MPV_FORMAT_NODE_MAP
            || out->format_out == MPV_FORMAT_NODE_ARRAY)
    {
        // case already handled in property_update
        out->json = out->value.u.string;
    }else{
        if( out->value.format == out->format_out ){
            switch(out->value.format){
                case MPV_FORMAT_INT64:
                    {
                        asprintf(&out->json, "%ld", out->value.u.int64);
                        break;
                    }
                case MPV_FORMAT_FLAG:
                    {
                        asprintf(&out->json, "\"%s\"", out->value.u.flag?"yes":"no");
                        break;
                    }
                case MPV_FORMAT_DOUBLE:
                    {
                        asprintf(&out->json, "%lf", out->value.u.double_);
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        // case already handled in property_update
                        out->json = out->value.u.string;
                        break;
                    }
            }
        }else{
            // case already handled in property_update
            out->json = out->value.u.string;
        }
    }

    assert(out->json != NULL);
}

/* Converts input into json key-value-pair.  */
void property_update(
        __status *status,
        mpv_event_property *in,
        __property *out)
{
    char changed = 0;

    //ONION_DEBUG("\nFormat in: %d\nFormat out: %d\nFormat expected:%d\n",
    //        in->format, out->format_out, out->node.format);

    pthread_mutex_lock(&status->lock);

    // If out->value maps on dynamic allocated data we 
    // need to copy content now. Otherwise, we can delay/omit its parsing.
    if (in->format == MPV_FORMAT_NONE){
        // no data avail for this mpv property. Gen dummy entries.
        /* Note 'chapter' has this format if 'chapters' is empty.
         * we can not simply ignore None-Nodes because
         * we wait on all properties if the observertion has started.
         */
        out->value.format = MPV_FORMAT_NONE;
        changed = 1;

    }else if (in->format == MPV_FORMAT_NODE){
        mpv_node *result_node = (mpv_node *)(in->data);
        out->value = *result_node;

        if( result_node->format == out->format_out ){
            switch(result_node->format){
                case MPV_FORMAT_INT64:
                case MPV_FORMAT_FLAG:
                case MPV_FORMAT_DOUBLE:
                    {
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        char *svalue = (result_node->u.string);
                        //asprintf(&out->json, "\"%s\": \"%s\"",
                        //        out->node.name, svalue);
                        // Store escaped variant between quotes
                        out->value.format = MPV_FORMAT_STRING;
                        out->value.u.string = utf8_quote_with_escape(svalue, '"');
                        changed = 2;
                        break;
                    }
            }
        }else{
            if ( result_node->format == MPV_FORMAT_STRING &&
                    out->format_out == MPV_FORMAT_FLAG ){
                // This will be reached if loop_playlist is set to "inf"
                // Maybe a wrong value for a flag?! (Copied bug)
                char *svalue = (result_node->u.string);
                out->value.format = MPV_FORMAT_STRING;
                out->value.u.string = utf8_quote_with_escape(svalue, '"');
                changed = 2;
            }else{
                ONION_ERROR("mpv_node missmatch. Format in: %d, Format out: %d",
                        result_node->format, out->format_out );
                out->value.format = MPV_FORMAT_STRING;
                out->value.u.string = strdup("???");
                changed = 2;
            }
        }
    }else if (out->format_out == MPV_FORMAT_NODE_MAP
            || out->format_out == MPV_FORMAT_NODE_ARRAY)
    {
        assert( in->format == MPV_FORMAT_STRING );
        char *svalue = *(char **)(in->data);
        out->value.format = MPV_FORMAT_STRING;
        // Print lists and dicts without quotes
        //asprintf(&out->value.u.string, "\"%s\": %s",
        //        out->node.name, svalue);
        out->value.u.string = strdup(svalue);
        // NOTE: Array is already converted in a json-string
        // No further escaping required/useful.
        changed = 2;
    }else{
        assert( in->format == out->format_out );
        if (in->format == out->format_out) {
            out->value.format = in->format;
            switch(out->value.format){
                case MPV_FORMAT_INT64:
                    {
                        int64_t *ivalue = *(int64_t **)(in->data);
                        out->value.u.int64 = *ivalue;
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_FLAG:
                    {
                        int *bvalue = *(int **)(in->data);
                        out->value.u.flag = *bvalue;
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_DOUBLE:
                    {
                        double *fvalue = *(double **)(in->data);
                        out->value.u.double_ = *fvalue;
                        changed = 1;
                        break;
                    }
                case MPV_FORMAT_STRING:
                    {
                        char *svalue = *(char **)(in->data);
                        out->value.format = MPV_FORMAT_STRING;
                        //asprintf(&out->value.u.string, "\"%s\": \"%s\"",
                        //        out->node.name, svalue);
                        // Store escaped variant between quotes
                        out->value.u.string = utf8_quote_with_escape(svalue, '"');
                        changed = 2;
                        break;
                    }
            }
        }else{
            if ( in->format == MPV_FORMAT_STRING &&
                    out->format_out == MPV_FORMAT_FLAG ){
                // This will be reached if loop_playlist is set to "inf"
                // Maybe a wrong value for a flag?! (Copied bug)
                char *svalue = *(char **)(in->data);
                out->value.format = MPV_FORMAT_STRING;
                out->value.u.string = utf8_quote_with_escape(svalue, '"');
                changed = 2;
            }else{
                ONION_ERROR("mpv_node missmatch. Format in: %d, Format out: %d",
                        in->format, out->format_out );
                out->value.format = MPV_FORMAT_STRING;
                out->value.u.string = strdup("???");
                changed = 2;
            }
        }
    }

    /* changed == 2: New json string is ready.
     * changed == 1: New json string can be generated from out->value
     */
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
        ONION_DEBUG("Init: %d/%d", status->num_initialized, status->num_props);
        // Ignore update_interval_ms
        // during initialization, but parse directly.
        parse_value(out);
    }

    if ( status->is_initialized  == 0
            && status->num_initialized >= status->num_props){
        pthread_mutex_unlock(&status->lock);
        status_set_initialized(status);
    }else{
        pthread_mutex_unlock(&status->lock);
    }
}


__status *status_init()
{
    __status *status = calloc(sizeof(__status), 1);

    status->props = calloc(sizeof(__property), NUM_PROPS);
#define ADD(NAME, TYPE, TYPE_OUT, UPDATE_INTERVAL) \
    if( pos<NUM_PROPS ) {\
        status->props[pos++] = property_init(\
                NAME, TYPE, TYPE_OUT,\
                UPDATE_INTERVAL, \
                REPLY_ID_OFFSET + pos); } \
    else { printf("Increase NUM_PROPS"); }
    //TYPE_OUT needs to be the internal type of the property
    //in mpv if TYPE is MPV_FORMAT_NODE.
    int pos = 0;
    int ms = ws_interval;
    //ADD("filename", MPV_FORMAT_NODE, MPV_FORMAT_STRING); // encoding problem?!
    ADD("filename", MPV_FORMAT_STRING, MPV_FORMAT_STRING, 0);
    ADD("duration", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, ms);
    ADD("time-pos", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, ms);
    // time-remaining = duration - time-pos
    ADD("playtime-remaining", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, ms);  //scaled by speed
    //ADD("playback-time", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, 0);  // redundant and scaled by speed
    //ADD("metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_STRING, 0);
    ADD("metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP, 0);
    ADD("volume", MPV_FORMAT_NODE,  MPV_FORMAT_DOUBLE, ms);
    ADD("volume-max", MPV_FORMAT_NODE,  MPV_FORMAT_DOUBLE, 0);
    ADD("playlist", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP, 0);
    ADD("track-list", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP, 0);
    // Format of chapter list: [{'title':'chapter 1', 'time': 1.0},...]
    ADD("chapter-list", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP, 0)
    ADD("loop-file", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG, 0);
    ADD("loop-playlist", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG, 0);
    ADD("chapters", MPV_FORMAT_NODE, MPV_FORMAT_INT64, 0);  // no list of  chapter titles, but number

    ADD("sub-delay", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, 0);
    //ADD("sub-delay", MPV_FORMAT_DOUBLE, MPV_FORMAT_STRING); // not '[num] ms' but 0.00…
    ADD("audio-delay", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, 0);
    ADD("speed", MPV_FORMAT_NODE, MPV_FORMAT_DOUBLE, 0);

    ADD("pause", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG, 0);
    ADD("fullscreen", MPV_FORMAT_NODE,  MPV_FORMAT_FLAG, 0);
    ADD("chapter", MPV_FORMAT_NODE, MPV_FORMAT_INT64, 0);
    ADD("chapter-metadata", MPV_FORMAT_NODE_MAP, MPV_FORMAT_NODE_MAP, 0); // like metadata, but rare. Usually chapter title only.
    // chapter-metadata redundant to chapter-list entry..
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
        __status *status)
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
        __status *status)
{
    if (status->is_observed) {
        return 0;
    }

    pthread_mutex_lock(&status->lock);
    int pos;
    for( pos=0; pos<status->num_props; ++pos){
        __property *prop = &status->props[pos];
        prop->initialized = 0;
        prop->updated = 0;
        prop->next_update_time = (struct timespec){.tv_sec=0, .tv_nsec=0};
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


int __property_observe(__property *prop){
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
#endif
    return 0;
}

// To trigger fetching of data again.
int __property_reobserve(__property *prop){
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
        __property *prop = &status->props[pos];
        if (0 == strcmp(prop->node.name, prop_name)){
            return __property_reobserve(prop);
        }
    }
    ONION_ERROR("No property '%s' observed.", prop_name);
    return -1;
}

void status_unobserve(
        mpv_handle *mpv,
        __status *status)
{
    pthread_mutex_lock(&status->lock);
    if (status->is_observed) {

        int pos;
        for( pos=0; pos<status->num_props; ++pos){
#ifdef WITH_MPV
            __property *prop = &status->props[pos];
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
        __status *status,
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
    //ONION_DEBUG("update observed property '%s'", prop_in->name);
    __property *prop_out = &status->props[prop_index];
    property_update(status, prop_in, prop_out);
}

void send_to_all_clients(
        __clients *pclients,
        __property *prop)
{
    int i;

#if 0
    // Test PING/PONG mechanism
    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket *pclient = &pclients->clients[i];
        if ( pclient->ws ){
            char pl[] = "payload";
            ONION_DEBUG("Send PING to client %d", i);
            onion_websocket_set_opcode(pclient->ws, OWS_PING);
            ssize_t s = onion_websocket_write(pclient->ws,
                    pl, strlen(pl));
        }
    }
    return;
#endif

    for( i=0; i<MAX_ACTIVE_CLIENTS; ++i){
        __websocket *pclient = &pclients->clients[i];
        if ( pclient->ws ){
            //ONION_DEBUG("Send websocket msg");
            pthread_mutex_lock(&pclient->lock);
            __chunked_websocket_printf(pclient->ws,
                    "{\"status_diff\": { \"%s\": %s }}",
                    prop->node.name, prop->json);
            pthread_mutex_unlock(&pclient->lock);
        }
    }
}


void status_build_full_json(
        __status *status)
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
            __property *prop = &status->props[n];
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
        __status *status)
{
    ONION_DEBUG("Mark status as initialized.");
    pthread_mutex_lock(&status->lock);
    if (status->is_initialized == 0 ){
      status->is_initialized = 1;
      pthread_cond_signal(&status->init_done);
    }
    pthread_mutex_unlock(&status->lock);

}

/*
 * Checks if proprety was updated by mpv
 * and potential update interval restritcions are met.
 */
int __prop_ready(__property *prop, struct timespec *now, int force){
    if (!prop->updated) return 0;
    if (!force && !time_diff_reached(prop, now)) return 0;
    return 1;
}

void status_send_update(
        __status *status,
        __clients *pclients,
        int force)
{
    if (status->is_initialized == 0 ){
      ONION_ERROR("Should not be reached");
      return;
    }

    struct timespec now;
    fetch_timestamp(&now);

    // Send updates with single properties
    int n, N=status->num_props;
    for(n=0; n<N; ++n){
        __property *prop = &status->props[n];
        if (__prop_ready(prop, &now, force)){

            parse_value(prop);

            // send single property

            pthread_mutex_lock(&status->lock);
            send_to_all_clients(pclients, &status->props[n]);

            status->props[n].updated = 0;
            status->num_updated--;

            // Update timestamp
            update_timestamp(prop, &now);

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

      //onion_websocket_set_opcode(ws, OWS_TEXT);
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
