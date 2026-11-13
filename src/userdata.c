/*
 * userdata.c
 * ---------------------------------------------------------------
 * Favourite locations and search history. Both files are simple
 * append/rewrite text files, processed line by line, so they can
 * grow large without needing big arrays in memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userdata.h"
#include "utils.h"

/* =============================================================
 *  GENERIC FILE FILTER
 * ============================================================= */

/* Callback: return 1 to keep a line, 0 to delete it. */
typedef int (*KeepFn)(char **fields, int n, const void *ctx);

typedef struct {
    const char *username;
    int location_id;
} Match;

/* Rewrite a data file keeping only the lines accepted by keep(). */
static int filter_file(const char *file, KeepFn keep, const void *ctx)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    char line[LINE_LEN], copy[LINE_LEN];
    char *f[4];
    FILE *in, *out;
    int removed = 0;

    data_path(file, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    in = fopen(path, "r");
    if (in == NULL) return 0;
    out = fopen(tmp, "w");
    if (out == NULL) { fclose(in); return -1; }

    while (fgets(line, sizeof line, in) != NULL) {
        int n;
        if (line[0] == '#') { fputs(line, out); continue; }
        str_copy(copy, line, sizeof copy);
        n = split_fields(copy, FIELD_SEP, f, 4);
        if (n >= 2 && !keep(f, n, ctx)) { removed++; continue; }
        fputs(line, out);
    }
    fclose(in);
    fclose(out);
    if (replace_file(tmp, path) != 0) return -1;
    return removed;
}

static int keep_not_user(char **f, int n, const void *ctx)
{
    const Match *m = (const Match *)ctx;
    (void)n;
    return !str_iequals(f[0], m->username);
}

static int keep_not_location(char **f, int n, const void *ctx)
{
    const Match *m = (const Match *)ctx;
    (void)n;
    return atoi(f[1]) != m->location_id;
}

static int keep_not_user_location(char **f, int n, const void *ctx)
{
    const Match *m = (const Match *)ctx;
    (void)n;
    return !(str_iequals(f[0], m->username) && atoi(f[1]) == m->location_id);
}

static long count_lines(const char *file, const char *username)
{
    char path[PATH_LEN], line[LINE_LEN];
    char *f[4];
    FILE *fp;
    long count = 0;

    data_path(file, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (split_fields(line, FIELD_SEP, f, 4) < 2) continue;
        if (username == NULL || str_iequals(f[0], username)) count++;
    }
    fclose(fp);
    return count;
}

/* =============================================================
 *  FAVOURITES
 * ============================================================= */

int favorites_list(const char *username, int ids[], int max)
{
    char path[PATH_LEN], line[LINE_LEN];
    char *f[4];
    FILE *fp;
    int n = 0;

    data_path(FILE_FAVORITES, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL && n < max) {
        if (line[0] == '#') continue;
        if (split_fields(line, FIELD_SEP, f, 4) >= 2 && str_iequals(f[0], username))
            ids[n++] = atoi(f[1]);
    }
    fclose(fp);
    return n;
}

int favorites_is(const char *username, int location_id)
{
    int ids[MAX_FAVORITES], n, i;
    n = favorites_list(username, ids, MAX_FAVORITES);
    for (i = 0; i < n; i++)
        if (ids[i] == location_id) return 1;
    return 0;
}

int favorites_add(const char *username, int location_id)
{
    char path[PATH_LEN];
    int ids[MAX_FAVORITES];
    FILE *fp;

    if (favorites_is(username, location_id)) return 1;
    if (favorites_list(username, ids, MAX_FAVORITES) >= MAX_FAVORITES) return -1;

    data_path(FILE_FAVORITES, path, sizeof path);
    fp = fopen(path, "a");
    if (fp == NULL) return -2;
    fprintf(fp, "%s|%d\n", username, location_id);
    fclose(fp);
    return 0;
}

int favorites_remove(const char *username, int location_id)
{
    Match m;
    m.username = username;
    m.location_id = location_id;
    return filter_file(FILE_FAVORITES, keep_not_user_location, &m);
}

int favorites_remove_user(const char *username)
{
    Match m;
    m.username = username;
    m.location_id = 0;
    return filter_file(FILE_FAVORITES, keep_not_user, &m);
}

