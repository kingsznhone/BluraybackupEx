#include "progress.h"

#include "compat.h"


uint64_t get_time_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
#endif
}

void format_size(char *buf, size_t bufsz, int64_t bytes) {
    if (bytes < 0)
        snprintf(buf, bufsz, "???");
    else if (bytes >= (int64_t)1 << 30)
        snprintf(buf, bufsz, "%.2f GiB", (double)bytes / (double)((int64_t)1 << 30));
    else if (bytes >= (int64_t)1 << 20)
        snprintf(buf, bufsz, "%.2f MiB", (double)bytes / (double)(1 << 20));
    else if (bytes >= (int64_t)1 << 10)
        snprintf(buf, bufsz, "%.1f KiB", (double)bytes / (double)(1 << 10));
    else
        snprintf(buf, bufsz, "%lld B", (long long)bytes);
}

void format_duration(char *buf, size_t bufsz, int64_t secs) {
    int h, m, s;
    if (secs < 0) secs = 0;
    h = (int)(secs / 3600);
    m = (int)((secs % 3600) / 60);
    s = (int)(secs % 60);
    if (h > 0)
        snprintf(buf, bufsz, "%d:%02d:%02d", h, m, s);
    else
        snprintf(buf, bufsz, "%d:%02d", m, s);
}

/* Print a one-line progress report to stderr.
 * label    : short description / filename (display truncated to 40 chars)
 * written  : bytes written so far
 * total    : total bytes, or -1 if unknown
 * start_ms : timestamp when the copy started (from get_time_ms())
 * done     : 1 to emit a final newline, 0 for an overwritable \r line */
void print_progress(const char *label, int64_t written, int64_t total, uint64_t start_ms, int done) {
    uint64_t now_ms   = get_time_ms();
    double   elapsed  = (double)(now_ms - start_ms) / 1000.0;
    double   speed    = elapsed > 0.01 ? (double)written / elapsed : 0.0;
    double   speed_mb = speed / (1024.0 * 1024.0);
    char     sz_w[16], sz_t[16], dur[12];

    format_size(sz_w, sizeof(sz_w), written);
    if (total > 0) {
        double  pct = (double)written / (double)total * 100.0;
        int64_t eta = speed > 0.0 ? (int64_t)((double)(total - written) / speed) : -1;
        format_size(sz_t, sizeof(sz_t), total);
        if (eta >= 0) {
            format_duration(dur, sizeof(dur), eta);
            fprintf(stderr, "\r  %-40.40s  %9s / %-9s  %5.1f%%  %7.2f MB/s  ETA %-8s", label, sz_w, sz_t, pct, speed_mb, dur);
        } else {
            fprintf(stderr, "\r  %-40.40s  %9s / %-9s  %5.1f%%  %7.2f MB/s          ", label, sz_w, sz_t, pct, speed_mb);
        }
    } else {
        format_duration(dur, sizeof(dur), (int64_t)elapsed);
        fprintf(stderr, "\r  %-40.40s  %9s           %7.2f MB/s  Elapsed %-8s", label, sz_w, speed_mb, dur);
    }
    if (done) fputc('\n', stderr);
    fflush(stderr);
}
