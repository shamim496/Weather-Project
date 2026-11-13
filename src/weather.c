/*
 * weather.c
 * ---------------------------------------------------------------
 * Current weather storage.
 *
 * File format (data/weather.txt):
 *   location_id|temperature|humidity|wind_speed|wind_dir|rain_chance|
 *   rainfall_mm|pressure|condition|updated|source
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "weather.h"
#include "location.h"
#include "forecast.h"
#include "utils.h"

static Weather g_weather[MAX_LOCATIONS];
static int     g_count = 0;

static const char *CONDITION_NAMES[COND_COUNT] = {
    "Clear Sky", "Partly Cloudy", "Cloudy", "Fog / Mist",
    "Light Rain", "Heavy Rain", "Thunderstorm", "Snow"
};

/* Small ASCII pictures, 5 lines x 13 characters each */
static const char *CONDITION_ICONS[COND_COUNT][5] = {
    { "    \\   /    ", "     .-.     ", "  - (   ) -  ", "     `-'     ", "    /   \\    " },
    { "   \\  /      ", " _ /\"\".-.    ", "   \\_(   ).  ", "   /(___(__) ", "             " },
    { "             ", "     .--.    ", "  .-(    ).  ", " (___.__)__) ", "             " },
    { "             ", " _ - _ - _ - ", "  _ - _ - _  ", " _ - _ - _ - ", "             " },
    { "     .-.     ", "    (   ).   ", "   (___(__)  ", "    ' ' ' '  ", "   ' ' ' '   " },
    { "     .-.     ", "    (   ).   ", "   (___(__)  ", "  ,',',','   ", "  ,',',','   " },
    { "     .-.     ", "    (   ).   ", "   (___(__)  ", "   _/ ,' _/  ", "   /  ,' /   " },
    { "     .-.     ", "    (   ).   ", "   (___(__)  ", "    *  *  *  ", "   *  *  *   " }
};

/* =============================================================
 *  STORAGE
 * ============================================================= */

static int parse_weather(char *line, Weather *w)
{
    char *f[12];
    int n = split_fields(line, FIELD_SEP, f, 12);
    if (n < 11) return 0;

    memset(w, 0, sizeof *w);
    w->location_id = atoi(f[0]);
    w->temperature = atof(f[1]);
    w->humidity    = clampi(atoi(f[2]), 0, 100);
    w->wind_speed  = atof(f[3]);
    w->wind_dir    = clampi(atoi(f[4]), 0, 359);
    w->rain_chance = clampi(atoi(f[5]), 0, 100);
    w->rainfall_mm = atof(f[6]);
    w->pressure    = atof(f[7]);
    w->condition   = (Condition)clampi(atoi(f[8]), 0, COND_COUNT - 1);
    str_copy(w->updated, f[9], sizeof w->updated);
    w->source      = (f[10][0] == SOURCE_MANUAL) ? SOURCE_MANUAL : SOURCE_AUTO;
    return w->location_id > 0;
}

int weather_load(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    g_count = 0;
    data_path(FILE_WEATHER, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;

    while (fgets(line, sizeof line, fp) != NULL && g_count < MAX_LOCATIONS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_weather(line, &g_weather[g_count])) g_count++;
    }
    fclose(fp);
    return g_count;
}

int weather_save(void)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    FILE *fp;
    int i;

    data_path(FILE_WEATHER, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) return -1;

    fprintf(fp, "# location_id|temperature|humidity|wind_speed|wind_dir|rain_chance|"
                "rainfall_mm|pressure|condition|updated|source(S=auto,M=manual)\n");
    for (i = 0; i < g_count; i++) {
        const Weather *w = &g_weather[i];
        fprintf(fp, "%d|%.1f|%d|%.1f|%d|%d|%.1f|%.1f|%d|%s|%c\n",
                w->location_id, w->temperature, w->humidity, w->wind_speed,
                w->wind_dir, w->rain_chance, w->rainfall_mm, w->pressure,
                (int)w->condition, w->updated, w->source);
    }
    fclose(fp);
    return replace_file(tmp, path);
}

/* =============================================================
 *  ACCESS
 * ============================================================= */

