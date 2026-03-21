#pragma once

#include "bluray.h"
#include "compat.h"

#include <stddef.h>


/* Subcommand: extract = extract file tree, patch = decrypted ISO, check = verify. */
typedef enum { CMD_EXTRACT = 0, CMD_PATCH = 1, CMD_CHECK = 2 } subcommand_t;

/* Argument parsing & program initialisation.
 * *cmd       – the subcommand to execute.
 * *output    – the positional output path (NULL for CMD_CHECK).
 * *buf_size  – I/O read buffer size. */
void init(int argc, char **argv, subcommand_t *cmd, BLURAY **bluray,
          char **output, size_t *buf_size, char **source_path);

/* Size string parser: "64k", "1m", "2g" or plain bytes */
size_t parse_size(const char *s);

/* Keyfile discovery */
int   regular_file_exists(const char *path);
char *search_keyfile(void);
