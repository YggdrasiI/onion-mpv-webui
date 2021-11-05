#include <onion/websocket.h>
#include <pthread.h>

#include <mpv/client.h>

#define MAX_ACTIVE_CLIENTS 20
#define NUM_PROPS 50
#define REPLY_ID_OFFSET 10000

// ================== BEGIN onion releated part ===============
typedef struct __websocket_t {
    onion_websocket *ws;
    pthread_mutex_t lock;
} __websocket_t;

typedef struct __clients_t {
    __websocket_t clients[MAX_ACTIVE_CLIENTS];
    int num_active_clients;
    pthread_mutex_t lock;
} __clients_t;


__clients_t *clients_init();


void clients_uninit(
        __clients_t *pclients);


__websocket_t *add_client(
        __clients_t *pclients,
        onion_websocket *ws);


int remove_client(
        __clients_t *pclients,
        onion_websocket *ws);


// url handler
onion_connection_status ws_status_start(
        void *data,
        onion_request * req,
        onion_response * res);

onion_connection_status ws_status_cont(
        void *data,
        onion_websocket * ws,
        ssize_t data_ready_len);


// ================== END onion releated part ===============

// ================== BEGIN mpv releated part ===============
typedef struct __property_t {
    // See mpv_event_property for this variables
    mpv_event_property node;
    /*
       const char *name;
       mpv_format format;
       void *data;
       */

    // extra elements
    mpv_format format_out;
    char initialized;
    char updated;

    char *json;

} __property_t;

typedef struct __status_t {
  char *json;
  int num_props;
  __property_t* props;

  int num_initialized;
  int num_updated;

  int is_initialized;
  int is_observed;

  pthread_mutex_t lock;
  pthread_mutex_t init_in_process;
  pthread_cond_t init_done;
} __status_t;



__property_t property_init(
        const char *name,
        mpv_format format,
        mpv_format format_out);

void property_free(
        __property_t *property);

/* Converts input into json key-value-pair.  */
void property_update(
        __status_t *status,
        mpv_event_property *in,
        __property_t *out);


__status_t *status_init();


void status_free(
        __status_t *status);


int status_observe(
        mpv_handle *mpv,
        __status_t *status);


void status_unobserve(
        mpv_handle *mpv,
        __status_t *status);


void status_update(
        __status_t *status,
        mpv_handle *mpv,
        mpv_event *event);


void status_set_initialized(
        __status_t *status);

void status_send_update(
        __status_t *status,
        __clients_t *pclients);


// intern
void status_build_full_json(
        __status_t *status);

void clients_close(
        __status_t *status,
        __clients_t *pclients);


// ================== END mpv releated part ===============

/*
 * Enclose utf-8 input string 'src' with quotes and also escape
 * the quote character.
 * Example:  'ab"cd' -> "ab\"cd",
 *
 * Note that multichar wide characters will not be
 * destroyed even if one byte matches 'quote_char'.
 *
 * Free return result after usage.
 */
char * utf8_quote_with_escape(char *src, char quote_char);

/*
   char *json = NULL;

// Note: LHS variable names with '_', but RHS properties with '-'
char *filename = mpv_get_property_string(mpv, "filename");
char *duration = mpv_get_property_string(mpv, "duration");
char *position = mpv_get_property_string(mpv, "time-pos");
char *remaining = mpv_get_property_string(mpv, "playtime-remaining");
char *metadata = mpv_get_property_string(mpv, "metadata");
char *volume = mpv_get_property_string(mpv, "volume");
char *volume_max = mpv_get_property_string(mpv, "volume-max");
char *playlist = mpv_get_property_string(mpv, "playlist");
char *track_list = mpv_get_property_string(mpv, "track-list");
char *loop_file = mpv_get_property_string(mpv, "loop-file");
char *loop_playlist = mpv_get_property_string(mpv, "loop-playlist");
char *chapters = mpv_get_property_string(mpv, "chapters");

char *sub_delay = mpv_get_property_osd_string(mpv, "sub-delay");
char *audio_delay = mpv_get_property_osd_string(mpv, "audio-delay");
// Note: lua-scripts clip last 3 chars of this to variables

int pause;
if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pause) < 0){
perror("Can not fetch pause property.\n");
goto gen_status_error;
}

int fullscreen;
if (mpv_get_property(mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen) < 0){
perror("Can not fetch fullscreen property.\n");
goto gen_status_error;
}

//char *chapter = "0";
int64_t chapter = 0;
if (chapters != NULL && chapters[0] != '0'){
//chapter = mpv_get_property_string(mpv, "chapter");
if (mpv_get_property(mpv, "chapter", MPV_FORMAT_INT64, &chapter) < 0){
perror("Can not fetch chapter property.\n");
}
}

// String parsings
double fduration;
if (sscanf(STR(duration), "%lf", &fduration) != 1){ fduration = 0; }
double fposition;
if (sscanf(STR(position), "%lf", &fposition) != 1){ fposition = 0; }
double fremaining;
if (sscanf(STR(remaining), "%lf", &fremaining) != 1){ fremaining = 0; }
double fvolume;
if (sscanf(STR(volume), "%lf", &fvolume) != 1){ fvolume = 0; }
double fvolume_max;
if (sscanf(STR(volume_max), "%lf", &fvolume_max) != 1){ fvolume_max = 0; }

int isub_delay; // strip ' ms'
if (sscanf(STR(sub_delay), "%d ms", &isub_delay) != 1){ isub_delay = 0; }
int iaudio_delay; // strip ' ms'
if (sscanf(STR(audio_delay), "%d ms", &iaudio_delay) != 1){ iaudio_delay = 0; }

int printf_status = asprintf(&json
, "{"
"\"filename\":"           "\"%s\""
",\n\"duration\":"          "%d"
",\n\"position\":"          "%d"
",\n\"remaining\":"         "%d"
",\n\"metadata\":"          "%s"  // obj
",\n\"volume\":"            "%d"
",\n\"volume-max\":"        "%d"
",\n\"playlist\":"          "%s"  // obj
",\n\"track-list\":"        "%s"  // obj
",\n\"loop-file\":"       "\"%s\""
",\n\"loop-playlist\":"   "\"%s\""
",\n\"chapters\":"          "%s"  // obj

",\n\"sub-delay\":"         "%d"
",\n\"audio-delay\":"       "%d"

",\n\"pause\":"             "%d"
",\n\"fullscreen\":"        "%d"

",\n\"chapter\":"          "%ld"
"}\n"

    , STR(filename)
    , (int)(fduration)
    , (int)(fposition)
    , (int)(fremaining)
    , OBJ(metadata)
    , (int)(fvolume)
    , (int)(fvolume_max)
    , OBJ(playlist)
    , OBJ(track_list)
    , STR(loop_file)
    , STR(loop_playlist)
, OBJ(chapters)

    , isub_delay
    , iaudio_delay

    , pause
    , fullscreen

    , chapter
    );
    */

