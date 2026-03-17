#include "bluray.h"

#include "async_writer.h"
#include "progress.h"

#include <libbluray/filesystem.h>


int file_exists(const char *path, bd_stat_s *info) {
    if (bd_stat(path, info) == 0) return 1;
    fprintf(stderr, BIN ": Can't open %s: %s.\n", path, strerror(errno));
    return 0;
}

int bd_source_exists(const char *path) {
    int       exist;
    bd_stat_s info;

    exist = 0;

    if (file_exists(path, &info)) {
        if (S_ISBLK(info.st_mode) || S_ISREG(info.st_mode) ||
            S_ISDIR(info.st_mode))
            exist = 1;
        else
            fprintf(stderr,
                    BIN
                    ": %s isn't a block device, image file, or directory.\n",
                    path);
    }

    return exist;
}

int copy_dir(BLURAY *bluray, const char *path, size_t buf_size) {
    int              all_good, read;
    struct bd_dir_s *dir;
    BD_DIRENT       *dirent;
    char            *new_path;

    all_good = 1;
    read     = 0;

    dir = bd_open_dir(bluray, path);
    if (dir == NULL) {
        fprintf(stderr, BIN ": Can't open Blu-ray dir %s.\n", path);
        return 0;
    }

    if (strcmp(path, "") != 0 && bd_mkdir(path) == -1 && errno != EEXIST) {
        fprintf(stderr, BIN ": Can't create destination dir %s.\n", path);
        return 0;
    }

    dirent = malloc(sizeof(BD_DIRENT));
    if (dirent == NULL) {
        fputs(BIN ": Can't allocate memory for BD_DIRENT.\n", stderr);
        dir->close(dir);
        return 0;
    }

    do {
        read = dir->read(dir, dirent);
        if (read != 0) {
            /* read == 1: EOF, read == -1: error.
             * Either way, dirent is not populated — stop immediately.
             * Note: some libbluray/libudfread versions never return 1 on
             * EOF; they return 0 with no data, then -1 on the next call.
             * See:
             * https://code.videolan.org/videolan/libudfread/-/blob/master/src/udfread.c#L1375
             */
            break;
        }

        new_path =
            malloc(sizeof(char) * (strlen(path) + strlen(dirent->d_name) + 2));
        if (new_path == NULL) {
            fputs(BIN ": Can't allocate memory for new_path.\n", stderr);
            all_good = 0;
            break;
        }
        strcpy(new_path, path);
#ifdef _WIN32
        if (strcmp(path, "") != 0) strcat(new_path, "\\");
#else
        if (strcmp(path, "") != 0) strcat(new_path, "/");
#endif
        strcat(new_path, dirent->d_name);

        if (strchr(dirent->d_name, '.') == NULL) {
            if (!copy_dir(bluray, new_path, buf_size)) all_good = 0;
        } else {
            if (!copy_file(bluray, new_path, new_path, buf_size)) all_good = 0;
        }

        free(new_path);
    } while (running);
    if (!running) all_good = 0;

    free(dirent);

    dir->close(dir);

    return all_good;
}

