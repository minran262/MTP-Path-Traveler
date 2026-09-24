/*
 * mtp_traveler.c
 *
 * A small utility for interacting with MTP (Media Transfer Protocol) devices.
 * Supports enumerating devices and storage volumes, listing folders, and
 * uploading files to a destination path on the device.
 *
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Build (Linux):
 *   gcc -O2 -o mtp_traveler src/mtp_traveler.c $(pkg-config --cflags --libs libmtp)
 *
 * Build (MSYS2 MINGW64):
 *   g++ -o mtp_traveler.exe src/mtp_traveler.c \
 *       -I/mingw64/include/libusb-1.0 \
 *       -I$HOME/libmtp-1.1.21/src \
 *       -L$HOME/libmtp-1.1.21/src/.libs \
 *       -lmtp -lusb-1.0 -liconv -lws2_32 \
 *       -static -static-libgcc -static-libstdc++
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <libmtp.h>

#define PROG "mtp_traveler"

/* ---------- Help ---------- */

static void usage(void) {
    fprintf(stderr,
        "\n"
        "MTP Path Traveler\n"
        "-----------------\n"
        "Utilities for reading and writing files on MTP devices.\n"
        "\n"
        "Usage:\n"
        "  %s list-devices\n"
        "  %s list-storage\n"
        "  %s list-folder <storage_id> <parent_id> [depth]\n"
        "  %s put <local_file> <parent_id> <target_path> [storage_id]\n"
        "\n"
        "Commands:\n"
        "  list-devices   List connected MTP devices.\n"
        "  list-storage   List storage volumes on the first device.\n"
        "  list-folder    List the contents of a folder on the device.\n"
        "  put            Upload a file to the given destination path.\n"
        "\n"
        "Arguments:\n"
        "  <local_file>   Local file to upload.\n"
        "  <parent_id>    ID of the destination folder (see list-folder).\n"
        "                 Accepts decimal or hex (0x...) values.\n"
        "  <target_path>  Name of the file to create on the device.\n"
        "                 Path separators are forwarded to the device.\n"
        "  <storage_id>   Optional. Storage volume ID (auto-detected if omitted).\n"
        "  [depth]        Optional. Recursion depth for list-folder (default: 2).\n"
        "\n"
        "Examples:\n"
        "  %s list-devices\n"
        "  %s list-storage\n"
        "  %s list-folder 3 0\n"
        "  %s put empty.txt 1 report.txt\n"
        "  %s put empty.txt 1 \"docs/report.txt\"\n"
        "\n",
        PROG, PROG, PROG, PROG,
        PROG, PROG, PROG, PROG, PROG);
}

/* ---------- Helpers ---------- */

static int parse_u32(const char *s, uint32_t *out) {
    if (!s || !out) return -1;
    char *end = NULL;
    errno = 0;
    unsigned long v;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        v = strtoul(s + 2, &end, 16);
    } else {
        v = strtoul(s, &end, 10);
    }
    if (errno != 0 || end == s || *end != '\0') return -1;
    *out = (uint32_t)v;
    return 0;
}

static LIBMTP_mtpdevice_t *open_first_device(void) {
    LIBMTP_raw_device_t *devices = NULL;
    int n = 0;
    LIBMTP_error_number_t err = LIBMTP_Detect_Raw_Devices(&devices, &n);
    if (err != LIBMTP_ERROR_NONE || n <= 0 || devices == NULL) {
        fprintf(stderr, "No MTP device detected.\n");
        if (devices) free(devices);
        return NULL;
    }
    /* Uncached mode is required for folder enumeration to work. */
    LIBMTP_mtpdevice_t *dev = LIBMTP_Open_Raw_Device_Uncached(&devices[0]);
    free(devices);
    if (!dev) {
        fprintf(stderr, "Failed to open device.\n");
        return NULL;
    }
    return dev;
}

static uint32_t first_storage_id(LIBMTP_mtpdevice_t *dev) {
    LIBMTP_Get_Storage(dev, LIBMTP_STORAGE_SORTBY_NOTSORTED);
    if (dev->storage) return dev->storage->id;
    return 0;
}

/* ---------- Commands ---------- */

static int cmd_list_devices(void) {
    LIBMTP_raw_device_t *devices = NULL;
    int n = 0;
    LIBMTP_error_number_t err = LIBMTP_Detect_Raw_Devices(&devices, &n);
    if (err != LIBMTP_ERROR_NONE) {
        fprintf(stderr, "Detect error: %d\n", err);
        return 1;
    }
    if (n <= 0) {
        printf("No MTP devices found.\n");
        free(devices);
        return 0;
    }
    printf("Found %d MTP device(s):\n", n);
    for (int i = 0; i < n; i++) {
        printf("  [%d] bus=%u dev=%u  vendor=0x%04x product=0x%04x\n",
               i,
               devices[i].bus_location,
               devices[i].devnum,
               devices[i].device_entry.vendor_id,
               devices[i].device_entry.product_id);
    }
    free(devices);
    return 0;
}

static int cmd_list_storage(void) {
    LIBMTP_mtpdevice_t *dev = open_first_device();
    if (!dev) return 1;

    LIBMTP_Get_Storage(dev, LIBMTP_STORAGE_SORTBY_NOTSORTED);
    LIBMTP_devicestorage_t *s = dev->storage;
    if (!s) {
        printf("No storage volumes reported.\n");
        LIBMTP_Release_Device(dev);
        return 1;
    }
    printf("Storage volumes:\n");
    while (s) {
        printf("  id=%-8u  desc=%-32s  free=%llu / %llu\n",
               s->id,
               s->StorageDescription ? s->StorageDescription : "(unknown)",
               (unsigned long long)s->FreeSpaceInBytes,
               (unsigned long long)s->MaxCapacity);
        s = s->next;
    }
    LIBMTP_Release_Device(dev);
    return 0;
}

