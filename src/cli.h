#pragma once

#include "bluray.h"
#include "compat.h"

#include <stddef.h>


/* Argument parsing & program initialisation */
void init(int argc, char **argv, BLURAY **bluray,
          char **output_dir, size_t *buf_size, int *check);

/* Size string parser: "64k", "1m", "2g" or plain bytes */
size_t parse_size(const char *s);

/* Keyfile discovery */
int   regular_file_exists(const char *path);
char *search_keyfile(void);

/* Usage / help / version output */
void print_default_usage(void);
void print_help(void);
void print_version(void);
