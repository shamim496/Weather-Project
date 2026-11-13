/*
 * detect.c
 * ---------------------------------------------------------------
 * Automatic location detection (see detect.h for the order used).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "detect.h"
#include "location.h"
#include "utils.h"

const char *detect_method_name(DetectMethod m)
{
    switch (m) {
    case DETECT_ENVIRONMENT: return "GPS coordinates from environment (WFS_LAT/WFS_LON)";
    case DETECT_GPS_FILE:    return "Device GPS sensor (device_location.txt)";
    case DETECT_TIMEZONE:    return "Computer time zone";
    case DETECT_HOME:        return "Home location from your profile";
    default:                 return "Not detected";
    }
}

static int valid_coords(double lat, double lon)
{
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

static int read_env_coords(double *lat, double *lon)
{
    const char *a = getenv("WFS_LAT");
    const char *b = getenv("WFS_LON");
    char *end1, *end2;
    if (a == NULL || b == NULL) return 0;
    *lat = strtod(a, &end1);
    *lon = strtod(b, &end2);
    return *end1 == '\0' && *end2 == '\0' && valid_coords(*lat, *lon);
}

static int read_gps_file(double *lat, double *lon)
{
    char path[PATH_LEN], line[LINE_LEN];
    char *f[3];
    FILE *fp;
    int ok = 0;

    data_path(FILE_DEVICE_GPS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (split_fields(line, FIELD_SEP, f, 3) >= 2) {
            char *end1, *end2;
            *lat = strtod(f[0], &end1);
            *lon = strtod(f[1], &end2);
            ok = end1 != f[0] && end2 != f[1] && valid_coords(*lat, *lon);
        }
        break;
    }
    fclose(fp);
    return ok;
}

/* First location whose UTC offset equals the computer's offset. */
static int match_timezone(void)
{
    double offset = system_utc_offset_seconds() / 3600.0;
    int i;
    for (i = 0; i < location_count(); i++) {
        const Location *l = location_at(i);
        if (fabs(l->utc_offset - offset) < 0.01) return l->id;
    }
    return 0;
}

int detect_location(int home_location, DetectResult *out)
{
    double lat, lon, dist;
    int id;

    memset(out, 0, sizeof *out);
    out->distance_km = -1.0;
    if (location_count() == 0) return 0;

    if (read_env_coords(&lat, &lon))     out->method = DETECT_ENVIRONMENT;
    else if (read_gps_file(&lat, &lon))  out->method = DETECT_GPS_FILE;

    if (out->method != DETECT_NONE) {
        id = location_nearest(lat, lon, &dist);
        if (id > 0) {
            out->location_id = id;
            out->latitude = lat;
            out->longitude = lon;
            out->distance_km = dist;
            return id;
        }
    }

    id = match_timezone();
    if (id > 0) {
        out->method = DETECT_TIMEZONE;
        out->location_id = id;
        return id;
    }

    if (home_location > 0 && location_find(home_location) != NULL) {
        out->method = DETECT_HOME;
        out->location_id = home_location;
        return home_location;
    }
    return 0;
}

/* Save new device coordinates (used to simulate moving the device). */
int detect_save_device_coords(double lat, double lon)
{
    char path[PATH_LEN];
    FILE *fp;
    if (!valid_coords(lat, lon)) return -1;
    data_path(FILE_DEVICE_GPS, path, sizeof path);
    fp = fopen(path, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# latitude|longitude|source  (simulated GPS reading of this device)\n");
    fprintf(fp, "%.4f|%.4f|GPS\n", lat, lon);
    fclose(fp);
    return 0;
}