int favorites_remove_location(int location_id)
{
    Match m;
    m.username = "";
    m.location_id = location_id;
    return filter_file(FILE_FAVORITES, keep_not_location, &m);
}

long favorites_total(void) { return count_lines(FILE_FAVORITES, NULL); }

/* =============================================================
 *  SEARCH HISTORY
 * ============================================================= */

int history_add(const char *username, int location_id)
{
    char path[PATH_LEN], when[TIME_LEN];
    FILE *fp;

    data_path(FILE_HISTORY, path, sizeof path);
    fp = fopen(path, "a");
    if (fp == NULL) return -1;
    now_string(when, sizeof when);
    fprintf(fp, "%s|%d|%s\n", username, location_id, when);
    fclose(fp);
    return 0;
}

/* The newest `max` entries of a user, newest first. */
int history_list(const char *username, HistoryEntry out[], int max)
{
    char path[PATH_LEN], line[LINE_LEN];
    char *f[4];
    FILE *fp;
    HistoryEntry ring[MAX_HISTORY_SHOW];
    int total = 0, count, i;

    if (max > MAX_HISTORY_SHOW) max = MAX_HISTORY_SHOW;
    if (max <= 0) return 0;
    data_path(FILE_HISTORY, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;

    while (fgets(line, sizeof line, fp) != NULL) {
        HistoryEntry *e;
        if (line[0] == '#') continue;
        if (split_fields(line, FIELD_SEP, f, 4) < 3) continue;
        if (!str_iequals(f[0], username)) continue;
        e = &ring[total % max];
        str_copy(e->username, f[0], sizeof e->username);
        e->location_id = atoi(f[1]);
        str_copy(e->time, f[2], sizeof e->time);
        total++;
    }
    fclose(fp);

    count = total < max ? total : max;
    for (i = 0; i < count; i++)          /* newest first */
        out[i] = ring[(total - 1 - i) % max];
    return count;
}

int history_clear(const char *username)
{
    Match m;
    m.username = username;
    m.location_id = 0;
    return filter_file(FILE_HISTORY, keep_not_user, &m);
}

int history_remove_location(int location_id)
{
    Match m;
    m.username = "";
    m.location_id = location_id;
    return filter_file(FILE_HISTORY, keep_not_location, &m);
}

long history_total(void) { return count_lines(FILE_HISTORY, NULL); }

long history_count_user(const char *username) { return count_lines(FILE_HISTORY, username); }

/* Count searches per location; ids/counts sorted by count (highest first). */
static int count_by_location(const char *username, int ids[], long counts[], int max)
{
    char path[PATH_LEN], line[LINE_LEN];
    char *f[4];
    FILE *fp;
    int all_ids[MAX_LOCATIONS];
    long all_counts[MAX_LOCATIONS];
    int distinct = 0, i, j;

    data_path(FILE_HISTORY, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL) {
        int id;
        if (line[0] == '#') continue;
        if (split_fields(line, FIELD_SEP, f, 4) < 2) continue;
        if (username != NULL && !str_iequals(f[0], username)) continue;
        id = atoi(f[1]);
        for (i = 0; i < distinct; i++)
            if (all_ids[i] == id) { all_counts[i]++; break; }
        if (i == distinct && distinct < MAX_LOCATIONS) {
            all_ids[distinct] = id;
            all_counts[distinct] = 1;
            distinct++;
        }
    }
    fclose(fp);

    /* sort by count, highest first (selection sort - small list) */
    for (i = 0; i < distinct - 1; i++) {
        int best = i;
        for (j = i + 1; j < distinct; j++)
            if (all_counts[j] > all_counts[best]) best = j;
        if (best != i) {
            int ti = all_ids[i];  long tc = all_counts[i];
            all_ids[i] = all_ids[best];   all_counts[i] = all_counts[best];
            all_ids[best] = ti;           all_counts[best] = tc;
        }
    }
    if (distinct > max) distinct = max;
    for (i = 0; i < distinct; i++) {
        ids[i] = all_ids[i];
        if (counts) counts[i] = all_counts[i];
    }
    return distinct;
}

int history_top_locations(int ids[], long counts[], int max)
{
    return count_by_location(NULL, ids, counts, max);
}

int history_most_searched_by(const char *username)
{
    int id;
    long c;
    if (count_by_location(username, &id, &c, 1) == 1) return id;
    return 0;
}
