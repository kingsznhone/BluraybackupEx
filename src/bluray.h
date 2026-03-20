#pragma once

#include "compat.h"

#include <libbluray/bluray.h>
#include <stddef.h>


void    log_disc_info(BLURAY *bluray, const BLURAY_DISC_INFO *info);
BLURAY *open_bluray(const char *device, const char *keyfile);

/* Returns a newly-allocated, filesystem-safe folder name for the disc.
 * Priority: UDF Volume ID > disc_name > meta di_name > disc_id hex.
 * Invalid path characters are replaced with '_'.  Caller must free(). */
char *get_disc_label(BLURAY *bluray);

int file_exists(const char *path, bd_stat_s *info);
int bd_source_exists(const char *path);
int copy_dir(BLURAY *bluray, const char *path, size_t buf_size);
int copy_file(BLURAY *bluray, const char *src, const char *dst, size_t buf_size);

/* Verify all files on the disc by reading every AACS unit and checking the
 * MAC.  Returns the total number of unreadable blocks across all files. */
int verify_dir(BLURAY *bluray, const char *path, size_t buf_size);

/* Create a single decrypted ISO image from a BD source (ISO file or device).
 * Phase 1 – raw stream copy of the source to output_path.
 * Phase 2 – re-read every .m2ts stream through libbluray's AACS-decrypting
 *            layer and write the decrypted sectors back to the exact sector
 *            positions in output_path using libudfread for LBA lookup.
 * Returns 1 on success, 0 on error. */
int dump_iso(BLURAY *bluray, const char *iso_path, const char *output_path, size_t buf_size);
