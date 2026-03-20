#pragma once

#include "bluray.h"
#include "compat.h"

#include <stddef.h>


/* Output mode: dir = extract file tree, iso = decrypted ISO image. */
typedef enum { MODE_DIR = 0, MODE_ISO = 1 } output_mode_t;

/* Argument parsing & program initialisation.
 * *output  – the positional output path (NULL if not supplied).
 * *mode    – output_mode_t: MODE_DIR or MODE_ISO (default: MODE_DIR). */
void init(int argc, char **argv, BLURAY **bluray,
          char **output, output_mode_t *mode, size_t *buf_size, int *check,
          char **source_path);

/* Size string parser: "64k", "1m", "2g" or plain bytes */
size_t parse_size(const char *s);

/* Keyfile discovery */
int   regular_file_exists(const char *path);
char *search_keyfile(void);

/* Usage / help / version output */
void print_default_usage(void);
void print_help(void);
void print_version(void);
