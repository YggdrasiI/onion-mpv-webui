#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>

#include <onion/log.h>
#include "onion_ws_status.h"

#ifdef CLOCK_MONOTONIC_COARSE
#define CLOCK_TYPE CLOCK_MONOTONIC_COARSE
#else
// Non-Linux fallback 
#define CLOCK_TYPE CLOCK_MONOTONIC
#endif

/* Not used */
int diff_ms(
        const struct timespec * spec1,
        const struct timespec * spec2)
{
  int ds = (spec2->tv_sec - spec1->tv_sec) * 1000;
  long dns = (spec2->tv_nsec - spec1->tv_nsec) / 1E6;
  return ds + dns;
}

/* Return 1 if  prop->next_update_time <= time */
int time_diff_reached(
        __property *prop,
        const struct timespec * time)
{
    // Compare fixed point numbers (A,a) with (B,b).
    
    // A < B
    if (prop->next_update_time.tv_sec < time->tv_sec) return 1;

    // A > B
    if (prop->next_update_time.tv_sec > time->tv_sec) return 0;

    // A == B && a > b
    if (prop->next_update_time.tv_nsec > time->tv_nsec) return 0;

    // A == B && a <= b
    return 1;
}

void eval_next_update_time(
        __property *prop)
{
    prop->next_update_time.tv_sec = prop->last_update_time.tv_sec + prop->minimal_update_diff.tv_sec;
    prop->next_update_time.tv_nsec = prop->last_update_time.tv_nsec + prop->minimal_update_diff.tv_nsec;
    if (prop->next_update_time.tv_nsec > 1E9) {
        prop->next_update_time.tv_sec  += 1;
        prop->next_update_time.tv_nsec -= 1E9;
    }
}

void update_timestamp(
        __property *prop,
        const struct timespec * time){
    prop->last_update_time = *time;
    eval_next_update_time(prop);
}

void fetch_timestamp(struct timespec * pspec){
    clock_gettime(CLOCK_TYPE, pspec);
}


