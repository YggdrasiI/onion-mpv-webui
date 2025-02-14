#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
//#include <libintl.h>
#include <assert.h>
#include <wordexp.h>

#include <onion/onion.h>
#include <onion/log.h>
#include <onion/dict.h>
#include <onion/url.h>
#include <onion/mime.h>
#include <onion/shortcuts.h>
#ifdef ONION_BUILD
#include <onion/handlers/exportlocal.h>
#include <onion/handlers/static.h>
#else
#include <onion/exportlocal.h>
#include <onion/static.h>
#endif

#include <onion/dict.h>
#include <onion/shortcuts.h>

#include "defines.h"
#include "mpv_api_commands.h"
#include "mpv_script_options.h"
#include "tools.h"
#include "buffer_pool.h"
#include "fileserver.h"
#include "media.h"
#include "onion_ws_status.h"
#include "onion_default_errors.h"



onion *o = NULL;
onion_dict *commands;
onion_dict *media_commands;
onion_dict *options;
int webui_onion_running = 0;

#ifdef WITH_MPV
extern pthread_mutex_t mpv_lock;
#else
pthread_mutex_t mpv_lock;
#endif

// Ring buffer to store output of some sprintf commands.
buffer_pool_t *path_buffers;

// Websocket stuff
__status *status;
__clients *websockets;

int webui_onion_is_running(){
  return webui_onion_running;
}
void webui_onion_set_running(int bRun){
    webui_onion_running = bRun;
}

void webui_onion_end_signal(int unused) {
    webui_onion_set_running(0);
    if( o ){
        clients_close(status, websockets);

        onion_listen_stop(o);
        //exit(EXIT_SUCCESS); // bad because exit() is not signal save
    }
}


onion_connection_status handle_status_get(void *d, onion_request * req, onion_response * res){
    onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

#ifdef WITH_MPV
    char *json = json_status_response();

    onion_connection_status ret = onion_shortcut_response(json, 200, req, res);
    free(json);
    return ret;
#else
    return onion_shortcut_response("{\"info\":\"Compiled without mpv\"}", 200, req, res);
#endif
}

/**
 * It checks if its a HEAD in which case it writes the headers,
 * and if not just printfs the user data.
 */
onion_connection_status handle_api_get_or_post_data(
        void *_, onion_request * req, onion_response * res)
{
  if (onion_request_get_flags(req) & OR_HEAD) {
    onion_response_write_headers(res);
    return OCS_PROCESSED;
  }

  onion_response_set_header(res, "Content-Type", onion_mime_get("_.json"));

  /* Get data */
  // Default variant: Api command encoded in path
  const char *api_args = consume_leading_slashs(onion_request_get_path(req));

  // Debugging variant:  Get string provided by form on api.html
  const char *api_args_by_form = onion_request_get_post(req, "api_args");
  if (api_args_by_form) {
      // use form input, but stip prefix if included.
      const char *prefix = "/api/";
      if ( api_args_by_form == strstr(api_args_by_form, prefix)){
          api_args = api_args_by_form + strlen(prefix);
      }else{
          api_args = api_args_by_form;
      }
  }

  /* Check data */
  if( api_args == NULL || strlen(api_args) == 0 ){
      return onion_shortcut_response(
              "{\"message\":\"Empty pattern posted.\"}",
              HTTP_BAD_REQUEST, req, res);
  }

  // Active set of commands
  onion_dict *current_commands = commands;
  // check for 'media/{suffix}' pattern. Then,
  // use media_commands for suffix.
  if ( api_args == strstr(api_args, "media/")){
      api_args = api_args + strlen("media/");
      current_commands = media_commands;
  }


  /* Handle data */
  onion_connection_status ret = OCS_NOT_PROCESSED;

  // split url into command/param1/param2/[...]


  char *substrings[3] = {NULL, NULL, NULL}; // *command, *param1, *param2;
  int n_consumed = -1;
  char sep[2] = {'\0', '\0'};
  int n = sscanf(
          api_args
          //, "%m[^/]%[/]%m[^/]%[/]%n" //  shrinks "//…" to "/"
          // (Attention, %[/] could be multiple chars wide )
          , "%m[^/]%c%m[^/]%c%n" // Do not allow empty cmd,param1
          , &substrings[0]
          , &sep[0]
          , &substrings[1]
          , &sep[1]
          , &n_consumed
          );

  switch (n){
      default:
      case 0: // "/..."
          substrings[0] = strdup(""); // no cmd
          substrings[1] = strdup(""); // no param1
          substrings[2] = strdup(""); // no param2
          break;
      case 1: // "cmd"
          substrings[1] = strdup(""); // no param1
          substrings[2] = strdup(""); // no param2
          break;
      case 2: // "cmd/"
          substrings[1] = strdup(""); // empty param1
          substrings[2] = strdup(""); // no param2
          break;
      case 3: // "cmd/param1"
          substrings[2] = strdup(""); // no param2");
          break;
      case 4: // "cmd/param1/" and "cmd/param1/[arbitary non empty str]
          if (strlen(api_args + n_consumed) == 0){
              substrings[2] = strdup(""); // empty param2
          }else{
              substrings[2] = strdup(api_args + n_consumed);
          }
          break;
  }


  if( n < 1 ){
      ret = onion_shortcut_response(
              "{\"message\":\"Api pattern not matched. Is 'cmd' empty?\"}",
              HTTP_BAD_REQUEST, req, res);
  }else{
      CommandHandler cmd = (CommandHandler) onion_dict_get(
              current_commands, substrings[0]);

      if( cmd ){
          char *output_message = NULL;

          pthread_mutex_lock(&mpv_lock);
          ONION_DEBUG("Locked for %s %s %s\n",
                  substrings[0],
                  substrings[1], substrings[2]);
          int cmd_status = (cmd)(substrings[0],
                  substrings[1], substrings[2],
                  &output_message);
          pthread_mutex_unlock(&mpv_lock);
          ONION_DEBUG("Unlocked for %s %s %s\n",
                  substrings[0],
                  substrings[1], substrings[2]);

          if (cmd_status > 0){
							//onion_response_set_code(res, HTTP_OK); // default
              onion_response_printf(res,
                      "{\"message\": \"success\"}");
              ret = OCS_PROCESSED;
          }else{
							onion_response_set_code(res, HTTP_BAD_REQUEST);
              onion_response_printf(res,
                      "{\"message\": \"%s\"}",
                      output_message);
              ret = OCS_PROCESSED;
          }
          FREE(output_message);
      }else{
					onion_response_set_code(res, HTTP_BAD_REQUEST);
          onion_response_printf(res,
                  "{\"message\": \"Command '%s' not defined.\"}",
                  substrings[0]);
					ret = OCS_PROCESSED;
      }
  }

  // Cleanup
  int m = 3;
  while(m--){
      FREE(substrings[m]);
  }

  return ret;
}