int copy_file(BLURAY *bluray, const char *src, const char *dst,
              size_t buf_size) {
    uint8_t          *bufs[2];
    FILE             *dst_file;
    int64_t           read = 0;
    struct bd_file_s *src_file;
    int               success, cur;
    async_writer_t    writer;
    int64_t           total_written, file_size;
    uint64_t          start_ms, last_update_ms;

    success       = 1;
    total_written = 0;
    file_size     = -1;

    bufs[0] = malloc(buf_size);
    bufs[1] = malloc(buf_size);
    if (bufs[0] == NULL || bufs[1] == NULL) {
        fputs(BIN ": Can't allocate read buffer.\n", stderr);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    src_file = bd_open_file_dec(bluray, src);
    if (src_file == NULL) {
        fprintf(stderr, BIN ": Can't open Blu-ray file %s.\n", src);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }
    /* Probe file size for progress reporting; falls back gracefully. */
    if (src_file->seek != NULL) {
        file_size = src_file->seek(src_file, 0, SEEK_END);
        if (file_size <= 0) file_size = -1;
        src_file->seek(src_file, 0, SEEK_SET);
    }
    dst_file = dst == NULL ? stdout : bd_fopen(dst, "wb");
    if (dst_file == NULL) {
        fprintf(stderr, BIN ": Can't open destination file %s.\n", dst);
        src_file->close(src_file);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    if (dst_file != stdout) setvbuf(dst_file, NULL, _IONBF, 0);

    if (aw_init(&writer, dst_file) != 0) {
        fputs(BIN ": Can't create writer thread.\n", stderr);
        if (dst_file != stdout) fclose(dst_file);
        src_file->close(src_file);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    start_ms = last_update_ms = get_time_ms();
    print_progress(src, 0, file_size, start_ms, 0);

    cur = 0;
    while (running) {
        size_t buf_pos = 0;
        /* Fill the buffer one AACS unit (6144 bytes) at a time;
         * bd_open_file_dec() requires each read to be exactly unit-sized. */
        while (running && buf_pos + ENCRYPTED_BYTES_TO_READ <= buf_size) {
            read = src_file->read(src_file, bufs[cur] + buf_pos,
                                  ENCRYPTED_BYTES_TO_READ);
            if (read <= 0) break;
            buf_pos += (size_t)read;
        }
        if (buf_pos > 0) {
            if (aw_submit(&writer, bufs[cur], buf_pos) != 0) {
                success = 0;
                if (dst_file == stdout)
                    fputs(BIN ": Can't write to standard output.\n", stderr);
                else
                    fprintf(stderr, BIN ": Destination write error on %s.\n",
                            dst);
                break;
            }
            cur ^= 1;
            total_written += (int64_t)buf_pos;
            {
                uint64_t now = get_time_ms();
                if (now - last_update_ms >= 1000) {
                    print_progress(src, total_written, file_size, start_ms, 0);
                    last_update_ms = now;
                }
            }
        }
        if (read <= 0 || !running) break;
    }
    print_progress(src, total_written, file_size, start_ms, 1);
    if (aw_finish(&writer) != 0 && success) {
        success = 0;
        if (dst_file == stdout)
            fputs(BIN ": Can't write to standard output.\n", stderr);
        else
            fprintf(stderr, BIN ": Destination write error on %s.\n", dst);
    }
    if (read < 0) {
        success = 0;
        fprintf(stderr, BIN ": Blu-ray read error on %s.\n", src);
    }
    if (!running) success = 0;

    if (dst_file != stdout) fclose(dst_file);
    free(bufs[0]);
    free(bufs[1]);
    src_file->close(src_file);

    return success;
}

int copy_main_title(BLURAY *bluray, const char *dst, size_t buf_size) {
    unsigned char     *bufs[2];
    FILE              *dst_file;
    BLURAY_TITLE_INFO *info;
    int                read = 0, success, title, cur;
    async_writer_t     writer;
    int64_t            total_written;
    uint64_t           start_ms, last_update_ms;

    success       = 1;
    total_written = 0;

    bufs[0] = malloc(buf_size);
    bufs[1] = malloc(buf_size);
    if (bufs[0] == NULL || bufs[1] == NULL) {
        fputs(BIN ": Can't allocate read buffer.\n", stderr);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    title = bd_get_main_title(bluray);
    if (title == -1) {
        fputs(BIN ": Can't get main Blu-ray title.\n", stderr);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }
    if (bd_select_title(bluray, title) == 0) {
        fprintf(stderr, BIN ": Can't select Blu-ray title %i.\n", title);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }
    info = bd_get_title_info(bluray, title, 1);
    if (info == NULL) {
        fprintf(stderr, BIN ": Can't get Blu-ray title %i info.\n", title);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }
    dst_file = dst == NULL ? stdout : bd_fopen(dst, "wb");
    if (dst_file == NULL) {
        fprintf(stderr, BIN ": Can't open destination file %s.\n", dst);
        bd_free_title_info(info);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    if (dst_file != stdout) setvbuf(dst_file, NULL, _IONBF, 0);

    if (aw_init(&writer, dst_file) != 0) {
        fputs(BIN ": Can't create writer thread.\n", stderr);
        if (dst_file != stdout) fclose(dst_file);
        bd_free_title_info(info);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    start_ms = last_update_ms = get_time_ms();
    print_progress("main title", 0, -1, start_ms, 0);

    cur = 0;
    while (running && (read = bd_read(bluray, bufs[cur], (int)buf_size)) &&
           read > 0) {
        if (aw_submit(&writer, (uint8_t *)bufs[cur], (size_t)read) != 0) {
            success = 0;
            if (dst_file == stdout)
                fputs(BIN ": Can't write to standard output.\n", stderr);
            else
                fprintf(stderr, BIN ": Destination write error on %s.\n", dst);
            break;
        }
        cur ^= 1;
        total_written += (int64_t)read;
        {
            uint64_t now = get_time_ms();
            if (now - last_update_ms >= 1000) {
                print_progress("main title", total_written, -1, start_ms, 0);
                last_update_ms = now;
            }
        }
    }
    print_progress("main title", total_written, -1, start_ms, 1);
    if (aw_finish(&writer) != 0 && success) {
        success = 0;
        if (dst_file == stdout)
            fputs(BIN ": Can't write to standard output.\n", stderr);
        else
            fprintf(stderr, BIN ": Destination write error on %s.\n", dst);
    }
    if (read < 0) {
        success = 0;
        fprintf(stderr, BIN ": Blu-ray read error on title %i.\n", title);
    }
    if (!running) success = 0;

    if (dst_file != stdout) fclose(dst_file);
    free(bufs[0]);
    free(bufs[1]);
    bd_free_title_info(info);

    return success;
}

BLURAY *open_bluray(const char *device, const char *keyfile,
                    const int copy_titles) {
    BLURAY                 *bluray;
    const BLURAY_DISC_INFO *info;

    bluray = bd_open(device, keyfile);
    if (bluray == NULL) {
        fprintf(stderr, BIN ": Can't open device %s.\n", device);
        return NULL;
    }

    info = bd_get_disc_info(bluray);
    if (info == NULL) {
        fputs(BIN ": Can't get disc info.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    fprintf(stderr,
            BIN ": Disc info: aacs_detected=%d, libaacs_detected=%d, "
                "aacs_handled=%d, bdplus_detected=%d, libbdplus_detected=%d, "
                "bdplus_handled=%d\n",
            info->aacs_detected, info->libaacs_detected, info->aacs_handled,
            info->bdplus_detected, info->libbdplus_detected,
            info->bdplus_handled);

    if (info->aacs_detected == 1 && info->libaacs_detected != 1) {
        fputs(BIN ": To decode an AACS encrypted disc install libaacs.\n",
              stderr);
        bd_close(bluray);
        return NULL;
    }
    if (info->aacs_detected == 1 && info->aacs_handled != 1) {
        fputs(BIN ": Unsupported AACS encoding.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    if (info->bdplus_detected == 1 && info->libbdplus_detected != 1) {
        fputs(BIN ": To decode a BD+ encrypted disc install libbdplus.\n",
              stderr);
        bd_close(bluray);
        return NULL;
    }
    if (info->bdplus_detected == 1 && info->bdplus_handled != 1) {
        fputs(BIN ": Unsupported BD+ encoding.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    if ((info->aacs_detected == 1 || info->bdplus_detected == 1 ||
         copy_titles == 1) &&
        bd_get_titles(bluray, TITLES_RELEVANT, 0) == 0) {
        fputs(BIN ": Can't get Blu-ray titles.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    return bluray;
}
