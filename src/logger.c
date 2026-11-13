/*
 * logger.c
 * ---------------------------------------------------------------
 * Appends one line per event to data/system.log, for example:
 *   2026-11-13 10:20:11 | LOGIN | user 'rahim' logged in
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "logger.h"
#include "utils.h"

void log_event(const char *type, const char *fmt, ...)
{
    char path[PATH_LEN];
    char when[TIME_LEN];
    char message[LINE_LEN];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    sanitize_field(message);

    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "a");
    if (fp == NULL) return;            /* logging must never stop the program */
    now_string(when, sizeof when);
    fprintf(fp, "%s | %-10s | %s\n", when, type, message);
    fclose(fp);
}

/*
 * Read the last max_lines lines of the log. A small ring buffer is
 * used so the whole file never has to be kept in memory.
 */
int log_tail(char lines[][LINE_LEN], int max_lines)
{
    char path[PATH_LEN];
    char buf[LINE_LEN];
    FILE *fp;
    int total = 0, i, start, count;
    static char ring[64][LINE_LEN];

    if (max_lines > 64) max_lines = 64;
    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;

    while (fgets(buf, sizeof buf, fp) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
        str_copy(ring[total % max_lines], buf, LINE_LEN);
        total++;
    }
    fclose(fp);

    count = total < max_lines ? total : max_lines;
    start = total < max_lines ? 0 : total % max_lines;
    for (i = 0; i < count; i++)
        str_copy(lines[i], ring[(start + i) % max_lines], LINE_LEN);
    return count;
}

/* Count how many log lines have the given event type. */
long log_count_type(const char *type)
{
    char path[PATH_LEN];
    char buf[LINE_LEN];
    char pattern[64];
    FILE *fp;
    long count = 0;

    snprintf(pattern, sizeof pattern, "| %-10s |", type);
    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(buf, sizeof buf, fp) != NULL)
        if (strstr(buf, pattern) != NULL) count++;
    fclose(fp);
    return count;
}
