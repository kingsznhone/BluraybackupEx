#pragma once

#include "compat.h"

#include <stddef.h>
#include <stdint.h>


/* Asynchronous double-buffer writer.
 * A dedicated thread writes one buffer to disk while the main thread fills
 * the other, overlapping reading with I/O for higher throughput. */
typedef struct {
    uint8_t *buf;
    size_t   len;
    FILE    *fp;
    int      error;
    int      pending;
    int      quit;
#ifdef _WIN32
    CRITICAL_SECTION   cs;
    CONDITION_VARIABLE cvJob;
    CONDITION_VARIABLE cvDone;
    HANDLE             hThread;
#else
    pthread_mutex_t mtx;
    pthread_cond_t  cvJob;
    pthread_cond_t  cvDone;
    pthread_t       tid;
#endif
} async_writer_t;

int aw_init(async_writer_t *w, FILE *fp);
int aw_submit(async_writer_t *w, uint8_t *buf, size_t len);
int aw_finish(async_writer_t *w);
