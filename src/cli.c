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

void print_default_usage(void) {
    fputs("Usage: " BIN " {-d device | -i input} [-k keyfile] [-m dir|iso] [output]\n", stdout);
}

void print_help(void) {
    print_default_usage();
    fputs("\n"
          "Options:\n"
          "  -d, --device=PATH   Physical Blu-ray device path (e.g. /dev/sr0, "
          "D:).\n"
          "  -i, --input=PATH    Local disc image file (ISO/BIN) or BDMV "
          "directory.\n"
          "  -k, --keydb=FILE    Path to the AACS keys database file.\n"
          "  -m, --mode=MODE     Output mode: dir (default) or iso.\n"
          "                        dir  – extract the full disc file tree into\n"
          "                               <output>/<disc-label>/.\n"
          "                        iso  – write a single decrypted ISO image to\n"
          "                               <output>. Phase 1 copies every sector\n"
          "                               raw; Phase 2 overwrites encrypted\n"
          "                               .m2ts streams with decrypted content.\n"
          "                               Requires -i <file.iso>.\n"
          "  output              Destination path (directory for dir mode, file\n"
          "                      for iso mode). If omitted, only disc "
          "information\n"
          "                      is printed.\n"
          "  -b, --buffer=SIZE   I/O read buffer size (e.g. 6144, 64k, 1m, 2g).\n"
          "                      Must be >= 6144. Default: 6144 (1 AACS block).\n"
          "                      Larger values improve throughput on "
          "images/SSDs.\n"
          "  -c, --check         Read every file on the disc to verify\n"
          "                      readability. On AACS-encrypted discs each "
          "block\n"
          "                      MAC is validated; errors are reported with the\n"
          "                      file path and byte offset. Can be combined "
          "with\n"
          "                      an output path to verify before extracting.\n"
          "  -h, --help          Print this help text.\n"
          "  -v, --version       Print version and license information.\n"
          "\n"
          "-d and -i are mutually exclusive; specify one to choose the source.\n"
          "If neither is given, the default device is used (/dev/sr0 or D:).\n"
          "\n"
          "If no output path is given, only disc information is displayed\n"
          "(and disc verification if -c is given).\n"
          "Program exits with 0 on success, 1 on error.\n"
          "\n" BIN " exits with 0 on success, 1 on error.\n",
          stdout);
}

void print_version(void) {
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

void init(int argc, char **argv, BLURAY **bluray, char **output, output_mode_t *mode, size_t *buf_size, int *check, char **source_path) {
#ifdef _WIN32
    char abs_source[_MAX_PATH * 4]; /* UTF-8 can be up to 4 bytes per code point */
#endif
    char *device       = NULL;
    int   device_set   = 0;
    int   free_keyfile = 0;
    char *input_file   = NULL;
    char *keyfile      = NULL;
    *output            = NULL;
    *mode              = MODE_DIR;
    *buf_size          = ENCRYPTED_BYTES_TO_READ;
    *check             = 0;
    if (source_path) *source_path = NULL;

    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;

        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--buffer")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": After \"%s\" write the buffer size.\n", argv[i]);
                goto exit;
            }
            *buf_size = parse_size(argv[++i]);
            if (*buf_size < ENCRYPTED_BYTES_TO_READ) {
                fprintf(stderr, BIN ": Invalid buffer size; must be >= %d.\n", ENCRYPTED_BYTES_TO_READ);
                goto exit;
            }

        } else if (!strncmp(argv[i], "-b", 2)) {
            *buf_size = parse_size(argv[i] + 2);
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

        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--check")) {
            *check = 1;

        } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--device")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": After \"%s\" write the Blu-ray device path.\n", argv[i]);
                goto exit;
            }
            device     = argv[++i];
            device_set = 1;

        } else if (!strncmp(argv[i], "-d", 2)) {
            device     = argv[i] + 2;
            device_set = 1;

        } else if (!strncmp(argv[i], "--device=", 9)) {
            device     = argv[i] + 9;
            device_set = 1;

        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_help();
            exit(0);

        } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": After \"%s\" write the input file/directory path.\n", argv[i]);
                goto exit;
            }
            input_file = argv[++i];

        } else if (!strncmp(argv[i], "-i", 2)) {
            input_file = argv[i] + 2;

        } else if (!strncmp(argv[i], "--input=", 8)) {
            input_file = argv[i] + 8;

        } else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--keydb")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": After \"%s\" write the keys database file path.\n", argv[i]);
                goto exit;
            }
            keyfile = argv[++i];

        } else if (!strncmp(argv[i], "-k", 2)) {
            keyfile = argv[i] + 2;

        } else if (!strncmp(argv[i], "--keydb=", 8)) {
            keyfile = argv[i] + 8;

        } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mode")) {
            if (i == argc - 1) {
                fprintf(stderr, BIN ": After \"%s\" write \"dir\" or \"iso\".\n", argv[i]);
                goto exit;
            }
            ++i;
            if (!strcmp(argv[i], "dir")) {
                *mode = MODE_DIR;
            } else if (!strcmp(argv[i], "iso")) {
                *mode = MODE_ISO;
            } else {
                fprintf(stderr,
                        BIN ": Unknown mode \"%s\"; expected \"dir\" or \""
                            "iso\".\n",
                        argv[i]);
                goto exit;
            }

        } else if (!strncmp(argv[i], "-m", 2)) {
            const char *val = argv[i] + 2;
            if (!strcmp(val, "dir")) {
                *mode = MODE_DIR;
            } else if (!strcmp(val, "iso")) {
                *mode = MODE_ISO;
            } else {
                fprintf(stderr,
                        BIN ": Unknown mode \"%s\"; expected \"dir\" or \""
                            "iso\".\n",
                        val);
                goto exit;
            }

        } else if (!strncmp(argv[i], "--mode=", 7)) {
            const char *val = argv[i] + 7;
            if (!strcmp(val, "dir")) {
                *mode = MODE_DIR;
            } else if (!strcmp(val, "iso")) {
                *mode = MODE_ISO;
            } else {
                fprintf(stderr,
                        BIN ": Unknown mode \"%s\"; expected \"dir\" or \""
                            "iso\".\n",
                        val);
                goto exit;
            }

        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            print_version();
            exit(0);

        } else if (!strncmp(argv[i], "-", 1)) {
            fprintf(stderr, BIN ": Unknown \"%s\" option.\n", argv[i]);
            goto exit;

        } else {
            /* First non-flag argument is the output path. */
            if (*output != NULL) {
                fprintf(stderr,
                        BIN ": Unexpected argument \"%s\"; output path is "
                            "already set to \"%s\".\n",
                        argv[i], *output);
                print_default_usage();
                goto exit;
            }
            *output = argv[i];
        }
    }

    /* Accept at most one positional argument after "--". */
    for (; i < argc; i++) {
        if (*output != NULL) {
            fprintf(stderr,
                    BIN ": Unexpected argument \"%s\"; output path is already "
                        "set to \"%s\".\n",
                    argv[i], *output);
            print_default_usage();
            goto exit;
        }
        *output = argv[i];
    }

    /* -d and -i are mutually exclusive */
    if (device_set && input_file != NULL) {
        fputs(BIN ": Options -d and -i are mutually exclusive.\n", stderr);
        goto exit;
    }

    /* Determine source */
    char *source;
    if (input_file != NULL)
        source = input_file;
    else if (device_set)
        source = device;
    else
#ifdef _WIN32
        source = "D:";
#else
        source = "/dev/sr0";
#endif

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
