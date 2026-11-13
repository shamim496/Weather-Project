/*
 * location.c
 * ---------------------------------------------------------------
 * Location storage and searching.
 *
 * File format (data/locations.txt), one location per line:
 *   id|name|region|lat|lon|utc_offset|avg_temp|season_range|
 *   daily_range|avg_humidity|rain_tendency
 * Lines starting with '#' are comments.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "location.h"
#include "utils.h"

static Location g_locations[MAX_LOCATIONS];
static int      g_count = 0;

/* Built-in locations written to the data file on first run. */
static const Location DEFAULT_LOCATIONS[] = {
    {  1, "Dhaka",       "Dhaka Division, Bangladesh",      23.8103,  90.4125,  6.0, 26.0, 10.0,  8.0, 75, 60 },
    {  2, "Chattogram",  "Chattogram Division, Bangladesh", 22.3569,  91.7832,  6.0, 25.9,  9.0,  7.0, 78, 70 },
    {  3, "Bogura",      "Rajshahi Division, Bangladesh",   24.8465,  89.3773,  6.0, 25.4, 12.0, 10.0, 73, 50 },
    {  4, "Rajshahi",    "Rajshahi Division, Bangladesh",   24.3745,  88.6042,  6.0, 25.6, 13.0, 11.0, 70, 45 },
    {  5, "Khulna",      "Khulna Division, Bangladesh",     22.8456,  89.5403,  6.0, 26.2, 11.0,  8.0, 78, 55 },
    {  6, "Sylhet",      "Sylhet Division, Bangladesh",     24.8949,  91.8687,  6.0, 24.9, 10.0,  8.0, 80, 80 },
    {  7, "Barishal",    "Barishal Division, Bangladesh",   22.7010,  90.3535,  6.0, 26.0, 10.0,  7.0, 80, 65 },
    {  8, "Rangpur",     "Rangpur Division, Bangladesh",    25.7439,  89.2752,  6.0, 24.6, 12.0, 10.0, 76, 55 },
    {  9, "Mymensingh",  "Mymensingh Division, Bangladesh", 24.7471,  90.4203,  6.0, 25.3, 11.0,  9.0, 77, 60 },
    { 10, "Cox's Bazar", "Chattogram Division, Bangladesh", 21.4272,  92.0058,  6.0, 26.1,  8.0,  6.0, 80, 75 },
    { 11, "Cumilla",     "Chattogram Division, Bangladesh", 23.4607,  91.1809,  6.0, 25.8, 10.0,  8.0, 77, 60 },
    { 12, "Dinajpur",    "Rangpur Division, Bangladesh",    25.6217,  88.6355,  6.0, 24.8, 13.0, 11.0, 74, 50 },
    { 13, "Kolkata",     "West Bengal, India",              22.5726,  88.3639,  5.5, 26.8, 11.0,  8.0, 74, 55 },
    { 14, "New Delhi",   "Delhi, India",                    28.6139,  77.2090,  5.5, 25.0, 19.0, 12.0, 60, 30 },
    { 15, "London",      "England, United Kingdom",         51.5074,  -0.1278,  0.0, 11.3, 13.0,  7.0, 78, 50 },
    { 16, "New York",    "New York, USA",                   40.7128, -74.0060, -5.0, 12.9, 25.0,  8.0, 64, 45 },
    { 17, "Tokyo",       "Kanto, Japan",                    35.6762, 139.6503,  9.0, 15.8, 21.0,  7.0, 65, 50 },
    { 18, "Dubai",       "Dubai, UAE",                      25.2048,  55.2708,  4.0, 28.0, 17.0, 10.0, 55,  5 },
    { 19, "Sydney",      "New South Wales, Australia",     -33.8688, 151.2093, 10.0, 18.0, 10.0,  7.0, 68, 40 },
    { 20, "Singapore",   "Singapore",                        1.3521, 103.8198,  8.0, 27.6,  2.0,  6.0, 84, 75 },
    { 21, "Moscow",      "Central Russia, Russia",          55.7558,  37.6173,  3.0,  5.8, 27.0,  7.0, 77, 45 }
};

/* =============================================================
 *  STORAGE
 * ============================================================= */

static int parse_location(char *line, Location *loc)
{
    char *f[12];
    int n = split_fields(line, FIELD_SEP, f, 12);
    if (n < 11) return 0;

    memset(loc, 0, sizeof *loc);
    loc->id            = atoi(f[0]);
    str_copy(loc->name,   f[1], sizeof loc->name);
    str_copy(loc->region, f[2], sizeof loc->region);
    loc->latitude      = atof(f[3]);
    loc->longitude     = atof(f[4]);
    loc->utc_offset    = atof(f[5]);
    loc->avg_temp      = atof(f[6]);
    loc->season_range  = atof(f[7]);
    loc->daily_range   = atof(f[8]);
    loc->avg_humidity  = atoi(f[9]);
    loc->rain_tendency = atoi(f[10]);
    return loc->id > 0 && loc->name[0] != '\0';
}

int location_load(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    g_count = 0;
    data_path(FILE_LOCATIONS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) {
        location_seed_defaults();
        return g_count;
    }
    while (fgets(line, sizeof line, fp) != NULL && g_count < MAX_LOCATIONS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_location(line, &g_locations[g_count])) g_count++;
    }
    fclose(fp);
    return g_count;
}

int location_save(void)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    FILE *fp;
    int i;

    data_path(FILE_LOCATIONS, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) return -1;

    fprintf(fp, "# id|name|region|latitude|longitude|utc_offset|avg_temp|"
                "season_range|daily_range|avg_humidity|rain_tendency\n");
    for (i = 0; i < g_count; i++) {
        const Location *l = &g_locations[i];
        fprintf(fp, "%d|%s|%s|%.4f|%.4f|%.1f|%.1f|%.1f|%.1f|%d|%d\n",
                l->id, l->name, l->region, l->latitude, l->longitude,
                l->utc_offset, l->avg_temp, l->season_range, l->daily_range,
                l->avg_humidity, l->rain_tendency);
    }
    fclose(fp);
    return replace_file(tmp, path);
}

