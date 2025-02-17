/*
 * Watchdog to update bundle if source has been changed.
 *
 *   Based on
 *   https://github.com/jrelo/fs_monitoring/blob/master/inotify-example.c
 *
 *   This program is released in the Public Domain.
 *
 *   Compile with:
 *     $> gcc -o js-bundle-watchdog js-bundle-watchdog.c
 *
 *   Run as:
 *     $> ./js-bundle-watchdog {bundle-file.js} /path/to/monitor /another/path/to/monitor ...
 */
#define _POSIX_SOURCE 1 // for SIG_BLOCK
#define _POSIX_C_SOURCE 200809L // for strdup()
#define _GNU_SOURCE     // for asprintf

#include <libgen.h>  // POSIX dirname, basename

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/signalfd.h>

//#include <linux/inotify.h>
#include <sys/inotify.h>

#include "argparse.h"
#include "hashmap.h"

const char *const usages[] = {
    "basic [options] [[--] args]",
    "basic [options]",
    NULL,
};

/* Structure to keep track of monitored directories */
typedef struct monitored_t {
  /* Path of the observed directory */
  char *path;
  union {
      /* inotify watch descriptor */
      int wd;
      /* ref counter during loop over all monitored paths. */
      int hit;
  };
} monitored_t;

typedef struct path_t {
    /* Path of the directory */
    char *path;
    monitored_t *m;
} path_t;

int monitor_compare_by_wd(const void *a, const void *b, void *udata) {
    const monitored_t *ma = a;
    const monitored_t *mb = b;
    return mb->wd - ma->wd;
}

int monitor_compare_by_path(const void *a, const void *b, void *udata) {
    const monitored_t *ma = a;
    const monitored_t *mb = b;
    return strcmp(ma->path, mb->path);
}

bool monitor_iter(const void *item, void *udata) {
    const monitored_t *m = item;
    printf("%s (wd=%d)\n", m->path, m->wd);
    return true;
}

uint64_t monitor_hash_by_wd(const void *item, uint64_t seed0, uint64_t seed1) {
    const monitored_t *m = item;
    return m->wd;
}

uint64_t monitor_hash_by_path(const void *item, uint64_t seed0, uint64_t seed1) {
    const monitored_t *m = item;
    return hashmap_sip(m->path, strlen(m->path), seed0, seed1);
}

int path_compare(const void *a, const void *b, void *udata) {
    const path_t *pa = a;
    const path_t *pb = b;
    return strcmp(pa->path, pb->path);
}

bool path_iter(const void *item, void *udata) {
    const path_t *p = item;
    printf("path: %s (monitored path=%s)\n", p->path, p->m->path);
    return true;
}

uint64_t path_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const path_t *p = item;
    return hashmap_sip(p->path, strlen(p->path), seed0, seed1);
}

/* Size of buffer to use when reading inotify events */
#define INOTIFY_BUFFER_SIZE 8192

/* Enumerate list of FDs to poll */
enum {
  FD_POLL_SIGNAL = 0,
  FD_POLL_INOTIFY,
  FD_POLL_MAX
};

/* FANotify-like helpers to iterate events */
#define IN_EVENT_DATA_LEN (sizeof(struct inotify_event))
#define IN_EVENT_NEXT(event, length)            \
  ((length) -= (event)->len,                    \
   (struct inotify_event*)(((char *)(event)) +	\
                           (event)->len))
#define IN_EVENT_OK(event, length)                  \
  ((long)(length) >= (long)IN_EVENT_DATA_LEN &&	    \
   (long)(event)->len >= (long)IN_EVENT_DATA_LEN && \
   (long)(event)->len <= (long)(length))

/* Setup inotify notifications (IN) mask. All these defined in inotify.h. */
int event_mask =
  (
   //IN_ACCESS |        /* File accessed */
   //IN_ATTRIB |        /* File attributes changed */
   //IN_OPEN   |        /* File was opened */
   IN_CLOSE_WRITE |   /* Writtable File closed */
   //IN_CLOSE_NOWRITE | /* Unwrittable File closed */
   //IN_CREATE |        /* File created in directory */
   IN_DELETE |        /* File deleted in directory */
   //IN_DELETE_SELF |   /* Directory deleted */
   IN_MODIFY |        /* File modified */
   //IN_MOVE_SELF |     /* Directory moved */
   //IN_MOVED_FROM |    /* File moved away from the directory */
   IN_MOVED_TO        /* File moved into the directory */
   );      

