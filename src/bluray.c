#include "bluray.h"

#include "async_writer.h"
#include "progress.h"

#include <inttypes.h>
#include <libbluray/filesystem.h>
#include <libbluray/meta_data.h>
#include <udfread/udfread.h>


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
        if (S_ISBLK(info.st_mode) || S_ISCHR(info.st_mode) || S_ISREG(info.st_mode) || S_ISDIR(info.st_mode))
            exist = 1;
        else
            fprintf(stderr, BIN ": %s isn't a block/character device, image file, or directory.\n", path);
    }

    return exist;
}

int extract_dir(BLURAY *bluray, const char *path, size_t buf_size) {
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

        new_path = malloc(sizeof(char) * (strlen(path) + strlen(dirent->d_name) + 2));
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
            if (!extract_dir(bluray, new_path, buf_size)) all_good = 0;
        } else {
            if (!extract_file(bluray, new_path, new_path, buf_size)) all_good = 0;
        }

        free(new_path);
    } while (running);
    if (!running) all_good = 0;

    free(dirent);

    dir->close(dir);

    return all_good;
}

int extract_file(BLURAY *bluray, const char *src, const char *dst, size_t buf_size) {
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
            read = src_file->read(src_file, bufs[cur] + buf_pos, ENCRYPTED_BYTES_TO_READ);
            if (read <= 0) break;
            buf_pos += (size_t)read;
        }
        if (buf_pos > 0) {
            if (aw_submit(&writer, bufs[cur], buf_pos) != 0) {
                success = 0;
                if (dst_file == stdout)
                    fputs(BIN ": Can't write to standard output.\n", stderr);
                else
                    fprintf(stderr, BIN ": Destination write error on %s.\n", dst);
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

/* Verify a single file by reading every AACS unit (6144 bytes) and letting
 * libaacs validate the MAC.  On any read failure the byte offset is reported
 * and, if possible, reading continues from the next unit.  Returns the number
 * of unreadable blocks encountered. */
static int verify_file(BLURAY *bluray, const char *path, size_t buf_size) {
    uint8_t          *buf;
    struct bd_file_s *src_file;
    int64_t           read;
    int64_t           offset;
    int64_t           file_size;
    int               errors;
    uint64_t          start_ms, last_update_ms;

    errors    = 0;
    offset    = 0;
    file_size = -1;
    read      = 0;

    buf = malloc(buf_size);
    if (buf == NULL) {
        fputs(BIN ": Can't allocate verify buffer.\n", stderr);
        return 0;
    }

    src_file = bd_open_file_dec(bluray, path);
    if (src_file == NULL) {
        fprintf(stderr, BIN ": Can't open Blu-ray file %s.\n", path);
        free(buf);
        return 0;
    }

    if (src_file->seek != NULL) {
        file_size = src_file->seek(src_file, 0, SEEK_END);
        if (file_size <= 0) file_size = -1;
        src_file->seek(src_file, 0, SEEK_SET);
    }

    start_ms = last_update_ms = get_time_ms();
    print_progress(path, 0, file_size, start_ms, 0);

    while (running) {
        size_t buf_pos = 0;
        /* Accumulate as many AACS units as fit in buf_size before processing.
         * This mirrors extract_file's inner loop and keeps per-read overhead low.
         */
        while (running && buf_pos + ENCRYPTED_BYTES_TO_READ <= buf_size) {
            read = src_file->read(src_file, buf + buf_pos, ENCRYPTED_BYTES_TO_READ);
            if (read <= 0) break;
            buf_pos += (size_t)read;
        }
        if (buf_pos > 0) {
            offset += (int64_t)buf_pos;
            uint64_t now = get_time_ms();
            if (now - last_update_ms >= 1000) {
                print_progress(path, offset, file_size, start_ms, 0);
                last_update_ms = now;
            }
        }
        if (read < 0) {
            fputc('\n', stderr);
            fprintf(stderr, BIN ": Read error in %s at byte offset %" PRId64 ".\n", path, offset);
            errors++;
            /* Try to skip to the next AACS unit and continue scanning. */
            if (src_file->seek == NULL) break;
            offset = ((offset / ENCRYPTED_BYTES_TO_READ) + 1) * ENCRYPTED_BYTES_TO_READ;
            if (src_file->seek(src_file, offset, SEEK_SET) < 0) break;
            read = 0; /* reset so outer loop continues */
            continue;
        }
        if (read == 0 || !running) break; /* EOF */
    }

    print_progress(path, offset, file_size, start_ms, 1);

    free(buf);
    src_file->close(src_file);
    return errors;
}

int verify_dir(BLURAY *bluray, const char *path, size_t buf_size) {
    int              total_errors;
    int              read;
    struct bd_dir_s *dir;
    BD_DIRENT       *dirent;
    char            *new_path;

    total_errors = 0;
    read         = 0;

    dir = bd_open_dir(bluray, path);
    if (dir == NULL) {
        fprintf(stderr, BIN ": Can't open Blu-ray dir %s.\n", path);
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
        if (read != 0) break;

        new_path = malloc(sizeof(char) * (strlen(path) + strlen(dirent->d_name) + 2));
        if (new_path == NULL) {
            fputs(BIN ": Can't allocate memory for new_path.\n", stderr);
            break;
        }
        strcpy(new_path, path);
#ifdef _WIN32
        if (strcmp(path, "") != 0) strcat(new_path, "\\");
#else
        if (strcmp(path, "") != 0) strcat(new_path, "/");
#endif
        strcat(new_path, dirent->d_name);

        if (strchr(dirent->d_name, '.') == NULL)
            total_errors += verify_dir(bluray, new_path, buf_size);
        else
            total_errors += verify_file(bluray, new_path, buf_size);

        free(new_path);
    } while (running);

    free(dirent);
    dir->close(dir);
    return total_errors;
}

static const char *video_format_str(uint8_t fmt) {
    switch (fmt) {
    case 1:
        return "480i";
    case 2:
        return "576i";
    case 3:
        return "480p";
    case 4:
        return "1080i";
    case 5:
        return "720p";
    case 6:
        return "1080p";
    case 7:
        return "576p";
    case 8:
        return "2160p (4K)";
    default:
        return "(unknown)";
    }
}

static const char *frame_rate_str(uint8_t rate) {
    switch (rate) {
    case 1:
        return "23.976 Hz";
    case 2:
        return "24 Hz";
    case 3:
        return "25 Hz";
    case 4:
        return "29.97 Hz";
    case 6:
        return "50 Hz";
    case 7:
        return "59.94 Hz";
    default:
        return "(unknown)";
    }
}

static const char *dynamic_range_str(uint8_t dr) {
    switch (dr) {
    case 0:
        return "SDR";
    case 1:
        return "HDR10";
    case 2:
        return "Dolby Vision";
    default:
        return "(unknown)";
    }
}

/* Returns the terminal display width of a UTF-8 string.
 * East-Asian wide (CJK, fullwidth, etc.) code points count as 2 columns;
 * everything else counts as 1.  Control/combining characters are ignored. */
static int utf8_display_width(const char *s) {
    int width = 0;
    while (*s) {
        uint32_t      cp = 0;
        unsigned char c  = (unsigned char)*s;
        int           bytes;
        if (c < 0x80) {
            cp    = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp    = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp    = c & 0x0F;
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp    = c & 0x07;
            bytes = 4;
        } else {
            s++;
            continue;
        }
        for (int i = 1; i < bytes; i++) {
            if (((unsigned char)s[i] & 0xC0) != 0x80) {
                bytes = 1;
                break;
            }
            cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
        }
        s += bytes;
        /* East-Asian wide ranges (Unicode Standard Annex #11) */
        if ((cp >= 0x1100 && cp <= 0x115F) || (cp == 0x2329 || cp == 0x232A) || (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0x33FF) ||
            (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0xA4CF) || (cp >= 0xA960 && cp <= 0xA97F) || (cp >= 0xAC00 && cp <= 0xD7FF) ||
            (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE1F) || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
            (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1B000 && cp <= 0x1B0FF) || (cp >= 0x1F300 && cp <= 0x1F64F) || (cp >= 0x20000 && cp <= 0x2FFFD) ||
            (cp >= 0x30000 && cp <= 0x3FFFD))
            width += 2;
        else if (cp >= 0x20) /* skip control chars */
            width += 1;
    }
    return width;
}

/* Prints a table row: │ left-24 │ value padded to 48 display cols │ */
static void print_row_str(const char *label, const char *value) {
    int dw  = utf8_display_width(value);
    int pad = 48 - dw;
    if (pad < 0) pad = 0;
    fprintf(stderr, "\u2502 %-24s \u2502 %s%*s \u2502\n", label, value, pad, "");
}

static void print_row_uint(const char *label, unsigned int value) {
    fprintf(stderr, "\u2502 %-24s \u2502 %-48u \u2502\n", label, value);
}

static void print_row_int(const char *label, int value) {
    fprintf(stderr, "\u2502 %-24s \u2502 %-48d \u2502\n", label, value);
}

#define YN(x) ((x) ? "Yes" : "No")

void log_disc_info(BLURAY *bluray, const BLURAY_DISC_INFO *info) {
    const struct meta_dl *meta;
    const char           *disc_name     = NULL;
    const char           *udf_volume_id = NULL;
    char                  disc_id_hex[41];
    int                   i;

    /* Resolve disc name: BD metadata > UDF volume ID > disclib XML */
    if (info->disc_name != NULL && info->disc_name[0] != '\0') disc_name = info->disc_name;
    if (info->udf_volume_id != NULL && info->udf_volume_id[0] != '\0') udf_volume_id = info->udf_volume_id;
    if (disc_name == NULL) {
        if (udf_volume_id != NULL) {
            disc_name = udf_volume_id;
        } else {
            meta = bd_get_meta(bluray);
            if (meta != NULL && meta->di_name != NULL && meta->di_name[0] != '\0') disc_name = meta->di_name;
        }
    }

    /* Format disc_id as hex string */
    for (i = 0; i < 20; i++) sprintf(disc_id_hex + i * 2, "%02X", info->disc_id[i]);
    disc_id_hex[40] = '\0';

    /* Table layout: │ 24-char-left │ 48-char-right │ = 79 chars total
     * Row:  │(1) (1)24(1) │(1) (1)48(1) │(1) = 79
     * Divs: ├──26──┬──50──┤  /  ├──26──┴──50──┤               */
    fputs("┌─────────────────────────────────────────────────────────────────────────────┐\n", stderr);
    fputs("│                           Blu-ray Disc Information                          │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_str("Disc Name", disc_name != NULL ? disc_name : "(unknown)");
    print_row_str("UDF Volume ID", udf_volume_id != NULL ? udf_volume_id : "(none)");
    print_row_str("Disc ID", disc_id_hex);
    print_row_str("BluRay Detected", YN(info->bluray_detected));
    fputs("├──────────────────────────┴──────────────────────────────────────────────────┤\n", stderr);
    fputs("│  Titles                                                                     │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_uint("Total Titles", info->num_titles);
    print_row_uint("HDMV Titles", info->num_hdmv_titles);
    print_row_uint("BD-J Titles", info->num_bdj_titles);
    print_row_uint("Unsupported Titles", info->num_unsupported_titles);
    print_row_str("No Menu Support", YN(info->no_menu_support));
    print_row_str("First Play Supported", YN(info->first_play_supported));
    print_row_str("Top Menu Supported", YN(info->top_menu_supported));
    fputs("├──────────────────────────┴──────────────────────────────────────────────────┤\n", stderr);
    fputs("│  Video                                                                      │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_str("Video Format", video_format_str(info->video_format));
    print_row_str("Frame Rate", frame_rate_str(info->frame_rate));
    print_row_str("3D Content", YN(info->content_exist_3D));
    print_row_str("Output Mode Preference", info->initial_output_mode_preference ? "3D" : "2D");
    print_row_str("Dynamic Range", dynamic_range_str(info->initial_dynamic_range_type));
    fputs("├──────────────────────────┴──────────────────────────────────────────────────┤\n", stderr);
    fputs("│  AACS                                                                       │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_str("AACS Detected", YN(info->aacs_detected));
    print_row_str("libaacs Available", YN(info->libaacs_detected));
    print_row_str("AACS Handled", YN(info->aacs_handled));
    print_row_int("AACS Error Code", info->aacs_error_code);
    print_row_int("AACS MKB Version", info->aacs_mkbv);
    fputs("├──────────────────────────┴──────────────────────────────────────────────────┤\n", stderr);
    fputs("│  BD+                                                                        │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_str("BD+ Detected", YN(info->bdplus_detected));
    print_row_str("libbdplus Available", YN(info->libbdplus_detected));
    print_row_str("BD+ Handled", YN(info->bdplus_handled));
    print_row_uint("BD+ Content Code Gen", info->bdplus_gen);
    if (info->bdplus_date != 0) {
        char date_buf[16];
        sprintf(date_buf, "%04u-%02u-%02u", (info->bdplus_date >> 16) & 0xFFFF, (info->bdplus_date >> 8) & 0xFF, info->bdplus_date & 0xFF);
        print_row_str("BD+ Code Date", date_buf);
    } else {
        print_row_str("BD+ Code Date", "(none)");
    }
    fputs("├──────────────────────────┴──────────────────────────────────────────────────┤\n", stderr);
    fputs("│  BD-J (Java)                                                                │\n", stderr);
    fputs("├──────────────────────────┬──────────────────────────────────────────────────┤\n", stderr);
    print_row_str("BD-J Detected", YN(info->bdj_detected));
    print_row_str("Java VM Available", YN(info->libjvm_detected));
    print_row_str("BD-J Handled", YN(info->bdj_handled));
    print_row_str("BD-J Org ID", info->bdj_org_id[0] ? info->bdj_org_id : "(none)");
    print_row_str("BD-J Disc ID", info->bdj_disc_id[0] ? info->bdj_disc_id : "(none)");
    fputs("└──────────────────────────┴──────────────────────────────────────────────────┘\n", stderr);
}

#undef YN

BLURAY *open_bluray(const char *device, const char *keyfile) {
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

    log_disc_info(bluray, info);

    if (info->aacs_detected == 1 && info->libaacs_detected != 1) {
        fputs(BIN ": To decode an AACS encrypted disc install libaacs.\n", stderr);
        bd_close(bluray);
        return NULL;
    }
    if (info->aacs_detected == 1 && info->aacs_handled != 1) {
        fputs(BIN ": Unsupported AACS encoding.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    if (info->bdplus_detected == 1 && info->libbdplus_detected != 1) {
        fputs(BIN ": To decode a BD+ encrypted disc install libbdplus.\n", stderr);
        bd_close(bluray);
        return NULL;
    }
    if (info->bdplus_detected == 1 && info->bdplus_handled != 1) {
        fputs(BIN ": Unsupported BD+ encoding.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    if ((info->aacs_detected == 1 || info->bdplus_detected == 1) && bd_get_titles(bluray, TITLES_ALL, 0) == 0) {
        fputs(BIN ": Can't get Blu-ray titles.\n", stderr);
        bd_close(bluray);
        return NULL;
    }

    return bluray;
}

char *get_disc_label(BLURAY *bluray) {
    const BLURAY_DISC_INFO *info;
    const struct meta_dl   *meta;
    const char             *raw = NULL;
    char                   *label;
    size_t                  i, len;

    info = bd_get_disc_info(bluray);
    if (info != NULL) {
        if (info->udf_volume_id != NULL && info->udf_volume_id[0] != '\0')
            raw = info->udf_volume_id;
        else if (info->disc_name != NULL && info->disc_name[0] != '\0')
            raw = info->disc_name;
    }
    if (raw == NULL) {
        meta = bd_get_meta(bluray);
        if (meta != NULL && meta->di_name != NULL && meta->di_name[0] != '\0') raw = meta->di_name;
    }

    /* Fallback: disc_id as hex string */
    if (raw == NULL) {
        char hex[41];
        if (info != NULL) {
            for (i = 0; i < 20; i++) sprintf(hex + i * 2, "%02X", info->disc_id[i]);
            hex[40] = '\0';
        } else {
            strcpy(hex, "UNKNOWN");
        }
        label = malloc(sizeof(hex));
        if (label == NULL) return NULL;
        strcpy(label, hex);
        return label;
    }

    len   = strlen(raw);
    label = malloc(len + 1);
    if (label == NULL) return NULL;

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)raw[i];
#ifdef _WIN32
        /* Windows forbidden filename chars and control chars */
        if (c < 0x20 || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            label[i] = '_';
        else
            label[i] = (char)c;
#else
        label[i] = (c == '/' || c == '\0') ? '_' : (char)c;
#endif
    }
    label[len] = '\0';
    return label;
}

/* --------------------------------------------------------------------------
 * ISO dump: sector-level decrypted copy
 * --------------------------------------------------------------------------
 *
 * Phase 1 – raw stream copy of the source file to output_path.
 * Phase 2 – for every .m2ts file found in the BD virtual filesystem, ask
 *            libudfread for the file's starting sector (LBA) inside the
 *            ISO and overwrite those sectors with the AACS-decrypted content
 *            obtained via bd_open_file_dec().
 *
 * Both phases show progress via print_progress().
 */

/* Returns 1 if the filename ends with ".m2ts" (case-insensitive). */
static int is_m2ts(const char *name) {
    size_t len = strlen(name);
    if (len < 5) return 0;
    const char *e = name + len - 5;
    return (e[0] == '.') && (e[1] == 'm' || e[1] == 'M') && (e[2] == '2') && (e[3] == 't' || e[3] == 'T') && (e[4] == 's' || e[4] == 'S');
}

/* Patch one .m2ts file in the output ISO with decrypted content.
 * bd_path  – path as returned by bd_open_dir (backslash on Windows)
 * udf      – open udfread handle on the *source* ISO
 * dst      – output ISO file opened for random-write ("r+b")
 * buf_size – I/O buffer size (will be rounded down to a multiple of 6144) */
static int patch_iso_stream(BLURAY *bluray, udfread *udf, FILE *dst, const char *bd_path, size_t buf_size) {
    UDFFILE          *udf_file = NULL;
    struct bd_file_s *bd_file  = NULL;
    uint8_t          *buf      = NULL;
    int64_t           file_size;
    int64_t           written = 0;
    int64_t           rd      = 0;
    int               success = 1;
    uint64_t          start_ms, last_update_ms;
    size_t            effective_buf;
    uint32_t          file_block = 0; /* logical 2048-byte block index within the file */

    /* Keep udf_file open throughout: we need per-block LBA lookups during
     * patching to correctly handle files whose UDF extents are not physically
     * contiguous in the ISO (UDF limits a single extent to ~1 GB, so any file
     * larger than ~1 GB uses multiple extents that may not be adjacent). */
    udf_file = udfread_file_open(udf, bd_path);
    if (udf_file == NULL) {
        fprintf(stderr, BIN ": udfread: can't open %s.\n", bd_path);
        return 0;
    }
    file_size = udfread_file_size(udf_file);

    if (udfread_file_lba(udf_file, 0) == 0) {
        /* LBA 0 is the system area; no BD data file lives there. */
        fprintf(stderr, BIN ": Can't determine sector address for %s.\n", bd_path);
        udfread_file_close(udf_file);
        return 0;
    }

    /* Open the same file via libbluray for AACS-decrypted reads. */
    bd_file = bd_open_file_dec(bluray, bd_path);
    if (bd_file == NULL) {
        fprintf(stderr, BIN ": Can't open %s for decryption.\n", bd_path);
        udfread_file_close(udf_file);
        return 0;
    }

    /* Round buf_size down to a whole number of AACS units (6144 bytes). */
    effective_buf = (buf_size / ENCRYPTED_BYTES_TO_READ) * ENCRYPTED_BYTES_TO_READ;
    if (effective_buf == 0) effective_buf = (size_t)ENCRYPTED_BYTES_TO_READ;

    buf = malloc(effective_buf);
    if (buf == NULL) {
        fputs(BIN ": Can't allocate patch buffer.\n", stderr);
        success = 0;
        goto done;
    }

    start_ms = last_update_ms = get_time_ms();
    print_progress(bd_path, 0, file_size, start_ms, 0);

    while (running) {
        size_t buf_pos = 0;
        /* Read as many AACS units as fit in the buffer. */
        while (running && buf_pos + ENCRYPTED_BYTES_TO_READ <= effective_buf) {
            rd = bd_file->read(bd_file, buf + buf_pos, ENCRYPTED_BYTES_TO_READ);
            if (rd <= 0) break;
            buf_pos += (size_t)rd;
        }
        if (buf_pos == 0) break;

        /* Write the buffer one 2048-byte sector at a time, seeking to each
         * block's physical LBA.  This correctly handles non-contiguous UDF
         * extents without any look-ahead optimisation. */
        for (size_t offset = 0; offset < buf_pos; offset += 2048, file_block++) {
            size_t   sector = buf_pos - offset >= 2048 ? 2048 : buf_pos - offset;
            uint32_t lba    = udfread_file_lba(udf_file, file_block);
            if (lba == 0) {
                fprintf(stderr, BIN ": Can't resolve LBA for block %u in %s.\n", file_block, bd_path);
                success = 0;
                goto done;
            }
            if (fseeko(dst, (int64_t)lba * 2048, SEEK_SET) != 0) {
                fprintf(stderr, BIN ": Seek error in output for %s.\n", bd_path);
                success = 0;
                goto done;
            }
            if (fwrite(buf + offset, 1, sector, dst) != sector) {
                fprintf(stderr, BIN ": Write error patching %s.\n", bd_path);
                success = 0;
                goto done;
            }
        }

        written += (int64_t)buf_pos;


        uint64_t now = get_time_ms();
        if (now - last_update_ms >= 1000) {
            print_progress(bd_path, written, file_size, start_ms, 0);
            last_update_ms = now;
        }

        if (rd <= 0) break;
    }

    print_progress(bd_path, written, file_size, start_ms, 1);
    if (!running) success = 0;

done:
    free(buf);
    bd_file->close(bd_file);
    udfread_file_close(udf_file);
    return success;
}

/* Recursively walk the BD virtual filesystem.  For every .m2ts file found,
 * call patch_iso_stream() to overwrite its sectors with decrypted data. */
static int patch_iso_dir(BLURAY *bluray, udfread *udf, FILE *dst, const char *path, size_t buf_size) {
    int              all_good = 1;
    int              read     = 0;
    struct bd_dir_s *dir;
    BD_DIRENT       *dirent;
    char            *new_path;

    dir = bd_open_dir(bluray, path);
    if (dir == NULL) {
        fprintf(stderr, BIN ": Can't open BD dir %s.\n", path);
        return 0;
    }

    dirent = malloc(sizeof(BD_DIRENT));
    if (dirent == NULL) {
        fputs(BIN ": Can't allocate BD_DIRENT.\n", stderr);
        dir->close(dir);
        return 0;
    }

    do {
        read = dir->read(dir, dirent);
        if (read != 0) break;

        new_path = malloc(strlen(path) + strlen(dirent->d_name) + 2);
        if (new_path == NULL) {
            fputs(BIN ": Can't allocate path buffer.\n", stderr);
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
            /* No dot in name → treat as directory. */
            if (!patch_iso_dir(bluray, udf, dst, new_path, buf_size)) all_good = 0;
        } else if (is_m2ts(dirent->d_name)) {
            /* Encrypted video stream: patch with decrypted content. */
            if (!patch_iso_stream(bluray, udf, dst, new_path, buf_size)) all_good = 0;
        }

        free(new_path);
    } while (running);

    free(dirent);
    dir->close(dir);
    return all_good && running;
}

int dump_iso(BLURAY *bluray, const char *iso_path, const char *output_path, size_t buf_size) {
    FILE          *src     = NULL;
    FILE          *dst     = NULL;
    uint8_t       *bufs[2] = {NULL, NULL};
    async_writer_t p1_writer;
    int            p1_aw_started = 0;
    udfread       *udf           = NULL;
    int64_t        total_size    = 0;
    int            success       = 0;

    /* Verify that the source is a regular file (not a device or directory). */
    {
        bd_stat_s info;
        if (!file_exists(iso_path, &info) || !S_ISREG(info.st_mode)) {
            fputs(BIN ": --iso requires a regular file as source (-i <file.iso>)."
                      "\n",
                  stderr);
            return 0;
        }
    }

    bufs[0] = malloc(buf_size);
    bufs[1] = malloc(buf_size);
    if (bufs[0] == NULL || bufs[1] == NULL) {
        fputs(BIN ": Can't allocate copy buffer.\n", stderr);
        free(bufs[0]);
        free(bufs[1]);
        return 0;
    }

    /* ---- Phase 1: raw stream copy ---- */
    fputs("\n"
          "┌─────────────────────────────────────────────────────────────────────────────┐\n"
          "│                          Phase 1/2 - Raw ISO Copy                           │\n"
          "└─────────────────────────────────────────────────────────────────────────────┘\n",
          stderr);

    src = bd_fopen(iso_path, "rb");
    if (src == NULL) {
        fprintf(stderr, BIN ": Can't open source %s.\n", iso_path);
        goto done;
    }
    if (fseeko(src, 0, SEEK_END) != 0 || (total_size = ftello(src)) <= 0) {
        fprintf(stderr, BIN ": Can't determine size of %s.\n", iso_path);
        goto done;
    }
    fseeko(src, 0, SEEK_SET);

    dst = bd_fopen(output_path, "wb");
    if (dst == NULL) {
        fprintf(stderr, BIN ": Can't create output file %s.\n", output_path);
        goto done;
    }
    setvbuf(dst, NULL, _IONBF, 0);

    {
        int64_t  written_total = 0;
        size_t   n;
        int      cur            = 0;
        uint64_t start_ms       = get_time_ms();
        uint64_t last_update_ms = start_ms;

        if (aw_init(&p1_writer, dst) != 0) {
            fputs(BIN ": Can't create writer thread.\n", stderr);
            goto done;
        }
        p1_aw_started = 1;

        print_progress(output_path, 0, total_size, start_ms, 0);

        while (running) {
            n = fread(bufs[cur], 1, buf_size, src);
            if (n == 0) break;
            if (aw_submit(&p1_writer, bufs[cur], n) != 0) {
                fprintf(stderr, BIN ": Write error on %s.\n", output_path);
                goto done;
            }
            cur ^= 1;
            written_total += (int64_t)n;
            {
                uint64_t now = get_time_ms();
                if (now - last_update_ms >= 1000) {
                    print_progress(output_path, written_total, total_size, start_ms, 0);
                    last_update_ms = now;
                }
            }
        }
        print_progress(output_path, written_total, total_size, start_ms, 1);
        if (aw_finish(&p1_writer) != 0) {
            p1_aw_started = 0;
            fprintf(stderr, BIN ": Write error on %s.\n", output_path);
            goto done;
        }
        p1_aw_started = 0;
    }

    if (!running) goto done;

    fclose(src);
    src = NULL;
    fclose(dst);
    dst = NULL;

    /* ---- Phase 2: decrypt and overwrite .m2ts streams ---- */
    fputs("\n"
          "┌─────────────────────────────────────────────────────────────────────────────┐\n"
          "│                     Phase 2/2 - Decrypting Streams                          │\n"
          "└─────────────────────────────────────────────────────────────────────────────┘\n",
          stderr);

    /* Skip Phase 2 entirely when there is nothing to decrypt. */
    {
        const BLURAY_DISC_INFO *info = bd_get_disc_info(bluray);
        if (info != NULL && info->aacs_detected == 0 && info->bdplus_detected == 0) {
            fputs(BIN ": Disc is not encrypted; Phase 2 skipped.\n", stderr);
            success = 1;
            goto done;
        }
    }

    udf = udfread_init();
    if (udf == NULL) {
        fputs(BIN ": Can't initialise udfread.\n", stderr);
        goto done;
    }
    if (udfread_open(udf, iso_path) < 0) {
        fprintf(stderr, BIN ": udfread can't open %s.\n", iso_path);
        goto done;
    }

    dst = bd_fopen(output_path, "r+b");
    if (dst == NULL) {
        fprintf(stderr, BIN ": Can't reopen output for update: %s.\n", output_path);
        goto done;
    }
    setvbuf(dst, NULL, _IONBF, 0);

    success = patch_iso_dir(bluray, udf, dst, "", buf_size);

done:
    if (p1_aw_started) aw_finish(&p1_writer);
    free(bufs[0]);
    free(bufs[1]);
    if (src != NULL) fclose(src);
    if (dst != NULL) fclose(dst);
    if (udf != NULL) udfread_close(udf);
    return success;
}
