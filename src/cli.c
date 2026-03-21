#include "cli.h"


size_t parse_size(const char *s) {
    char              *end;
    unsigned long long val;

    if (s == NULL || *s == '\0') return 0;

    errno = 0;
    val   = strtoull(s, &end, 10);
    if (errno != 0 || end == s || val == 0) return 0;

    switch (*end) {
    case 'k':
    case 'K':
        val *= 1024ULL;
        end++;
        break;
    case 'm':
    case 'M':
        val *= 1024ULL * 1024ULL;
        end++;
        break;
    case 'g':
    case 'G':
        val *= 1024ULL * 1024ULL * 1024ULL;
        end++;
        break;
    case '\0':
        break;
    default:
        return 0;
    }

    if (*end != '\0') return 0;

    return (size_t)val;
}

int regular_file_exists(const char *path) {
    int       exist;
    bd_stat_s info;

    exist = 0;

    if (file_exists(path, &info)) {
        if (S_ISREG(info.st_mode))
            exist = 1;
        else
            fprintf(stderr, BIN ": %s isn't a regular file.\n", path);
    }

    return exist;
}

char *search_keyfile(void) {
    char        *home, *name;
    size_t       i, len;
    static char *path;
    char         relpath[24];

    len = 0;
#ifdef _WIN32
    {
        /* Use wide API to correctly handle non-ASCII APPDATA paths. */
        static char home_acp[_MAX_PATH * 2];
        wchar_t    *whome = _wgetenv(L"APPDATA");
        if (whome != NULL) WideCharToMultiByte(CP_ACP, 0, whome, -1, home_acp, sizeof(home_acp), NULL, NULL);
        home = whome != NULL ? home_acp : NULL;
    }
    strcpy(relpath, "\\aacs\\KEYDB.cfg");
#else
    home = getenv("HOME");
    strcpy(relpath, "/.config/aacs/KEYDB.cfg");
#endif

    if (home == NULL)
        strcpy(relpath, "KEYDB.cfg");
    else
        len += strlen(home);
    len += strlen(relpath);
    path = malloc(len + 1);
    for (i = 0; i != len; i++) path[i] = '\0';
    if (home != NULL) strcpy(path, home);
    strcat(path, relpath);

    if (regular_file_exists(path)) return path;

#ifdef _WIN32
    name = strrchr(path, '\\');
    if (name == NULL) name = strrchr(path, '/');
#else
    name = strrchr(path, '/');
#endif
    if (name == NULL)
        name = path;
    else
        name++;

    strcpy(name, "keydb.cfg");
    if (regular_file_exists(path)) return path;

    strcpy(name, "KeyDB.cfg");
    if (regular_file_exists(path)) return path;

    strcpy(name, "KEYDB.CFG");
    if (regular_file_exists(path)) return path;

    free(path);
    path = NULL;
    return path;
}

/* ---------- help / version ---------- */

static void print_version(void) {
    fputs(BIN " " VERSION "\n"
              "Copyright (C) 2023 Matteo Bini\n"
              "Copyright (C) 2024-2026 kingsznhone\n"
              "License: GPLv3+ <http://www.gnu.org/licenses/gpl.html>.\n"
              "This is free software: you are free to change and redistribute it.\n"
              "There is NO WARRANTY, to the extent permitted by law.\n"
              "\n"
              "Written by Matteo Bini.\n"
              "\n"
              "Major improvements and contributions by kingsznhone "
              "<https://github.com/kingsznhone>\n",
          stdout);
}

static void print_extract_help(void) {
    fputs("Usage: " BIN " extract [-k keyfile] [-b size] <source> <output-dir>\n"
          "\n"
          "Extract the full disc file tree into <output-dir>/<disc-label>/.\n"
          "\n"
          "Arguments:\n"
          "  <source>            Disc source: device path (e.g. /dev/sr0, D:), ISO/BIN\n"
          "                      file, or BDMV directory.\n"
          "  <output-dir>        Base output directory. The disc label is appended\n"
          "                      automatically: <output-dir>/<disc-label>/.\n"
          "\n"
          "Options:\n"
          "  -k, --keydb=FILE    Path to the AACS keys database file.\n"
          "  -b, --buffer=SIZE   I/O read buffer size (e.g. 6144, 6k, 6m, 60m).\n"
          "                      Must be >= 6144. Default: 6144 (1 AACS block).\n"
          "  -h, --help          Print this help text.\n"
          "\n"
          "Program exits with 0 on success, 1 on error.\n",
          stdout);
}

