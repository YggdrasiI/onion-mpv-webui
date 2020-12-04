#define _GNU_SOURCE

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <onion/onion.h>
#include <onion/log.h>

#include "webui_onion.h"
#include "mpv_script_options.h"

extern onion *o;

#include "onion_ws_status.h"
extern __clients_t *websockets;

int show_help() {
  printf
      ("Onion basic fileserver (otemplate edition). (C) 2011 Coralbits S.L.\n\n"
       "Usage: fileserver [options] [directory to serve]\n\n" "Options:\n"
       "  --port N\n" "   -p N           Listens at given port. Default 9000\n"
       "  --listen ADDRESS\n"
       "   -l ADDRESS     Listen to that address. It can be a IPv4 or IPv6 IP, or a local host name. Default: 0.0.0.0\n"
       "  --help\n" "   -h             Shows this help\n"
       "\n" "\n");
  return 0;
}

int parse_args(onion_dict *options, int argc, char **argv){

  int i;
  /* This part is only used in executable without mpv stuff. */
  for (i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "--port") == 0) || (strcmp(argv[i], "-p") == 0)) {
      const char *port = argv[++i];
      onion_dict_add(options, "port", port,
              OD_FREE_VALUE|OD_DUP_VALUE|OD_REPLACE);
      ONION_INFO("Listening at port %s", port);
    }
    else if ((strcmp(argv[i], "--listen") == 0) || (strcmp(argv[i], "-l") == 0)) {
      const char *hostname = argv[++i];
      onion_dict_add(options, "hostname", hostname,
              OD_FREE_VALUE|OD_DUP_VALUE|OD_REPLACE);
      ONION_INFO("Listening at hostname %s", hostname);
    }
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      show_help();
      return -1;
    }
    else if (argv[i][0] != '-') {
      const char *dirname = argv[i];
      onion_dict_add(options, "root_dir", dirname,
              OD_FREE_VALUE|OD_DUP_VALUE|OD_REPLACE);
    }else {
      ONION_INFO("Unhandled option/argument: '%s'", argv[i]);
    }
  }
  ONION_INFO("Exporting directory %s",
          onion_dict_get(options, "root_dir"));

  return 0;
}


int main(int argc, char **argv) {

    int ret = 0;
    
    onion_dict *options = get_default_options();
    if (parse_args(options, argc, argv)){
        return 0; // quit after help text is shown.
    }

    webui_onion_init(options);

    int error = onion_listen(o);
    if (error) {
        perror("Cant create the server.\n");
        ret = -1;
        goto cleanup;
    }

    webui_onion_set_running(1);
    while (webui_onion_is_running()) {
        // Wait on Ctrl+C
        usleep(1000*100);
    }
    printf("webui_onion interrupted\n");

cleanup:
    printf("Stop listening\n");
    onion_listen_stop(o);
    webui_onion_uninit(o);
    onion_dict_free(options);
    return ret;
}
