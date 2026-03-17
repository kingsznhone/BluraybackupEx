#include "async_writer.h"

static
#ifdef _WIN32
    unsigned __stdcall
#else
    void *
#endif
    aw_thread_func(void *arg) {
    async_writer_t *w = (async_writer_t *)arg;
#ifdef _WIN32
    EnterCriticalSection(&w->cs);
    for (;;) {
        while (!w->pending && !w->quit)
            SleepConditionVariableCS(&w->cvJob, &w->cs, INFINITE);
        if (w->quit) break;
        LeaveCriticalSection(&w->cs);
        if (fwrite(w->buf, 1, w->len, w->fp) != w->len) w->error = 1;
        EnterCriticalSection(&w->cs);
        w->pending = 0;
        WakeConditionVariable(&w->cvDone);
    }
    LeaveCriticalSection(&w->cs);
    return 0;
#else
    pthread_mutex_lock(&w->mtx);
    for (;;) {
        while (!w->pending && !w->quit) pthread_cond_wait(&w->cvJob, &w->mtx);
        if (w->quit) break;
        pthread_mutex_unlock(&w->mtx);
        if (fwrite(w->buf, 1, w->len, w->fp) != w->len) w->error = 1;
        pthread_mutex_lock(&w->mtx);
        w->pending = 0;
        pthread_cond_signal(&w->cvDone);
    }
    pthread_mutex_unlock(&w->mtx);
    return NULL;
#endif
}

int aw_init(async_writer_t *w, FILE *fp) {
    memset(w, 0, sizeof(*w));
    w->fp = fp;
#ifdef _WIN32
    InitializeCriticalSection(&w->cs);
    InitializeConditionVariable(&w->cvJob);
    InitializeConditionVariable(&w->cvDone);
    w->hThread = (HANDLE)_beginthreadex(NULL, 0, aw_thread_func, w, 0, NULL);
    return w->hThread ? 0 : -1;
#else
    pthread_mutex_init(&w->mtx, NULL);
    pthread_cond_init(&w->cvJob, NULL);
    pthread_cond_init(&w->cvDone, NULL);
    return pthread_create(&w->tid, NULL, aw_thread_func, w);
#endif
}

/* Hand a filled buffer to the writer thread.  Blocks until the previous
 * write has completed so the caller may safely reuse the *other* buffer. */
int aw_submit(async_writer_t *w, uint8_t *buf, size_t len) {
#ifdef _WIN32
    EnterCriticalSection(&w->cs);
    while (w->pending) SleepConditionVariableCS(&w->cvDone, &w->cs, INFINITE);
    if (w->error) {
        LeaveCriticalSection(&w->cs);
        return -1;
    }
    w->buf     = buf;
    w->len     = len;
    w->pending = 1;
    WakeConditionVariable(&w->cvJob);
    LeaveCriticalSection(&w->cs);
#else
    pthread_mutex_lock(&w->mtx);
    while (w->pending) pthread_cond_wait(&w->cvDone, &w->mtx);
    if (w->error) {
        pthread_mutex_unlock(&w->mtx);
        return -1;
    }
    w->buf     = buf;
    w->len     = len;
    w->pending = 1;
    pthread_cond_signal(&w->cvJob);
    pthread_mutex_unlock(&w->mtx);
#endif
    return 0;
}

/* Drain the last write and tear down the writer thread. */
int aw_finish(async_writer_t *w) {
#ifdef _WIN32
    EnterCriticalSection(&w->cs);
    while (w->pending) SleepConditionVariableCS(&w->cvDone, &w->cs, INFINITE);
    w->quit = 1;
    WakeConditionVariable(&w->cvJob);
    LeaveCriticalSection(&w->cs);
    WaitForSingleObject(w->hThread, INFINITE);
    CloseHandle(w->hThread);
    DeleteCriticalSection(&w->cs);
#else
    pthread_mutex_lock(&w->mtx);
    while (w->pending) pthread_cond_wait(&w->cvDone, &w->mtx);
    w->quit = 1;
    pthread_cond_signal(&w->cvJob);
    pthread_mutex_unlock(&w->mtx);
    pthread_join(w->tid, NULL);
    pthread_mutex_destroy(&w->mtx);
    pthread_cond_destroy(&w->cvJob);
    pthread_cond_destroy(&w->cvDone);
#endif
    return w->error ? -1 : 0;
}
