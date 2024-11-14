/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A little flash file system manager for the Raspberry Pico
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/watchdog.h"

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "lfs.h"
#include "tusb.h"
#include "vi.h"
#include "xreceive.h"
#include "xtransmit.h"

#define MAX_ARGS 4

#define VT_ESC "\033"
#define VT_CLEAR VT_ESC "[H" VT_ESC "[J"

typedef char buf_t[128];

static uint32_t screen_x = 80, screen_y = 24;
static lfs_file_t file;
static buf_t cmd_buffer, curdir, path, result;
static int argc;
static char* argv[MAX_ARGS + 1];
static bool mounted = false, run = true;

static void echo_key(char c) {
    putchar(c);
    if (c == '\r')
        putchar('\n');
}

typedef void (*cmd_func_t)(void);

static const char* search_cmds(int len);

static void parse_cmd(void) {
    // read line into buffer
    char* cp = cmd_buffer;
    char* cp_end = cp + sizeof(cmd_buffer);
    char c;
    do {
        c = getchar();
        if (c == '\t') {
            bool infirst = true;
            for (char* p = cmd_buffer; p < cp; p++)
                if ((*p == ' ') || (*p == ',')) {
                    infirst = false;
                    break;
                }
            if (infirst) {
                const char* p = search_cmds(cp - cmd_buffer);
                if (p) {
                    while (*p) {
                        *cp++ = *p;
                        echo_key(*p++);
                    }
                    *cp++ = ' ';
                    echo_key(' ');
                }
            }
            continue;
        }
        echo_key(c);
        if (c == '\b') {
            if (cp != cmd_buffer) {
                cp--;
                printf(" \b");
                fflush(stdout);
            }
        } else if (cp < cp_end)
            *cp++ = c;
    } while ((c != '\r') && (c != '\n'));
    // parse buffer
    cp = cmd_buffer;
    bool not_last = true;
    for (argc = 0; not_last && (argc < MAX_ARGS); argc++) {
        while ((*cp == ' ') || (*cp == ','))
            cp++; // skip blanks
        if ((*cp == '\r') || (*cp == '\n'))
            break;
        argv[argc] = cp; // start of string
        while ((*cp != ' ') && (*cp != ',') && (*cp != '\r') && (*cp != '\n'))
            cp++; // skip non blank
        if ((*cp == '\r') || (*cp == '\n'))
            not_last = false;
        *cp++ = 0; // terminate string
    }
    argv[argc] = NULL;
}

char* full_path(const char* name) {
    if (name == NULL)
        return NULL;
    if (name[0] == '/') {
        strcpy(path, name);
    } else if (curdir[0] == 0) {
        strcpy(path, "/");
        strcat(path, name);
    } else {
        strcpy(path, curdir);
        if (name[0]) {
            strcat(path, "/");
            strcat(path, name);
        }
    }
    return path;
}

static void xmodem_cb(uint8_t* buf, uint32_t len) {
    if (lfs_file_write(&file, buf, len) != len)
        printf("error writing file\n");
}

static bool check_mount(bool need) {
    if (mounted == need)
        return false;
    sprintf(result, "filesystem is %s mounted", (need ? "not" : "already"));
    return true;
}

static bool check_name(void) {
    if (argc > 1)
        return false;
    strcpy(result, "missing file or directory name");
    return true;
}

static void put_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (lfs_file_open(&file, full_path(argv[1]), LFS_O_WRONLY | LFS_O_CREAT) < 0) {
        strcpy(result, "Can't create file");
        return;
    }
    stdio_set_translate_crlf(&stdio_uart, false);
    xmodemReceive(xmodem_cb);
    stdio_set_translate_crlf(&stdio_uart, true);
    int pos = lfs_file_seek(&file, 0, LFS_SEEK_END);
    lfs_file_close(&file);
    sprintf(result, "\nfile transfered, size: %d\n", pos);
}