static void list_folder_recursive(LIBMTP_mtpdevice_t *dev,
                                  uint32_t storage_id,
                                  uint32_t parent_id,
                                  int depth,
                                  int max_depth)
{
    if (depth > max_depth) return;

    LIBMTP_file_t *files = LIBMTP_Get_Files_And_Folders(dev, storage_id, parent_id);
    if (!files) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("[empty or inaccessible]\n");
        return;
    }

    LIBMTP_file_t *cur = files;
    while (cur) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("%-4s id=%-8u parent=%-8u size=%-12llu  %s\n",
               cur->filetype == LIBMTP_FILETYPE_FOLDER ? "DIR" : "FILE",
               cur->item_id,
               cur->parent_id,
               (unsigned long long)cur->filesize,
               cur->filename ? cur->filename : "(no name)");

        if (cur->filetype == LIBMTP_FILETYPE_FOLDER) {
            list_folder_recursive(dev, storage_id, cur->item_id, depth + 1, max_depth);
        }
        cur = cur->next;
    }

    LIBMTP_file_t *p = files;
    while (p) {
        LIBMTP_file_t *next = p->next;
        LIBMTP_destroy_file_t(p);
        p = next;
    }
}

static int cmd_list_folder(uint32_t storage_id, uint32_t parent_id, int max_depth) {
    LIBMTP_mtpdevice_t *dev = open_first_device();
    if (!dev) return 1;

    if (storage_id == 0) {
        storage_id = first_storage_id(dev);
    }

    printf("Listing storage=%u parent=%u (depth<=%d)\n\n",
           storage_id, parent_id, max_depth);
    list_folder_recursive(dev, storage_id, parent_id, 0, max_depth);

    LIBMTP_Release_Device(dev);
    return 0;
}

static int cmd_put(const char *local_path,
                   uint32_t parent_id,
                   const char *target_path,
                   uint32_t storage_id_override)
{
    /* Verify local file */
    struct stat sb;
    if (stat(local_path, &sb) != 0) {
        fprintf(stderr, "Local file not found: %s\n", local_path);
        return 1;
    }

    LIBMTP_mtpdevice_t *dev = open_first_device();
    if (!dev) return 1;

    uint32_t storage_id = storage_id_override ? storage_id_override
                                              : first_storage_id(dev);
    if (storage_id == 0) {
        fprintf(stderr, "Could not determine storage ID.\n");
        LIBMTP_Release_Device(dev);
        return 1;
    }

    LIBMTP_file_t *file = LIBMTP_new_file_t();
    file->filesize = (uint64_t)sb.st_size;
    file->parent_id = parent_id;
    file->filename = strdup(target_path);
    file->filetype = LIBMTP_FILETYPE_UNKNOWN;
    file->storage_id = storage_id;

    printf("Uploading:\n");
    printf("  source       : %s (%llu bytes)\n", local_path,
           (unsigned long long)sb.st_size);
    printf("  storage_id   : %u\n", storage_id);
    printf("  parent_id    : 0x%08x (%u)\n", parent_id, parent_id);
    printf("  target_path  : %s\n\n", target_path);

    int rc = LIBMTP_Send_File_From_File(dev, local_path, file, NULL, NULL);

    if (rc == 0) {
        printf("OK: file created on device.\n");
    } else {
        printf("ERROR: upload failed (rc=%d)\n", rc);
        LIBMTP_Dump_Errorstack(dev);
    }

    LIBMTP_destroy_file_t(file);
    LIBMTP_Release_Device(dev);
    return rc == 0 ? 0 : 1;
}

/* ---------- Entry point ---------- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
        strcmp(cmd, "-h") == 0) {
        usage();
        return 0;
    }

    LIBMTP_Init();

    if (strcmp(cmd, "list-devices") == 0) {
        return cmd_list_devices();
    }

    if (strcmp(cmd, "list-storage") == 0) {
        return cmd_list_storage();
    }

    if (strcmp(cmd, "list-folder") == 0) {
        if (argc < 4) { usage(); return 1; }
        uint32_t storage_id = 0, parent_id = 0;
        if (parse_u32(argv[2], &storage_id) != 0) {
            fprintf(stderr, "Invalid storage_id: %s\n", argv[2]);
            return 1;
        }
        if (parse_u32(argv[3], &parent_id) != 0) {
            fprintf(stderr, "Invalid parent_id: %s\n", argv[3]);
            return 1;
        }
        int depth = 2;
        if (argc >= 5) {
            depth = atoi(argv[4]);
            if (depth < 0) depth = 0;
            if (depth > 10) depth = 10;
        }
        return cmd_list_folder(storage_id, parent_id, depth);
    }

    if (strcmp(cmd, "put") == 0) {
        if (argc < 5) { usage(); return 1; }
        const char *local_path  = argv[2];
        const char *parent_str  = argv[3];
        const char *target_path = argv[4];

        uint32_t parent_id = 0;
        if (parse_u32(parent_str, &parent_id) != 0) {
            fprintf(stderr, "Invalid parent_id: %s\n", parent_str);
            return 1;
        }

        uint32_t storage_id = 0;
        if (argc >= 6) {
            if (parse_u32(argv[5], &storage_id) != 0) {
                fprintf(stderr, "Invalid storage_id: %s\n", argv[5]);
                return 1;
            }
        }
        return cmd_put(local_path, parent_id, target_path, storage_id);
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    usage();
    return 1;
}