static void print_patch_help(void) {
    fputs("Usage: " BIN " patch [-k keyfile] [-b size] <source.iso> <output.iso>\n"
          "\n"
          "Write a single decrypted ISO image from a source ISO file.\n"
          "Phase 1 copies every sector raw; Phase 2 overwrites encrypted\n"
          ".m2ts streams with decrypted content.\n"
          "\n"
          "Arguments:\n"
          "  <source.iso>        Input ISO file.\n"
          "  <output.iso>        Output decrypted ISO file path.\n"
          "\n"
          "Options:\n"
          "  -k, --keydb=FILE    Path to the AACS keys database file.\n"
          "  -b, --buffer=SIZE   I/O read buffer size (e.g. 6144, 6k, 6m, 60m).\n"
          "                      Must be >= 6144. Default: 6144 (1 AACS block).\n"
          "  -h, --help          Print this help text.\n"
          "\n"
          "Program exits with 0 on success, 1 on error.\n",
          stdout);
}

static void print_check_help(void) {
    fputs("Usage: " BIN " check [-k keyfile] [-b size] <source>\n"
          "\n"
          "Read every file on the disc to verify readability.\n"
          "On AACS-encrypted discs each block MAC is validated;\n"
          "errors are reported with file path and byte offset.\n"
          "\n"
          "Arguments:\n"
          "  <source>            Disc source: device path (e.g. /dev/sr0, D:), ISO/BIN\n"
          "                      file, or BDMV directory.\n"
          "\n"
          "Options:\n"
          "  -k, --keydb=FILE    Path to the AACS keys database file.\n"
          "  -b, --buffer=SIZE   I/O read buffer size (e.g. 6144, 6k, 6m, 60m).\n"
          "                      Must be >= 6144. Default: 6144 (1 AACS block).\n"
          "  -h, --help          Print this help text.\n"
          "\n"
          "Program exits with 0 if no read errors found, 1 on error.\n",
          stdout);
}

static void print_global_help(void) {
    fputs(BIN " " VERSION " - Blu-ray Disc backup tool\n"
          "\n"
          "Usage: " BIN " <subcommand> [options]\n"
          "\n"
          "Subcommands:\n"
          "  extract   Extract the full disc file tree  <source> <output-dir>\n"
          "  patch     Write a single decrypted ISO image  <source.iso> <output.iso>\n"
          "  check     Verify disc readability and AACS MAC integrity  <source>\n"
          "\n"
          "Run '" BIN " <subcommand> --help' for subcommand-specific options.\n"
          "\n"
          "Global options:\n"
          "  -h, --help      Print this help text.\n"
          "  -v, --version   Print version and license information.\n"
          "\n"
          "Program exits with 0 on success, 1 on error.\n",
          stdout);
}

static void print_sub_help(subcommand_t cmd) {
    if (cmd == CMD_EXTRACT)
        print_extract_help();
    else if (cmd == CMD_PATCH)
        print_patch_help();
    else
        print_check_help();
}

/* ---------- init ---------- */