onion_connection_status handle_api_post_data2(void *_, onion_request * req,
        onion_response * res) {

    onion_connection_status ret = onion_shortcut_redirect("api.html", req, res);

    // TODO This will produce an unwanted a second reply...
    handle_api_get_or_post_data(NULL, req, res);
    return ret;
}

// Sugar: Swap wrong argument order /api/media => /media/api
onion_connection_status swap_args_redirect(const char *new_prefix,
        onion_request * req,
        onion_response * res) {

    char *new_url = strdup(onion_request_get_fullpath(req));
    memcpy(new_url, new_prefix, strnlen(new_prefix, 20));
    onion_connection_status ret = onion_shortcut_redirect(new_url, req, res);
    free(new_url);
    return ret;
}

// similar to handle_api_get_or_post_data
char *handle_command_p2(
        const char *api_args)
/*        const char *command,
        const char *param1,
        const char *param2)
        */
{

    // Active set of commands
    onion_dict *current_commands = commands;
    // check for 'media/{suffix}' pattern. Then,
    // use media_commands for suffix.
    if ( api_args == strstr(api_args, "media/")){
        api_args = api_args + strlen("media/");
        current_commands = media_commands;
    }


    char *output = NULL;
    char *message = NULL;

    char *substrings[3] = {NULL, NULL, NULL}; // *command, *param1, *param2;
    int n_consumed = -1;
    char sep[2] = {'\0', '\0'};
    int n = sscanf(
            api_args
            //, "%m[^/]%[/]%m[^/]%[/]%n" //  shrinks "//…" to "/"
            // (Attention, %[/] could be multiple chars wide )
            , "%m[^/]%c%m[^/]%c%n" // Do not allow empty cmd,param1
            , &substrings[0]
            , &sep[0]
            , &substrings[1]
            , &sep[1]
            , &n_consumed
            );

    switch (n){
        default:
        case 0: // "/..."
            substrings[0] = strdup(""); // no cmd
            substrings[1] = strdup(""); // no param1
            substrings[2] = strdup(""); // no param2
            break;
        case 1: // "cmd"
            substrings[1] = strdup(""); // no param1
            substrings[2] = strdup(""); // no param2
            break;
        case 2: // "cmd/"
            substrings[1] = strdup(""); // empty param1
            substrings[2] = strdup(""); // no param2
            break;
        case 3: // "cmd/param1"
            substrings[2] = strdup(""); // no param2");
            break;
        case 4: // "cmd/param1/" and "cmd/param1/[arbitary non empty str]
            if (strlen(api_args + n_consumed) == 0){
                substrings[2] = strdup(""); // empty param2
            }else{
                substrings[2] = strdup(api_args + n_consumed);
            }
            break;
    }


    if( n < 1 ){
        output = strdup("{\"message\":\"Api pattern not matched. Is 'cmd' empty?\"}");
    }else{
        CommandHandler cmd = (CommandHandler) onion_dict_get(
                current_commands, substrings[0]);

        if( cmd ){
            char *output_message = NULL;

            pthread_mutex_lock(&mpv_lock);
            ONION_DEBUG("Locked for %s %s %s\n",
                    substrings[0],
                    substrings[1], substrings[2]);
            int cmd_status = (cmd)(substrings[0],
                    substrings[1], substrings[2],
                    &message);
            pthread_mutex_unlock(&mpv_lock);
            ONION_DEBUG("Unlocked for %s %s %s\n",
                    substrings[0],
                    substrings[1], substrings[2]);


            if (cmd_status > 0){
                asprintf(&output, 
                        "{\"message\": \"success\"}");
            }else{
                asprintf(&output, 
                        "{\"message\": \"%s\"}",
                        message);
            }
        }else{
            asprintf(&output, 
                    "{\"message\": \"Command '%s' not defined.\"}",
                    substrings[0]);
        }
    }

    // Cleanup
    int m = 3;
    while(m--){
        FREE(substrings[m]);
    }

    return output;
}