int event_mask2 =
  (
   //IN_ACCESS |        /* File accessed */
   //IN_ATTRIB |        /* File attributes changed */
   //IN_OPEN   |        /* File was opened */
   IN_CLOSE_WRITE |   /* Writtable File closed */
   //IN_CLOSE_NOWRITE | /* Unwrittable File closed */
   //IN_CREATE |        /* File created in directory */
   IN_DELETE |        /* File deleted in directory */
   //IN_DELETE_SELF |   /* Directory deleted */
   //IN_MODIFY |        /* File modified */
   //IN_MOVE_SELF |     /* Directory moved */
   //IN_MOVED_FROM |    /* File moved away from the directory */
   IN_MOVED_TO        /* File moved into the directory */
   );      

/* Array of directories being monitored */
monitored_t *monitors;
int n_monitors;

int MINIMAL_UPDATE_DISTANCE_S = 10;
time_t last_refresh_time = 0;
char *target_file = NULL;
char *terser_input_file = NULL;
const char *terser_args = NULL;
char *tmp_path = NULL;
const size_t tmp_path_len = 1024;

struct hashmap *wd_to_mon_map;
struct hashmap *file_to_mon_map;
struct hashmap *files_refreshed;


char *path_join(const char *folder, const char *basename){
    int ps = snprintf(tmp_path, tmp_path_len, "%s/%s", folder, basename);
    if( ps < 0 || ps >= tmp_path_len ){
        perror("snprintf failed!");
        exit(EXIT_FAILURE);
    }
    return tmp_path;
}


const char *sprint_time(time_t t){
    const char *format = "%a, %d %b %y %T %z";
    static char outstr[200];
    struct tm *tmp;

    outstr[sizeof(outstr)-1] = '\0';
    tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }

    if (strftime(outstr, sizeof(outstr), format, tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }

    //printf("%s", outstr);
    return outstr;
}

/* Check if path was already processed and wrote otherwise */
void bundle_write(const char *input_file, FILE *outfile){

    char *_input_file = strdup(input_file);
    const monitored_t *f = hashmap_get(files_refreshed,
            &(monitored_t){ .path=_input_file, .hit=1 });
    int hit;
    if (f){
        // Reuse existing entry (Attention, hit==0 is possible)
        hit = f->hit + 1;
        hashmap_set(files_refreshed,
                &(monitored_t){ .path=f->path, .hit=hit});

        free(_input_file);
    }else{
        // Create new entry
        hit = 1;
        hashmap_set(files_refreshed,
                &(monitored_t){.path=_input_file, .hit=hit});
    }

    if(hit > 1){
        printf("Skip '%s'\n", input_file);
        return;
    }

    printf("Cat  '%s'\n", input_file);
    FILE *infile = fopen(input_file, "r");
    if (infile == NULL) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFSIZ];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), infile)) > 0) {
        fwrite(buffer, 1, n, outfile);
    }

    fclose(infile);
}

void refresh(const char *path, FILE *outfile){


    DIR *dir = opendir(path);
    if (!dir){
        // Its a file
        bundle_write(path, outfile);
        return;
    }

    // its a dir, loop over *.js-files
    struct dirent *de;
    while ((de = readdir(dir))) {     // Fill one files.[filename] per file.
        // Skip ".", ".." and hidden files in listing
        if ( de->d_name[0] == '.' ) continue;

        size_t l = strlen(de->d_name);
        if (l < 3 || 0 != strcmp(".js", &de->d_name[l-3])){
            continue;
        }

        path_join(path, de->d_name);
        bundle_write(tmp_path, outfile);
    }
    closedir(dir);
}