int check_cp_parms(char** from, char** to, int copy) {
    *from = NULL;
    *to = NULL;
    int rc = 1;
    do {
        if (argc < 3) {
            strcpy(result, "need two names");
            break;
        }
        *from = strdup(full_path(argv[1]));
        if (*from == NULL) {
            strcpy(result, "no memory");
            break;
        }
        if (copy) {
            struct lfs_info info;
            if (lfs_stat(*from, &info) < 0) {
                sprintf(result, "%s not found", *from);
                break;
            }
            if (info.type != LFS_TYPE_REG) {
                sprintf(result, "%s is a directory", *from);
                break;
            }
        }
        *to = strdup(full_path(argv[2]));
        if (*to == NULL) {
            strcpy(result, "no memory");
            break;
        }
        struct lfs_info info;
        if (lfs_stat(*from, &info) < 0) {
            sprintf(result, "%s not found", *from);
            break;
        }
        if (lfs_stat(*to, &info) >= 0) {
            sprintf(result, "%s already exists", *to);
            break;
        }
        rc = 0;
    } while (0);
    if (rc) {
        if (*from)
            free(*from);
        if (*to)
            free(*to);
    }
    return rc;
}

static void mv_cmd(void) {
    char* from;
    char* to;
    if (check_cp_parms(&from, &to, 0))
        return;
    if (lfs_rename(from, to) < 0)
        sprintf(result, "could not rename %s to %s", from, to);
    else
        sprintf(result, "%s renamed to %s", from, to);
    free(from);
    free(to);
}

static void cp_cmd(void) {
    char* from;
    char* to;
    char* buf = NULL;
    if (check_cp_parms(&from, &to, 1))
        return;
    result[0] = 0;
    lfs_file_t in, out;
    do {
        buf = malloc(4096);
        if (buf == NULL) {
            strcpy(result, "no memory");
            break;
        }
        if (lfs_file_open(&in, from, LFS_O_RDONLY) < 0) {
            sprintf(result, "error opening %s", from);
            break;
        }
        if (lfs_file_open(&out, to, LFS_O_WRONLY | LFS_O_CREAT) < 0) {
            sprintf(result, "error opening %s", from);
            break;
        }
        int l = lfs_file_read(&in, buf, 4096);
        while (l > 0) {
            if (lfs_file_write(&out, buf, l) != l) {
                sprintf(result, "error writing %s", to);
                break;
            }
            l = lfs_file_read(&in, buf, 4096);
        }
    } while (false);
    lfs_file_close(&in);
    lfs_file_close(&out);
    if (buf)
        free(buf);
    if (!result[0])
        sprintf(result, "file %s copied to %s", from, to);
    free(from);
    free(to);
}

static void get_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (lfs_file_open(&file, full_path(argv[1]), LFS_O_RDONLY) < 0) {
        strcpy(result, "Can't open file");
        return;
    }
    uint32_t len = lfs_file_seek(&file, 0, LFS_SEEK_END);
    lfs_file_rewind(&file);
    char* buf = malloc(len);
    if (buf == NULL) {
        strcpy(result, "not enough memory");
        goto err2;
    }
    if (lfs_file_read(&file, buf, len) != len) {
        strcpy(result, "error reading file");
        goto err1;
    }
    stdio_set_translate_crlf(&stdio_uart, false);
    xmodemTransmit(buf, len);
    stdio_set_translate_crlf(&stdio_uart, true);
    printf("\nfile transfered, size: %d\n", len);
err1:
    free(buf);
err2:
    lfs_file_close(&file);
    strcpy(result, "transfered");
}

static void mkdir_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    if (lfs_mkdir(full_path(argv[1])) < 0) {
        strcpy(result, "Can't create directory");
        return;
    }
    sprintf(result, "%s created", full_path(argv[1]));
}

