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
        #define S_ISBLK(m) ((void)(m), 0)
    #endif

    #ifndef S_ISCHR
        #define S_ISCHR(m) ((void)(m), 0)
    #endif

    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif

    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif

    #define bd_stat_s struct __stat64

/* Internal helper: convert s from one specific codepage to a new wchar_t
 * buffer. Returns a malloc'd pointer on success, NULL on failure. */
static inline wchar_t *try_cp_to_wide(UINT cp, DWORD flags, const char *s) {
    int n = MultiByteToWideChar(cp, flags, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(cp, flags, s, -1, w, n) > 0) return w;
    free(w);
    return NULL;
}

/* Convert a path string (tries UTF-8 first, falls back to the system ANSI
 * code page) to a newly-allocated wide-char string.
 * libbluray/libudfread may return UTF-8 or the system ACP depending on the
 * platform build; this helper handles both transparently.
 * Caller must free() the result. Returns NULL on failure. */
static inline wchar_t *path_to_wide(const char *s) {
    wchar_t *w = try_cp_to_wide(CP_UTF8, MB_ERR_INVALID_CHARS, s);
    return w ? w : try_cp_to_wide(CP_ACP, 0, s);
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
    wchar_t wmode[8];
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wmode, 8);
    FILE *f = _wfopen(w, wmode);
    free(w);
    return f;
}

    #define bd_mkdir(p)    _w_mkdir(p)
    #define chdir(d)       _w_chdir(d)
    #define bd_stat(p, s)  _w_stat(p, s)
    #define bd_fopen(p, m) _w_fopen(p, m)

    /* MSVC does not provide fseeko/ftello; map them to the 64-bit variants. */
    #define fseeko(f, o, w) _fseeki64((f), (o), (w))
    #define ftello(f)       _ftelli64(f)

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