onion_dict *setup_commands(){
  onion_dict *_commands = onion_dict_new();

  onion_dict_add(_commands, CMD(play), 0);
  onion_dict_add(_commands, CMD(pause), 0);
  onion_dict_add(_commands, CMD(toggle_pause), 0);
  onion_dict_add(_commands, CMD(toggle_mute), 0);
  onion_dict_add(_commands, CMD(fullscreen), 0);
  onion_dict_add(_commands, CMD(seek), 0);
  onion_dict_add(_commands, CMD(set_position), 0);
  onion_dict_add(_commands, CMD(playlist_prev), 0);
  onion_dict_add(_commands, CMD(playlist_next), 0);
  onion_dict_add(_commands, CMD(playlist_jump), 0);
  onion_dict_add(_commands, CMD(playlist_remove), 0);
  onion_dict_add(_commands, CMD(playlist_move), 0);
  onion_dict_add(_commands, CMD(playlist_move_up), 0);
  onion_dict_add(_commands, CMD(playlist_shuffle), 0);
  onion_dict_add(_commands, CMD(loop_file), 0);
  onion_dict_add(_commands, CMD(loop_playlist), 0);
  onion_dict_add(_commands, CMD(add_volume), 0);
  onion_dict_add(_commands, CMD(set_volume), 0);
  onion_dict_add(_commands, CMD(add_sub_delay), 0);
  onion_dict_add(_commands, CMD(set_sub_delay), 0);
  onion_dict_add(_commands, CMD(add_audio_delay), 0);
  onion_dict_add(_commands, CMD(set_audio_delay), 0);
  onion_dict_add(_commands, CMD(cycle), 0);
  onion_dict_add(_commands, CMD(set_subtitle), 0);
  onion_dict_add(_commands, CMD(set_audio), 0);
  onion_dict_add(_commands, CMD(set_video), 0);
  onion_dict_add(_commands, CMD(add_chapter), 0);
  onion_dict_add(_commands, CMD(set_chapter), 0);
  onion_dict_add(_commands, CMD(increase_playback_speed), 0);
  onion_dict_add(_commands, CMD(reset_playback_speed), 0);
  onion_dict_add(_commands, CMD(quit), 0);
  onion_dict_add(_commands, CMD(playlist_add), 0);
  return _commands;
}


