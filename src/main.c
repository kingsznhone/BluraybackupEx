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
    int     only_main_title, success;
    size_t  buf_size;

    if (argc == 1) {
        fputs(BIN " " VERSION " - Blu-ray Disc backup tool\n"
                  "Usage: " BIN
                  " {-d device | -i input} [-k keyfile] [-o outdir]\n"
                  "       " BIN
                  " {-d device | -i input} [-k keyfile] -m [-o outdir]\n"
                  "Try '" BIN " --help' for more information.\n",
              stdout);
        return 0;
    }

    init(argc, argv, &bluray, &only_main_title, &output_dir, &buf_size);
    success = 0;

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    if (only_main_title) {
        char  *dest;
        size_t len;

        if (output_dir != NULL) {
            if (bd_mkdir(output_dir) == -1 && errno != EEXIST) {
                fprintf(stderr, BIN ": Can't create output directory %s.\n",
                        output_dir);
                goto done;
            }
            len  = strlen(output_dir) + 1 + strlen("main_title.m2ts") + 1;
            dest = malloc(len);
            if (dest == NULL) {
                fputs(BIN ": Can't allocate memory.\n", stderr);
                goto done;
            }
            sprintf(dest, "%s/main_title.m2ts", output_dir);
        } else {
            dest = malloc(strlen("main_title.m2ts") + 1);
            if (dest == NULL) {
                fputs(BIN ": Can't allocate memory.\n", stderr);
                goto done;
            }
            strcpy(dest, "main_title.m2ts");
        }
        success = copy_main_title(bluray, dest, buf_size);
        free(dest);
    } else {
        if (output_dir != NULL) {
            if (bd_mkdir(output_dir) == -1 && errno != EEXIST) {
                fprintf(stderr, BIN ": Can't create output directory %s.\n",
                        output_dir);
                goto done;
            }
            if (chdir(output_dir) == -1) {
                fprintf(stderr, BIN ": Can't change to output directory %s.\n",
                        output_dir);
                goto done;
            }
        }
        success = copy_dir(bluray, "", buf_size);
    }

done:
    bd_close(bluray);

    return !success;
}
