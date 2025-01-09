#define _GNU_SOURCE

#include <string.h>

#include <onion/types.h>
#include <onion/response.h>
#include <onion/mime.h>
#include <onion/log.h>

// It exists no onion_get_internal_error_handler
// We directly accesse struct instead of patching libonion.
#include <onion/types_internal.h>

#include "onion_default_errors.h"


// Replacing default error messages by json error message if api was used.
#define ERROR_500_JSON "{\"message\": \"<h1>500 - Internal error</h1> Check server logs or contact administrator.\"}"
#define ERROR_403_JSON "{\"message\": \"<h1>403 - Forbidden</h1>\"}"
#define ERROR_404_JSON "{\"message\": \"<h1>404 - Not found</h1>\"}"
#define ERROR_501_JSON "{\"message\": \"<h1>501 - Not implemented</h1>\"}"

/**
 * @short Default error printer.
 * @ingroup onion
 *
 * Ugly errors, that can be reimplemented setting a handler with onion_set_internal_error_handler.
 */
static int onion_error(void *privdata, onion_request * req,
        onion_response * res) {

    const char *uri = onion_request_get_fullpath(req);
    if ( 0 == strncmp("/api", uri, 4) || 0 == strncmp("/media/api", uri, 10) ) {
        // Continue with this handler and reply with json-error-message
        onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));
    }else{
        // Continue with default handler and reply html-error-message
        onion_handler *internal_error_handler = (onion_handler *) privdata;
        return onion_handler_handle(internal_error_handler, req, res);
    }

    const char *msg;
    int l;
    int code;
    switch ( onion_request_get_flags(req) & 0x0F000) {
        case OR_INTERNAL_ERROR:
            msg = ERROR_500_JSON;
            l = sizeof(ERROR_500_JSON) - 1;
            code = HTTP_INTERNAL_ERROR;
            break;
        case OR_NOT_IMPLEMENTED:
            msg = ERROR_501_JSON;
            l = sizeof(ERROR_501_JSON) - 1;
            code = HTTP_NOT_IMPLEMENTED;
            break;
        case OR_FORBIDDEN:
            msg = ERROR_403_JSON;
            l = sizeof(ERROR_403_JSON) - 1;
            code = HTTP_FORBIDDEN;
            break;
        default:
            msg = ERROR_404_JSON;
            l = sizeof(ERROR_404_JSON) - 1;
            code = HTTP_NOT_FOUND;
            break;
    }

    ONION_DEBUG0("Error: %s, code %d.", msg, code);

    onion_response_set_code(res, code);
    onion_response_set_length(res, l);
    onion_response_write_headers(res);

    onion_response_write(res, msg, l);
    return OCS_PROCESSED;
}

void __clear_wrapped_handler(void *privdata){
    if (privdata){
        onion_handler_free((onion_handler *)privdata);
        privdata = NULL;
    }
}

void webui_wrap_onion_internal_error_handler(onion *o) {

    onion_handler *internal_error_handler = o->internal_error_handler;
    // Or… if libonion header will be extended…
    //onion_handler *internal_error_handler = onion_get_internal_error_handler();
    if( NULL == internal_error_handler ){
        ONION_ERROR("Internal error handler not set. Call this function later!");
        return;
    }

    // Replacing internal error handler, but storing reference to previous value
    // as privdata to call/deconstruct it later.
    onion_handler *error_handler = onion_handler_new(
            (onion_handler_handler) onion_error,
            internal_error_handler, __clear_wrapped_handler);

    ONION_DEBUG("NewH %p", error_handler);
    onion_set_internal_error_handler(o, error_handler);
}
