/*
 * utils.c
 * ---------------------------------------------------------------
 * Implementation of the helper functions declared in utils.h.
 * All functions are written in portable C99 so that the project
 * compiles with GCC on Linux, macOS and Windows (MinGW).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>     /* _mkdir */
#endif

#include "utils.h"

static char g_data_dir[PATH_LEN]   = DEFAULT_DATA_DIR;
static char g_report_dir[PATH_LEN] = DEFAULT_REPORT_DIR;

/* =============================================================
 *  STRING HELPERS
 * ============================================================= */

/* Remove leading and trailing white space (in place). */
void str_trim(char *s)
{
    char *start;
    size_t len;

    if (s == NULL) return;

    start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

/* Safe string copy that always terminates the destination. */
void str_copy(char *dst, const char *src, size_t size)
{
    if (dst == NULL || size == 0) return;
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

void str_to_lower(char *dst, const char *src, size_t size)
{
    size_t i;
    if (size == 0) return;
    for (i = 0; src[i] != '\0' && i < size - 1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

int str_iequals(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Case-insensitive "does haystack contain needle?" */
int str_icontains(const char *haystack, const char *needle)
{
    char h[LINE_LEN], n[LINE_LEN];
    str_to_lower(h, haystack, sizeof h);
    str_to_lower(n, needle, sizeof n);
    return strstr(h, n) != NULL;
}

int str_is_number(const char *s)
{
    if (s == NULL || *s == '\0') return 0;
    if (*s == '-' || *s == '+') s++;
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Data files use '|' as separator, so it must never appear inside a field. */
void sanitize_field(char *s)
{
    for (; *s; s++) {
        if (*s == FIELD_SEP) *s = '/';
        else if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
    }
}

/*
 * Split a line into fields (in place). Unlike strtok() this keeps
 * empty fields, e.g. "a||c" gives three fields: "a", "", "c".
 * Returns the number of fields found.
 */
int split_fields(char *line, char sep, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;
    size_t len = strlen(line);

    /* strip the end-of-line characters */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';

    if (max_fields <= 0) return 0;
    fields[count++] = p;
    while (*p) {
        if (*p == sep) {
            *p = '\0';
            if (count >= max_fields) break;
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

/* Levenshtein edit distance, used for "Did you mean ...?" suggestions. */
int edit_distance(const char *a, const char *b)
{
    int prev[64], curr[64];
    int la = (int)strlen(a), lb = (int)strlen(b);
    int i, j;

    if (la > 63) la = 63;
    if (lb > 63) lb = 63;

    for (j = 0; j <= lb; j++) prev[j] = j;
    for (i = 1; i <= la; i++) {
        curr[0] = i;
        for (j = 1; j <= lb; j++) {
            int cost = (tolower((unsigned char)a[i - 1]) ==
                        tolower((unsigned char)b[j - 1])) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int best = del < ins ? del : ins;
            curr[j] = best < sub ? best : sub;
        }
        memcpy(prev, curr, sizeof(int) * (size_t)(lb + 1));
    }
    return prev[lb];
}

/* =============================================================
 *  TIME HELPERS
 * ============================================================= */

void format_time(time_t t, char *buf, size_t size)
{
    struct tm *tmv = localtime(&t);
    if (tmv == NULL) { str_copy(buf, "unknown", size); return; }
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tmv);
}

void format_time_short(time_t t, char *buf, size_t size)
{
    struct tm *tmv = localtime(&t);
    if (tmv == NULL) { str_copy(buf, "unknown", size); return; }
    strftime(buf, size, "%Y-%m-%d %H:%M", tmv);
}

/* Parse "YYYY-MM-DD HH:MM[:SS]" (local time). Returns -1 on error. */
time_t parse_time(const char *s)
{
    struct tm tmv;
    int y, mo, d, h = 0, mi = 0, se = 0;
    int n;

    if (s == NULL) return (time_t)-1;
    n = sscanf(s, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &se);
    if (n < 3) return (time_t)-1;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59)
        return (time_t)-1;

    memset(&tmv, 0, sizeof tmv);
    tmv.tm_year  = y - 1900;
    tmv.tm_mon   = mo - 1;
    tmv.tm_mday  = d;
    tmv.tm_hour  = h;
    tmv.tm_min   = mi;
    tmv.tm_sec   = se;
    tmv.tm_isdst = -1;
    return mktime(&tmv);
}

void now_string(char *buf, size_t size)
{
    format_time(time(NULL), buf, size);
}

/* Offset of the computer's clock from UTC, in seconds (e.g. +21600 for UTC+6). */
long system_utc_offset_seconds(void)
{
    time_t now = time(NULL);
    struct tm lt = *localtime(&now);
    struct tm gt = *gmtime(&now);
    gt.tm_isdst = lt.tm_isdst;
    return (long)difftime(mktime(&lt), mktime(&gt));
}

/* =============================================================
 *  PATH & FILE HELPERS
 * ============================================================= */

void set_data_dir(const char *dir)   { str_copy(g_data_dir, dir, sizeof g_data_dir); }
const char *get_data_dir(void)       { return g_data_dir; }
void set_report_dir(const char *dir) { str_copy(g_report_dir, dir, sizeof g_report_dir); }
const char *get_report_dir(void)     { return g_report_dir; }

/* Build "<data_dir>/<file>" */
void data_path(const char *file, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", g_data_dir, file);
}

/* Create a directory if it does not exist. Returns 0 on success. */
int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return 0;
#ifdef _WIN32
    return _mkdir(path) == 0 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 ? 0 : -1;
#endif
}

int file_exists(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

long file_size(const char *path)
{
    long size;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    return size;
}

/*
 * Replace final_path with tmp_path. On Windows rename() fails when
 * the destination exists, so the old file is removed first.
 */
int replace_file(const char *tmp_path, const char *final_path)
{
    remove(final_path);
    return rename(tmp_path, final_path) == 0 ? 0 : -1;
}

/* =============================================================
 *  PASSWORD HASHING
 * ============================================================= */

/*
 * Salted FNV-1a (64 bit) hash, repeated 1000 times.
 * The username is used as salt so two users with the same password
 * get different hashes. Plain text passwords are never stored.
 * (Good enough for a lab project - not a replacement for bcrypt.)
 */
void hash_password(const char *username, const char *password, char out[HASH_LEN])
{
    unsigned long long h = 1469598103934665603ULL;   /* FNV offset basis */
    const unsigned long long prime = 1099511628211ULL;
    char salted[LINE_LEN];
    char lower_user[USERNAME_LEN];
    int round;
    const char *p;

    str_to_lower(lower_user, username, sizeof lower_user);
    snprintf(salted, sizeof salted, "%s:%s:WFS", lower_user, password);

    for (round = 0; round < 1000; round++) {
        for (p = salted; *p; p++) {
            h ^= (unsigned char)*p;
            h *= prime;
        }
        h ^= (unsigned long long)round;
        h *= prime;
    }
    /* print as two 32-bit halves - works with every C runtime */
    snprintf(out, HASH_LEN, "%08lx%08lx",
             (unsigned long)((h >> 32) & 0xFFFFFFFFUL),
             (unsigned long)(h & 0xFFFFFFFFUL));
}

/* =============================================================
 *  DETERMINISTIC NOISE (same input -> same output on every PC)
 * ============================================================= */

uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t make_seed(int a, int b, int c)
{
    uint32_t h = hash32((uint32_t)a * 73856093U);
    h = hash32(h ^ ((uint32_t)b * 19349663U));
    h = hash32(h ^ ((uint32_t)c * 83492791U));
    return h;
}

double noise_unit(uint32_t seed)
{
    return (double)(hash32(seed) & 0xFFFFFFU) / 16777216.0;
}

double noise_signed(uint32_t seed)
{
    return noise_unit(seed) * 2.0 - 1.0;
}

/* =============================================================
 *  MATHS
 * ============================================================= */

double clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