int weather_count(void) { return g_count; }

Weather *weather_at(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return &g_weather[index];
}

Weather *weather_get(int location_id)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_weather[i].location_id == location_id) return &g_weather[i];
    return NULL;
}

int weather_set(const Weather *w)
{
    Weather *existing = weather_get(w->location_id);
    if (existing != NULL) {
        *existing = *w;
    } else {
        if (g_count >= MAX_LOCATIONS) return -1;
        g_weather[g_count++] = *w;
    }
    return weather_save();
}

int weather_remove(int location_id)
{
    int i;
    for (i = 0; i < g_count; i++) {
        if (g_weather[i].location_id == location_id) {
            for (; i < g_count - 1; i++) g_weather[i] = g_weather[i + 1];
            g_count--;
            return weather_save();
        }
    }
    return -1;
}

/* =============================================================
 *  AUTOMATIC FEED
 * ============================================================= */

long weather_age_minutes(const Weather *w, time_t now)
{
    time_t t = parse_time(w->updated);
    if (t == (time_t)-1) return 999999L;
    return (long)(difftime(now, t) / 60.0);
}

/*
 * Keep the data fresh: locations without a record, or with an old
 * record, receive a new reading from the automatic weather feed.
 * force = 1 refreshes every location (admin "Refresh now").
 * Manual readings entered by the admin are respected for 6 hours.
 */
int weather_sync(time_t now, int force)
{
    int i, refreshed = 0;

    for (i = 0; i < location_count(); i++) {
        const Location *loc = location_at(i);
        Weather *w = weather_get(loc->id);
        int need = 0;

        if (w == NULL || force) {
            need = 1;
        } else {
            long age = weather_age_minutes(w, now);
            if (w->source == SOURCE_AUTO && age >= AUTO_REFRESH_MINUTES) need = 1;
            if (w->source == SOURCE_MANUAL && age >= MANUAL_KEEP_MINUTES) need = 1;
        }
        if (need) {
            Weather fresh;
            forecast_simulate_current(loc, now, &fresh);
            if (w != NULL) {
                *w = fresh;
            } else if (g_count < MAX_LOCATIONS) {
                g_weather[g_count++] = fresh;
            }
            refreshed++;
        }
    }
    /* remove records that belong to deleted locations */
    for (i = 0; i < g_count; i++) {
        if (location_find(g_weather[i].location_id) == NULL) {
            int j;
            for (j = i; j < g_count - 1; j++) g_weather[j] = g_weather[j + 1];
            g_count--;
            i--;
            refreshed++;
        }
    }
    if (refreshed > 0) weather_save();
    return refreshed;
}

/* =============================================================
 *  HELPERS
 * ============================================================= */

/* Decide the weather condition from the measured values. */
Condition condition_derive(double temp, int humidity, int rain_chance,
                           double wind, double pressure)
{
    if (rain_chance >= 75 && (wind >= 35.0 || pressure <= 1000.0)) return COND_THUNDERSTORM;
    if (rain_chance >= 50 && temp <= 1.0)  return COND_SNOW;
    if (rain_chance >= 75)                 return COND_HEAVY_RAIN;
    if (rain_chance >= 50)                 return COND_LIGHT_RAIN;
    if (humidity >= 95 && wind < 10.0)     return COND_FOG;
    if (rain_chance >= 30)                 return COND_CLOUDY;
    if (rain_chance >= 15)                 return COND_PARTLY_CLOUDY;
    return COND_CLEAR;
}

const char *condition_name(Condition c)
{
    if (c < 0 || c >= COND_COUNT) return "Unknown";
    return CONDITION_NAMES[c];
}

const char *condition_icon_line(Condition c, int line)
{
    if (c < 0 || c >= COND_COUNT || line < 0 || line > 4) return "             ";
    return CONDITION_ICONS[c][line];
}

/* 16 point compass: 0 -> N, 45 -> NE, 90 -> E ... */
const char *wind_direction_name(int degrees)
{
    static const char *NAMES[16] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int index;
    degrees %= 360;
    if (degrees < 0) degrees += 360;
    index = (int)((degrees + 11.25) / 22.5) % 16;
    return NAMES[index];
}
