#pragma once

#include <stddef.h>
#include <stdint.h>

uint64_t get_time_ms(void);
void     format_size(char *buf, size_t bufsz, int64_t bytes);
void     format_duration(char *buf, size_t bufsz, int64_t secs);
void     print_progress(const char *label, int64_t written, int64_t total,
                        uint64_t start_ms, int done);