void location_seed_defaults(void)
{
    int i, n = (int)(sizeof DEFAULT_LOCATIONS / sizeof DEFAULT_LOCATIONS[0]);
    g_count = 0;
    for (i = 0; i < n && i < MAX_LOCATIONS; i++)
        g_locations[g_count++] = DEFAULT_LOCATIONS[i];
    location_save();
}

/* =============================================================
 *  ACCESS
 * ============================================================= */

int location_count(void) { return g_count; }

Location *location_at(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return &g_locations[index];
}

Location *location_find(int id)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_locations[i].id == id) return &g_locations[i];
    return NULL;
}

const char *location_name(int id)
{
    const Location *l = location_find(id);
    return l ? l->name : "(deleted location)";
}

/* =============================================================
 *  SEARCH
 * ============================================================= */

/*
 * Find locations whose name or region contains the query
 * (case-insensitive). An exact name match is always listed first.
 */
int location_search(const char *query, int ids[], int max)
{
    int i, found = 0;

    if (query == NULL || query[0] == '\0') return 0;

    for (i = 0; i < g_count && found < max; i++)
        if (str_iequals(g_locations[i].name, query))
            ids[found++] = g_locations[i].id;

    for (i = 0; i < g_count && found < max; i++) {
        if (str_iequals(g_locations[i].name, query)) continue;
        if (str_icontains(g_locations[i].name, query) ||
            str_icontains(g_locations[i].region, query))
            ids[found++] = g_locations[i].id;
    }
    return found;
}

/*
 * "Did you mean ...?" - returns locations whose name is close to the
 * query (small edit distance), best match first.
 */
int location_suggest(const char *query, int ids[], int max)
{
    int dist[MAX_LOCATIONS];
    int order[MAX_LOCATIONS];
    int i, j, n = 0;
    int limit = (int)strlen(query) / 3;
    if (limit < 2) limit = 2;

    for (i = 0; i < g_count; i++) {
        int d = edit_distance(query, g_locations[i].name);
        if (d <= limit) {
            dist[n] = d;
            order[n] = g_locations[i].id;
            n++;
        }
    }
    /* simple insertion sort by distance */
    for (i = 1; i < n; i++) {
        int dk = dist[i], ok = order[i];
        j = i - 1;
        while (j >= 0 && dist[j] > dk) {
            dist[j + 1] = dist[j];
            order[j + 1] = order[j];
            j--;
        }
        dist[j + 1] = dk;
        order[j + 1] = ok;
    }
    if (n > max) n = max;
    for (i = 0; i < n; i++) ids[i] = order[i];
    return n;
}

/* =============================================================
 *  ADMIN OPERATIONS
 * ============================================================= */

int location_name_exists(const char *name, int except_id)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_locations[i].id != except_id && str_iequals(g_locations[i].name, name))
            return 1;
    return 0;
}

int location_add(Location *loc)
{
    int i, max_id = 0;

    if (g_count >= MAX_LOCATIONS) return -1;
    for (i = 0; i < g_count; i++)
        if (g_locations[i].id > max_id) max_id = g_locations[i].id;

    loc->id = max_id + 1;
    sanitize_field(loc->name);
    sanitize_field(loc->region);
    g_locations[g_count++] = *loc;
    if (location_save() != 0) return -1;
    return loc->id;
}

int location_update(const Location *loc)
{
    Location *existing = location_find(loc->id);
    if (existing == NULL) return -1;
    *existing = *loc;
    sanitize_field(existing->name);
    sanitize_field(existing->region);
    return location_save();
}

int location_delete(int id)
{
    int i;
    for (i = 0; i < g_count; i++) {
        if (g_locations[i].id == id) {
            for (; i < g_count - 1; i++) g_locations[i] = g_locations[i + 1];
            g_count--;
            return location_save();
        }
    }
    return -1;
}

/* =============================================================
 *  GEOGRAPHY
 * ============================================================= */

/* Great-circle distance between two points on Earth (Haversine formula). */
double haversine_km(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0;                  /* Earth radius in km */
    double dlat = (lat2 - lat1) * PI / 180.0;
    double dlon = (lon2 - lon1) * PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) *
               sin(dlon / 2) * sin(dlon / 2);
    return 2.0 * R * atan2(sqrt(a), sqrt(1.0 - a));
}

/* Id of the location closest to the given coordinates (or -1). */
int location_nearest(double lat, double lon, double *distance_km)
{
    int i, best = -1;
    double best_d = 1e12;

    for (i = 0; i < g_count; i++) {
        double d = haversine_km(lat, lon, g_locations[i].latitude, g_locations[i].longitude);
        if (d < best_d) {
            best_d = d;
            best = g_locations[i].id;
        }
    }
    if (distance_km) *distance_km = best_d;
    return best;
}

/* Break a time down into the location's own local time. */
void location_local_tm(const Location *loc, time_t t, struct tm *out)
{
    time_t shifted = t + (time_t)(loc->utc_offset * 3600.0);
    struct tm *g = gmtime(&shifted);
    if (g != NULL) *out = *g;
    else memset(out, 0, sizeof *out);
}

void location_format_coords(const Location *loc, char *buf, size_t size)
{
    snprintf(buf, size, "%.2f%c, %.2f%c",
             fabs(loc->latitude),  loc->latitude  >= 0 ? 'N' : 'S',
             fabs(loc->longitude), loc->longitude >= 0 ? 'E' : 'W');
}