static void rm_cmd(void) {
    if (check_mount(true))
        return;
    if (check_name())
        return;
    // lfs won't remove a non empty directory but returns without error!
    struct lfs_info info;
    char* fp = full_path(argv[1]);
    if (lfs_stat(fp, &info) < 0) {
        sprintf(result, "%s not found", full_path(argv[1]));
        return;
    }
    int isdir = 0;
    if (info.type == LFS_TYPE_DIR) {
        isdir = 1;
        lfs_dir_t dir;
        lfs_dir_open(&dir, fp);
        int n = 0;
        while (lfs_dir_read(&dir, &info))
            if ((strcmp(info.name, ".") != 0) && (strcmp(info.name, "..") != 0))
                n++;
        lfs_dir_close(&dir);
        if (n) {
            sprintf(result, "directory %s not empty", fp);
            return;
        }
    }
    if (lfs_remove(fp) < 0)
        strcpy(result, "Can't remove file or directory");
    sprintf(result, "%s %s removed", isdir ? "directory" : "file", fp);
}

static void mount_cmd(void) {
    if (check_mount(false))
        return;
    if (lfs_mount() != LFS_ERR_OK) {
        strcpy(result, "Error mounting filesystem");
        return;
    }
    mounted = true;
    strcpy(result, "mounted");
}

static void unmount_cmd(void) {
    if (check_mount(true))
        return;
    if (lfs_unmount() != LFS_ERR_OK) {
        strcpy(result, "Error unmounting filesystem");
        return;
    }
    mounted = false;
    strcpy(result, "mounted");
}

static void format_cmd(void) {
    if (check_mount(false))
        return;
    printf("are you sure (y/N) ? ");
    fflush(stdout);
    parse_cmd();
    if ((argc == 0) || ((argv[0][0] | ' ') != 'y')) {
        strcpy(result, "user cancelled");
        return;
    }
    if (lfs_format() != LFS_ERR_OK)
        strcpy(result, "Error formating filesystem");
    strcpy(result, "formatted");
}

static void status_cmd(void) {
    if (check_mount(true))
        return;
    struct lfs_fsstat_t stat;
    lfs_stat(&stat);
    const char percent = 37;
    sprintf(result,
            "\nflash base 0x%x, blocks %d, block size %d, used %d, total %u bytes, %1.1f%c used.\n",
            lfs_flash_base(), (int)stat.block_count, (int)stat.block_size, (int)stat.blocks_used,
            stat.block_count * stat.block_size, stat.blocks_used * 100.0 / stat.block_count,
            percent);
}

static void ls_cmd(void) {
    if (check_mount(true))
        return;
    if (argc > 1)
        full_path(argv[1]);
    else
        full_path("");
    lfs_dir_t dir;
    if (lfs_dir_open(&dir, path) < 0) {
        strcpy(result, "not a directory");
        return;
    }
    printf("\n");
    struct lfs_info info;
    while (lfs_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_DIR)
                printf(" %7d [%s]\n", info.size, info.name);
    lfs_dir_rewind(&dir);
    while (lfs_dir_read(&dir, &info) > 0)
        if (strcmp(info.name, ".") && strcmp(info.name, ".."))
            if (info.type == LFS_TYPE_REG)
                printf(" %7d %s\n", info.size, info.name);
    lfs_dir_close(&dir);
    result[0] = 0;
}

static void cd_cmd(void) {
    if (check_mount(true))
        return;
    if (argc < 2) {
        curdir[0] = 0;
        return;
    }
    if (strcmp(argv[1], "..") == 0) {
        if (curdir[0] == 0) {
            strcpy(result, "not a directory");
            return;
        }
        int i;
        for (i = strlen(curdir) - 1; i >= 0; i--)
            if (curdir[i] == '/')
                break;
        if (i < 0)
            i = 0;
        curdir[i] = 0;
        return;
    }
    full_path(argv[1]);
    lfs_dir_t dir;
    if (lfs_dir_open(&dir, path) < 0) {
        strcpy(result, "not a directory");
        return;
    }
    lfs_dir_close(&dir);
    strcpy(curdir, path);
    sprintf(result, "changed to %s", curdir);
}