void refresh_webui_js_files(){
    time_t current_time = time(NULL);
    double tdiff = difftime(current_time, last_refresh_time);
    if (tdiff < 0.0) { // (*)
        // Multiple changes occoured in a short period. Skip all
        // changes after the first one which was handled after the sleep, below.
        return;
    }
    if (tdiff < (double)MINIMAL_UPDATE_DISTANCE_S){
        // Wait on more changes of other files
        int wait_s = MINIMAL_UPDATE_DISTANCE_S - (int)tdiff;
        printf("Sleep…\n");
        sleep(wait_s);
    }

    current_time = time(NULL);
    // Update timestamp and set it into the future for (*)
    last_refresh_time = current_time + 2;

    // Merge all js files into one big.
    printf("Update at '%s'\n", sprint_time(current_time));

    // Reset hit counter
    size_t iter = 0;
    void *item;
    while (hashmap_iter(files_refreshed, &iter, &item)) {
        struct monitored_t *m = item;
        m->hit = 0;
    }

    FILE *outfile = fopen(terser_input_file?terser_input_file:target_file, "w");
    if (outfile == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    iter = 0;
    while (hashmap_iter(file_to_mon_map, &iter, &item)) {
        const path_t *p = item;
        //const monitored_t *m = p->m;
        refresh(p->path, outfile);
    }

    fclose(outfile);

    if (terser_input_file){ // Compress step to get target_file
        char *command = NULL;
        if ( -1 == asprintf(&command,
                    "../minimizer/terser.sh \"%s\" "
                    "--output \"%s\" " 
                    "%s", terser_input_file, target_file, terser_args)){
            perror("asprintf failed!");
            exit(EXIT_FAILURE);
        }
        printf("Call '%s'\n", command);
        system(command);
    }

}

/* Returns: 
 *  1 is file
 *  0 is dir
 * errno ( <i 0 ) failed access
 */
int isdir(const char *path){
  //int fd = open(path, O_PATH|O_NOATIME);
  //if (fd < 0) return errno;
  //DIR *dir = fopendir(fd);
  DIR *dir = opendir(path);
  if (!dir){
      if (errno == ENOTDIR) return 1;
      return errno;
  }
  closedir(dir);
  //close(fd);
  return 0;
  
}


void
__event_process (struct inotify_event *event)
{
    int i;

    const monitored_t *m = hashmap_get(wd_to_mon_map,
            &(monitored_t){ .wd=event->wd });

    if (!m){
        fprintf(stderr, "Hey, hashmap contains no element for this event->wd.");
        return;
    }

    char *fname = NULL;
    int event_can_be_skiped = 1;

    if (event->len==0) {
        fname = m->path;
    }else{
        fname = path_join(m->path, event->name);
    }

    // Check if it's one of the explicit named files;
    const path_t *f = hashmap_get(file_to_mon_map,
            &(path_t){.path=fname});
    if (f){
        event_can_be_skiped = 0;
    }

    // Just lock for *.js files and abort otherwise
    size_t l = strlen(fname);
    if (l >= 3 && 0 == strcmp(".js", &fname[l-3])){
        event_can_be_skiped = 0;
    }

    if (event_can_be_skiped) return;

    printf ("Received event in '%s': ", fname);

    if (event->mask & IN_ACCESS)
        printf ("\tIN_ACCESS\n");
    if (event->mask & IN_ATTRIB)
        printf ("\tIN_ATTRIB\n");
    if (event->mask & IN_OPEN)
        printf ("\tIN_OPEN\n");
    if (event->mask & IN_CLOSE_WRITE)
        printf ("\tIN_CLOSE_WRITE\n");
    if (event->mask & IN_CLOSE_NOWRITE)
        printf ("\tIN_CLOSE_NOWRITE\n");
    if (event->mask & IN_CREATE)
        printf ("\tIN_CREATE\n");
    if (event->mask & IN_DELETE)
        printf ("\tIN_DELETE\n");
    if (event->mask & IN_DELETE_SELF)
        printf ("\tIN_DELETE_SELF\n");
    if (event->mask & IN_MODIFY)
        printf ("\tIN_MODIFY\n");
    if (event->mask & IN_MOVE_SELF)
        printf ("\tIN_MOVE_SELF\n");
    if (event->mask & IN_MOVED_FROM)
        printf ("\tIN_MOVED_FROM (cookie: %d)\n",
                event->cookie);
    if (event->mask & IN_MOVED_TO)
        printf ("\tIN_MOVED_TO (cookie: %d)\n",
                event->cookie);

    refresh_webui_js_files();
    fflush (stdout);

    return;
}


int
__initialize_inotify (int          argc,
                      const char **argv)
{
  int i;
  int wd;
  int inotify_fd;

  /* Create new inotify device */
  if ((inotify_fd = inotify_init ()) < 0)
    {
      fprintf (stderr,
               "Couldn't setup new inotify device: '%s'\n",
               strerror (errno));
      return -1;
    }

  /* Allocate array of monitor setups */
  n_monitors = argc;
  //observed_paths = calloc (n_monitors, sizeof (char *));

  /* Loop all input directories, setting up watches */
  for (i = 0; i < n_monitors; ++i)
  {
      // Check if file or dir
      int idir = isdir(argv[i]);
      if (idir < 0){
          fprintf(stderr,
                  "Couldn't monitoring '%s': '%s'\n",
                  argv[i],
                  strerror (errno));
          exit (EXIT_FAILURE);
      }

      char *path;
      char *file;
      if (idir == 0) { // it's a dir
          path = strdup(argv[i]);
          file = NULL;
      }
      if (idir == 1) { // it's a file
          char *_tmp_path = strdup(argv[i]);
          path = strdup(dirname(_tmp_path));
          file = strdup(argv[i]);
          free(_tmp_path);
      }

      // Check if path is already monitored
      // Looping here, because we're not using the key of the hash map.
      monitored_t *m_found = NULL;
      size_t iter = 0;
      void *item;
      while (hashmap_iter(wd_to_mon_map, &iter, &item)) {
          const monitored_t *m = item;
          if (0 == strcmp(m->path, path)){
              m_found = (monitored_t *)m;
              break;
          }
      }
      if (m_found) {
          printf("This path is already monitored.\n");
          if (file){
              printf("Adding file to file_to_mon-map.\n");
              hashmap_set(file_to_mon_map,
                      &(path_t){.path=strdup(file), .m=m_found});
          }

          free(path);
          free(file);
          continue;
      }

      if ((wd = inotify_add_watch (inotify_fd,
                      path,
                      event_mask)) < 0)
      {
          fprintf (stderr,
                  "Couldn't add monitor in directory '%s': '%s'\n",
                  path,
                  strerror (errno));
          exit (EXIT_FAILURE);
      }

      hashmap_set(wd_to_mon_map,
              &(monitored_t){.path=strdup(path), .wd=wd});
      m_found = (monitored_t *)hashmap_get(wd_to_mon_map,
              &(monitored_t){.path=path}); // not returned by _set call
      hashmap_set(file_to_mon_map,
              &(path_t){.path=strdup(file?file:path), .m=m_found});
      printf ("Started monitoring: '%s'...\n",
              file?file:path);

      free(path);
      free(file);
  }

  return inotify_fd;
}

void
__shutdown_signals (int signal_fd)
{
  close (signal_fd);
}

int
__initialize_signals (void)
{
  int signal_fd;
  sigset_t sigmask;

  /* We want to handle SIGINT and SIGTERM in the signal_fd, so we block them. */
  sigemptyset (&sigmask);
  sigaddset (&sigmask, SIGINT);
  sigaddset (&sigmask, SIGTERM);

  if (sigprocmask (SIG_BLOCK, &sigmask, NULL) < 0)
    {
      fprintf (stderr,
               "Couldn't block signals: '%s'\n",
               strerror (errno));
      return -1;
    }

  /* Get new FD to read signals from it */
  if ((signal_fd = signalfd (-1, &sigmask, 0)) < 0)
    {
      fprintf (stderr,
               "Couldn't setup signal FD: '%s'\n",
               strerror (errno));
      return -1;
    }

  return signal_fd;
}

void clear_hashmaps(int inotify_fd){

    size_t iter = 0;
    void *item;
    while (hashmap_iter(files_refreshed, &iter, &item)) {
        const path_t *f = item;
        free(f->path);
    }

    item = 0;
    while (hashmap_iter(file_to_mon_map, &iter, &item)) {
        const path_t *p = item;
        free(p->path);
    }

    item = 0;
    while (hashmap_iter(wd_to_mon_map, &iter, &item)) {
        const monitored_t *m = item;
        inotify_rm_watch (inotify_fd, m->wd);
        free(m->path);
    }
}

int main (int argc, const char **argv)
{
    int signal_fd;
    int inotify_fd;
    struct pollfd fds[FD_POLL_MAX];

    // for some {path}/{filename} merges
    tmp_path = malloc(tmp_path_len * sizeof (char));

    // Strutures to organize input as file->monitored_path->wd maps.
    // wd is inotify watch descriptor.
    wd_to_mon_map = hashmap_new(
            sizeof(monitored_t), 0, 0, 0, 
            monitor_hash_by_wd, monitor_compare_by_wd, NULL, NULL);
    file_to_mon_map = hashmap_new(
            sizeof(path_t), 0, 0, 0, 
            path_hash, path_compare, NULL, NULL);
    files_refreshed = hashmap_new(
            sizeof(monitored_t), 0, 0, 0, 
            monitor_hash_by_path, monitor_compare_by_path, NULL, NULL);

    // Argparse
    int oneshot = 0;
    //int terser = 0; // call minimizer TODO
    const char *default_target_file = "/run/shm/webui.bundle.js";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Basic options"),
        OPT_BOOLEAN('1', "oneshot", &oneshot, "Generate output and exit", NULL, 0, 0),
        //OPT_BOOLEAN('c', "terser", &terser, "Compress output with terser", NULL, 0, 0),
        OPT_STRING('c', "terser", &terser_args, "Compress output with terser", NULL, 0, 0),
        OPT_STRING('o', "output", &target_file, "Bundle output file", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nBundles JavaScript files into one big file", "\nThis tool watches for changes in the input files by Linux's inotify and updates the output file on changes.\n\nIn input directories it observes all *.js files.");
    argc = argparse_parse(&argparse, argc, argv);

    // Copy input string or default value.
    target_file = strdup(target_file?target_file:default_target_file);

    if (terser_args) {
        // Interleave temp file
        if (-1 == asprintf(&terser_input_file, "%s.tmp", target_file)){
            perror("asprintf failed!");
            exit(EXIT_FAILURE);
        }
    }

    if (argc != 0) {
        printf("argc: %d\n", argc);
        int i;
        for (i = 0; i < argc; i++) {
            printf("argv[%d]: %s\n", i, *(argv + i));
        }
    }
    if (argc == 0) {
        perror("No input files/directories defined!");
        exit(EXIT_FAILURE);
    }

    /* Initialize signals FD */
    if ((signal_fd = __initialize_signals ()) < 0)
    {
        fprintf (stderr, "Couldn't initialize signals\n");
        exit (EXIT_FAILURE);
    }

    /* Initialize inotify FD and the watch descriptors */
    if ((inotify_fd = __initialize_inotify (argc, argv)) < 0)
    {
        fprintf (stderr, "Couldn't initialize inotify\n");
        exit (EXIT_FAILURE);
    }

    printf ("Target bundle file: '%s'\n", target_file);

    // First run at startup
    refresh_webui_js_files();
    if (oneshot) goto main_clean;

    /* Setup polling */
    fds[FD_POLL_SIGNAL].fd = signal_fd;
    fds[FD_POLL_SIGNAL].events = POLLIN;
    fds[FD_POLL_INOTIFY].fd = inotify_fd;
    fds[FD_POLL_INOTIFY].events = POLLIN;

    /* Now loop */
    for (;;)
    {
        /* Block until there is something to be read */
        if (poll (fds, FD_POLL_MAX, -1) < 0)
        {
            fprintf (stderr,
                    "Couldn't poll(): '%s'\n",
                    strerror (errno));
            exit (EXIT_FAILURE);
        }

        /* Signal received? */
        if (fds[FD_POLL_SIGNAL].revents & POLLIN)
        {
            struct signalfd_siginfo fdsi;

            if (read (fds[FD_POLL_SIGNAL].fd,
                        &fdsi,
                        sizeof (fdsi)) != sizeof (fdsi))
            {
                fprintf (stderr,
                        "Couldn't read signal, wrong size read\n");
                exit (EXIT_FAILURE);
            }

            /* Break loop if we got the expected signal */
            if (fdsi.ssi_signo == SIGINT ||
                    fdsi.ssi_signo == SIGTERM)
            {
                break;
            }

            fprintf (stderr,
                    "Received unexpected signal\n");
        }

        /* Inotify event received? */
        if (fds[FD_POLL_INOTIFY].revents & POLLIN)
        {
            char buffer[INOTIFY_BUFFER_SIZE];
            size_t length;

            /* Read from the FD. It will read all events available up to
             * the given buffer size. */
            if ((length = read (fds[FD_POLL_INOTIFY].fd,
                            buffer,
                            INOTIFY_BUFFER_SIZE)) > 0)
            {
                struct inotify_event *event;

                event = (struct inotify_event *)buffer;
                while (IN_EVENT_OK (event, length))
                {
                    __event_process (event);
                    event = IN_EVENT_NEXT (event, length);
                }
            }
        }
    }

main_clean:
    /* Clean exit */
    __shutdown_signals (signal_fd);
    free (target_file);
    free (terser_input_file);
    free (tmp_path);

    clear_hashmaps(inotify_fd);
    hashmap_free(files_refreshed);
    hashmap_free(file_to_mon_map);
    hashmap_free(wd_to_mon_map);

    printf ("Exiting inotify example...\n");

    return EXIT_SUCCESS;
}


