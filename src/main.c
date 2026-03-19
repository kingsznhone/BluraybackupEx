#include "bluray.h"
#include "cli.h"
#include "compat.h"

/* C17 sanity checks */
_Static_assert(sizeof(size_t) >= 4, "size_t must be at least 32 bits");
_Static_assert(
    ENCRYPTED_BYTES_TO_READ % 2048 == 0,
    "ENCRYPTED_BYTES_TO_READ must be a multiple of one BD sector (2048 bytes)");

volatile sig_atomic_t running = 1;

static void stop(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    BLURAY *bluray = NULL;
    char   *output_dir;

#ifdef _WIN32
    /* Switch the console to UTF-8 so box-drawing characters and any
     * non-ASCII disc names are displayed correctly. */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    int    success;
    int    check;
    size_t buf_size;
    char  *disc_dir = NULL;

    if (argc == 1) {
        fputs(BIN " " VERSION " - Blu-ray Disc backup tool\n"
                  "Usage: " BIN
                  " {-d device | -i input} [-k keyfile] [-o outdir]\n"
                  "Try '" BIN " --help' for more information.\n",
              stdout);
        return 0;
    }

    check = 0;
    init(argc, argv, &bluray, &output_dir, &buf_size, &check);
    success = 0;

    /* No -o and no -c: disc info was already printed; nothing more to do. */
    if (output_dir == NULL && !check) {
        success = 1;
        goto done;
    }

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    if (check) {
        int errors;
        fputs("\n"
              "┌─────────────────────────────────────────────────────────────────────────────┐\n"
              "│                            Disc Integrity Check                             │\n"
              "└─────────────────────────────────────────────────────────────────────────────┘\n",
              stderr);
        errors = verify_dir(bluray, "", buf_size);
        if (errors == 0)
            fputs("\n" BIN ": Disc check passed. No read errors found.\n", stderr);
        else
            fprintf(stderr,
                    "\n" BIN ": Disc check complete: %d read error(s) found.\n",
                    errors);
        if (output_dir == NULL) {
            success = (errors == 0);
            goto done;
        }
        if (!running) goto done;
    }

    /* Build the actual extraction directory: <output_dir>/<disc_label> */
    {
        char  *label = get_disc_label(bluray);
        size_t len;
        if (label == NULL) {
            fputs(BIN ": Can't determine disc label.\n", stderr);
            goto done;
        }
        len      = strlen(output_dir) + 1 + strlen(label) + 1;
        disc_dir = malloc(len);
        if (disc_dir == NULL) {
            fputs(BIN ": Can't allocate path buffer.\n", stderr);
            free(label);
            goto done;
        }
#ifdef _WIN32
        snprintf(disc_dir, len, "%s\\%s", output_dir, label);
#else
        snprintf(disc_dir, len, "%s/%s", output_dir, label);
#endif
        free(label);
        fprintf(stderr, BIN ": Output directory: %s\n", disc_dir);
    }

    fputs("\n"
          "┌─────────────────────────────────────────────────────────────────────────────┐\n"
          "│                              Extracting Files                               │\n"
          "└─────────────────────────────────────────────────────────────────────────────┘\n",
          stderr);

    if (bd_mkdir(output_dir) == -1 && errno != EEXIST) {
        fprintf(stderr, BIN ": Can't create output directory %s.\n",
                output_dir);
        goto done;
    }
    if (bd_mkdir(disc_dir) == -1 && errno != EEXIST) {
        fprintf(stderr, BIN ": Can't create disc directory %s.\n", disc_dir);
        goto done;
    }
    if (chdir(disc_dir) == -1) {
        fprintf(stderr, BIN ": Can't change to disc directory %s.\n", disc_dir);
        goto done;
    }
    success = copy_dir(bluray, "", buf_size);

done:
    free(disc_dir);
    bd_close(bluray);

    return !success;
}
