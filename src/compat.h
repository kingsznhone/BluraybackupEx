#pragma once

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <direct.h>
    #include <io.h>
    #include <process.h>
    #include <windows.h>

    #ifndef S_ISBLK
        #define S_ISBLK(m) 0
    #endif

    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif

    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif

    #define bd_stat_s struct __stat64

/* Convert a path string (tries UTF-8 first, falls back to the system ANSI
 * code page) to a newly-allocated wide-char string.
 * libbluray/libudfread may return UTF-8 or the system ACP depending on the
 * platform build; this helper handles both transparently.
 * Caller must free() the result. Returns NULL on failure. */
static inline wchar_t *path_to_wide(const char *s) {
    /* Try strict UTF-8 first. */
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    if (n > 0) {
        wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
        if (w &&
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n) > 0)
            return w;
        free(w);
    }
    /* Fall back to the system ANSI code page (e.g. CP936 on Chinese Windows).
     */
    n = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_ACP, 0, s, -1, w, n) == 0) {
        free(w);
        return NULL;
    }
    return w;
}

static inline int _w_mkdir(const char *p) {
    wchar_t *w = path_to_wide(p);
    if (!w) return -1;
    int r = _wmkdir(w);
    free(w);
    return r;
}

static inline int _w_chdir(const char *p) {
    wchar_t *w = path_to_wide(p);
    if (!w) return -1;
    int r = _wchdir(w);
    free(w);
    return r;
}

static inline int _w_stat(const char *p, struct __stat64 *s) {
    wchar_t *w = path_to_wide(p);
    if (!w) return -1;
    int r = _wstat64(w, s);
    free(w);
    return r;
}

static inline FILE *_w_fopen(const char *path, const char *mode) {
    wchar_t *w = path_to_wide(path);
    if (!w) return NULL;
    wchar_t wmode[8] = {0};
    for (int i = 0; i < 7 && mode[i]; i++)
        wmode[i] = (wchar_t)(unsigned char)mode[i];
    FILE *f = _wfopen(w, wmode);
    free(w);
    return f;
}

    #define bd_mkdir(p)    _w_mkdir(p)
    #define chdir(d)       _w_chdir(d)
    #define bd_stat(p, s)  _w_stat(p, s)
    #define bd_fopen(p, m) _w_fopen(p, m)

#else
    #include <pthread.h>
    #include <unistd.h>

    #define bd_mkdir(p)    mkdir((p), 0755)
    #define bd_stat(p, s)  stat(p, s)
    #define bd_stat_s      struct stat
    #define bd_fopen(p, m) fopen((p), (m))
#endif

#define ENCRYPTED_BYTES_TO_READ 6144

extern volatile sig_atomic_t running;