static void vi_cmd(void) {
    if (check_mount(true))
        return;
    vi(screen_x, screen_y, argc - 1, argv + 1);
    strcpy(result, VT_CLEAR "\n");
}

static void quit_cmd(void) {
    // release any resources we were using
    if (mounted)
        lfs_unmount();
    strcpy(result, "");
    run = false;
}

typedef struct {
    const char* name;
    cmd_func_t func;
    const char* descr;
} cmd_t;

// clang-format off
static cmd_t cmd_table[] = {
	{"cd", 		cd_cmd, 		"change directory"},
    {"cp",      cp_cmd,         "copy file"},
    {"format", 	format_cmd, 	"format the filesystem"},
    {"get", 	get_cmd, 		"get file (xmodem)"},
    {"ls", 		ls_cmd, 		"list directory"},
    {"mkdir", 	mkdir_cmd, 		"create directory"},
    {"mount", 	mount_cmd, 		"mount filesystem"},
    {"mv",      mv_cmd,         "rename file or directory"},
    {"put", 	put_cmd, 		"put file (xmodem)"},
    {"q", 		quit_cmd, 		"quit"},
    {"rm", 		rm_cmd, 		"remove file or directory"},
    {"status", 	status_cmd,		"filesystem status"},
    {"unmount",	unmount_cmd,	"unmount filesystem"},
    {"vi", 		vi_cmd, 		"vi editor"}
};
// clang-format on

static const char* search_cmds(int len) {
    if (len == 0)
        return NULL;
    int i, last_i, count = 0;
    for (i = 0; i < sizeof cmd_table / sizeof cmd_table[0]; i++)
        if (strncmp(cmd_buffer, cmd_table[i].name, len) == 0) {
            last_i = i;
            count++;
        }
    if (count != 1)
        return NULL;
    return cmd_table[last_i].name + len;
}

static bool stdio_init(int uart_rx_pin) {
    gpio_init(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 1, 0);
    sleep_ms(1);
    bool v1 = gpio_get(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 0, 1);
    sleep_ms(1);
    bool v2 = gpio_get(uart_rx_pin);
    gpio_set_pulls(uart_rx_pin, 0, 0);
    if (v1 != v2) {
        stdio_usb_init();
        while (!tud_cdc_connected())
            sleep_ms(1000);
        return false;
    } else {
        stdio_uart_init();
        getchar_timeout_us(1000);
    }
    return true;
}

static bool screen_size(void) {
    int rc = false;
    do {
        stdio_set_translate_crlf(&stdio_uart, false);
        printf(VT_ESC "[999;999H" VT_ESC "[6n");
        fflush(stdout);
        int k = getchar_timeout_us(100000);
        if (k == PICO_ERROR_TIMEOUT)
            break;
        char* cp = cmd_buffer;
        while (cp < cmd_buffer + sizeof cmd_buffer) {
            k = getchar_timeout_us(100000);
            if (k == PICO_ERROR_TIMEOUT)
                break;
            *cp++ = k;
        }
        stdio_set_translate_crlf(&stdio_uart, true);
        if (cmd_buffer[0] != '[')
            break;
        *cp = 0;
        if (cp - cmd_buffer < 5)
            break;
        char* end;
        uint32_t row, col;
        if (!isdigit(cmd_buffer[1]))
            break;
        errno = 0;
        row = strtoul(cmd_buffer + 1, &end, 10);
        if (errno)
            break;
        if (*end != ';' || !isdigit(end[1]))
            break;
        col = strtoul(end + 1, &end, 10);
        if (errno)
            break;
        if (*end != 'R')
            break;
        if (row < 1 || col < 1 || (row | col) > 0x7fff)
            break;
        screen_x = col;
        screen_y = row;
        rc = true;
    } while (false);
    return rc;
}