int webui_onion_init(onion_dict *_options) {

  options = _options;

  //onion_log=onion_log_syslog;
  //char *port = "9000";
  //char *hostname = "::";
  //const char *dirname = "./webui-page";

  /* Localization setup */
#if 0
  // This is the root directory where the translations are.
#define W "."
  setenv("LANGUAGE", "locale", 1);      // Remove LANGUAGE env var, set it to the locale name,
  //setlocale(LC_ALL, ""); // This would disturb the floating number parsing
                           // inside mpv-event loop!
  setlocale(LC_MESSAGES, "");
  bindtextdomain("locale", W);  // This is necesary because of the fake name

  const char *langs[] = { "es", "zh", "fr", "pl", "de"};
  // …add more languages here…

  for( int i=0; i<sizeof(langs)/sizeof(langs[0]); ++i){
      bindtextdomain(langs[i], W);

      // Hm, in contrast to ofileserver example, the following
      // line is required...
      bind_textdomain_codeset(langs[i], "utf-8");
  }

  textdomain("C");              // Default language
  // All is configured now, now in hands of dgettext(LANG, txt);
#endif

  // Disable DEBUG of libonion output
  if ('0' == onion_dict_get(options, "debug")[0]) {
      onion_log_flags |= OF_NODEBUG;
      //onion_log_flags |= OF_NOINFO;
  }

  /* Note that PATH_MAX does not guarantee that arbitary
   * paths are not longer than this value. E.g. an absolute
   * path of a relative path could be longer. So still
   * check the boundaries on writing into 'path_buffers'.
   * See https://eklitzke.org/path-max-is-tricky
   */
  path_buffers = buffer_pool_new(10, PATH_MAX);

  /* Struct for websocket status messages
   */
  status = status_init();
  websockets = clients_init();

  /* Fill dict(s) with function handlers to available commands. */
  commands = setup_commands();
  media_commands = setup_media_commands();

  /* // Not required.
  if (pthread_mutex_init(&mpv_lock, NULL) != 0) {
      perror("\n mutex init failed\n");
      return -3;
  }*/

  /* Create onion instance */
  //o = onion_new(O_THREADED|O_POOL|O_DETACH_LISTEN);
  //o = onion_new(O_DETACH_LISTEN);
  o = onion_new(O_THREADED|O_DETACH_LISTEN|O_NO_SIGTERM); // requires mutex lock on mpv access?!

  //Connect url pattern with handlers. Also added the empty rule that redirects to static/index.html
  onion_url *urls = onion_url_new();
  onion_url *api = onion_url_new();

  onion_url_add_url(urls, "^api/", api);  // with slash to handle 'api' by static_file provider

  /* Dynamic content */
  onion_url_add(urls, "ws", ws_status_start);  // websocket
  onion_url_add_static(api, "version", VERSION, HTTP_OK);
  onion_url_add(api, "status", handle_status_get);

  onion_url_add_with_data(api, "^media", swap_args_redirect, "/media/api", NULL);

  // This as last to not overrule the other rules.
  onion_url_add(api, "^", handle_api_get_or_post_data);   // "/api"
  //onion_url_add_with_data(api, "^/media", onion_shortcut_internal_redirect, "/index.html", NULL);


  onion_url *media = onion_url_new();
  onion_url_add_url(urls, "^media", media);
  /* Add handlers for each shared folder */
  /* /media/<share>/.* and /media/api/<cmd>/<share>/.* */
  webui_onion_share_media_folders(media, options);


  /* Redirect to index.html on top level */
  onion_url_add_with_data(urls, "", onion_shortcut_internal_redirect, "/index.html", NULL);

  onion_url_add(urls, "api2", handle_api_post_data2); // "/api2"
  /* /api2 evaluates POST content of api.html and redirects to api.html
   * after handling command */

  // TODO: Remove json reply before html of redirection
  onion_url_add_static(urls, "api.html",
                       "<html>\n"
                       "<head>\n"
                       " <title>Gen Api post string</title>\n"
                       "</head>\n"
                       "\n"
                       "Write something: \n"
                       "<form method=\"POST\" action=\"api2\">\n"
                       "<input type=\"text\" name=\"api_args\" value=\"/api/\">\n"
                       "<input type=\"submit\">\n"
                       "</form>\n" "\n" "</html>\n", HTTP_OK);


  // Finally, one handler for all other requests
  webui_onion_static_files(
          onion_dict_get(options, "root_dir"),
          "^",
          urls);
  // "^" doesn't remove url parts from path in internal header,
  // "^.*" removes everything (bad!)


  // #############################################################

  onion_set_root_handler(o, onion_url_to_handler(urls));
  onion_set_port(o, onion_dict_get(options, "port"));
  onion_set_hostname(o, onion_dict_get(options, "hostname"));

  // Overwrite internal error handler
  webui_wrap_onion_internal_error_handler(o);

  signal(SIGINT, webui_onion_end_signal);
  signal(SIGTERM, webui_onion_end_signal);

  // Check if threaded
  int flags = onion_flags(o);
  if ((flags & O_THREADS_AVAILABLE) == 0){
      perror("No thread support available. Onion server listening will block.\n");
      return -2;
  }
  if ((flags & O_THREADS_ENABLED) == 0){
      perror("Thread support not enabled. Onion server listening will block.\n");
      return -1;
  }

  return 0;
}

void webui_onion_uninit(onion *o){

#ifdef WITH_MPV
    // Quit mpv instance
    CommandHandler cmd = (CommandHandler) onion_dict_get(
            commands, "quit");
    if( cmd ){
        pthread_mutex_lock(&mpv_lock);
        int cmd_status = (cmd)("quit", "0", "", NULL); // crashs
        pthread_mutex_unlock(&mpv_lock);
    }
#endif

    onion_free(o);
    onion_dict_free(commands); commands = NULL;
    onion_dict_free(media_commands); media_commands = NULL;
    buffer_pool_free(&path_buffers);
    //pthread_mutex_destroy(&mpv_lock);

    status_free(status);
    clients_uninit(websockets);
}