void init(int argc, char **argv, subcommand_t *cmd, BLURAY **bluray,
          char **output, size_t *buf_size, char **source_path) {
#ifdef _WIN32
    char abs_source[_MAX_PATH * 4]; /* UTF-8 can be up to 4 bytes per code point */
#endif
    char *pos[2]       = {NULL, NULL};
    int   pos_count    = 0;
    int   free_keyfile = 0;
    char *keyfile      = NULL;
    *cmd               = CMD_EXTRACT;
    *output            = NULL;
    *buf_size          = ENCRYPTED_BYTES_TO_READ;
    if (source_path) *source_path = NULL;

    /* argc == 1 is handled in main() before calling init(). */
    if (argc < 2) goto exit;

    /* Global flags before the subcommand name */
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        print_global_help();
        exit(0);
    }
    if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
        print_version();
        exit(0);
    }

    /* Identify subcommand */
    if (!strcmp(argv[1], "extract")) {
        *cmd = CMD_EXTRACT;
    } else if (!strcmp(argv[1], "patch")) {
        *cmd = CMD_PATCH;
    } else if (!strcmp(argv[1], "check")) {
        *cmd = CMD_CHECK;
    } else {
        fprintf(stderr, BIN ": Unknown subcommand \"%s\".\n", argv[1]);
        fputs("Try '" BIN " --help' for available subcommands.\n", stderr);
        goto exit;
    }

    /* Parse subcommand options from argv[2] onwards */
    int i;
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;

        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_sub_help(*cmd);
            exit(0);

        /* -b / --buffer=SIZE */
        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--buffer")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": Option \"%s\" requires an argument.\n", argv[i]);
                goto exit;
            }
            *buf_size = parse_size(argv[++i]);
            if (*buf_size < ENCRYPTED_BYTES_TO_READ) {
                fprintf(stderr, BIN ": Invalid buffer size; must be >= %d.\n", ENCRYPTED_BYTES_TO_READ);
                goto exit;
            }

        } else if (!strncmp(argv[i], "--buffer=", 9)) {
            *buf_size = parse_size(argv[i] + 9);
            if (*buf_size < ENCRYPTED_BYTES_TO_READ) {
                fprintf(stderr, BIN ": Invalid buffer size; must be >= %d.\n", ENCRYPTED_BYTES_TO_READ);
                goto exit;
            }

        /* -k / --keydb=FILE */
        } else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--keydb")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": Option \"%s\" requires an argument.\n", argv[i]);
                goto exit;
            }
            keyfile = argv[++i];

        } else if (!strncmp(argv[i], "--keydb=", 8)) {
            keyfile = argv[i] + 8;

        /* Unknown option */
        } else if (argv[i][0] == '-') {
            fprintf(stderr, BIN ": Unknown option \"%s\".\n", argv[i]);
            goto exit;

        /* Positional: output path */
        } else {
            if (pos_count >= 2) {
                fprintf(stderr, BIN ": Too many arguments.\n");
                goto exit;
            }
            pos[pos_count++] = argv[i];
        }
    }

    /* Accept at most one positional argument after "--" */
    for (; i < argc; i++) {
        if (pos_count >= 2) {
            fprintf(stderr, BIN ": Too many arguments.\n");
            goto exit;
        }
        pos[pos_count++] = argv[i];
    }

    /* Assign source and output from positional arguments */
    char *source;
    if (*cmd == CMD_PATCH) {
        if (pos_count != 2) {
            fputs(BIN ": 'patch' requires exactly two arguments: <source.iso> <output.iso>.\n"
                      "Try '" BIN " patch --help' for usage.\n", stderr);
            goto exit;
        }
        source  = pos[0];
        *output = pos[1];
    } else if (*cmd == CMD_EXTRACT) {
        if (pos_count != 2) {
            fputs(BIN ": 'extract' requires exactly two arguments: <source> <output-dir>.\n"
                      "Try '" BIN " extract --help' for usage.\n", stderr);
            goto exit;
        }
        source  = pos[0];
        *output = pos[1];
    } else { /* CMD_CHECK */
        if (pos_count != 1) {
            fputs(BIN ": 'check' requires exactly one argument: <source>.\n"
                      "Try '" BIN " check --help' for usage.\n", stderr);
            goto exit;
        }
        source = pos[0];
    }

#ifdef _WIN32
    {
        wchar_t *_wsrc = path_to_wide(source);
        wchar_t  _wabs[_MAX_PATH];
        if (_wsrc && _wfullpath(_wabs, _wsrc, _MAX_PATH) != NULL) {
            /* Convert back to the system ANSI code page so that the path
             * can be passed to libbluray's bd_open() which uses ANSI APIs
             * internally on Windows. */
            WideCharToMultiByte(CP_ACP, 0, _wabs, -1, abs_source, sizeof(abs_source), NULL, NULL);
            source = abs_source;
        }
        free(_wsrc);
    }
#endif
    if (!bd_source_exists(source)) goto exit;

    if (keyfile == NULL) {
        keyfile = search_keyfile();
        if (keyfile == NULL) {
            fputs(BIN ": Can't find keys database file.\n", stderr);
            goto exit;
        } else {
            free_keyfile = 1;
        }

    } else if (!regular_file_exists(keyfile)) {
        goto exit;
    }

    *bluray = open_bluray(source, keyfile);

    /* Return a heap-allocated copy of the resolved source path.
     * Caller is responsible for free()ing it. */
    if (*bluray != NULL && source_path != NULL) {
        size_t slen  = strlen(source) + 1;
        *source_path = malloc(slen);
        if (*source_path == NULL)
            fputs(BIN ": Can't copy source path.\n", stderr);
        else
            memcpy(*source_path, source, slen);
    }

exit:
    if (free_keyfile) free(keyfile);
    if (*bluray == NULL) exit(1);
}
