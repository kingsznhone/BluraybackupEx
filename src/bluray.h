#pragma once

#include "compat.h"

#include <libbluray/bluray.h>
#include <stddef.h>


BLURAY *open_bluray(const char *device, const char *keyfile, int copy_titles);

int file_exists(const char *path, bd_stat_s *info);
int bd_source_exists(const char *path);
int copy_dir(BLURAY *bluray, const char *path, size_t buf_size);
int copy_file(BLURAY *bluray, const char *src, const char *dst,
              size_t buf_size);
int copy_main_title(BLURAY *bluray, const char *dst, size_t buf_size);
