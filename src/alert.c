/*
 * alert.c
 * ---------------------------------------------------------------
 * Manual alert storage + automatic alert detection.
 *
 * File format (data/alerts.txt):
 *   id|location_id|severity|type|message|issued|expires|active
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "alert.h"
#include "utils.h"

static Alert g_alerts[MAX_ALERTS];
static int   g_count = 0;

const char *severity_name(int severity)
{
    switch (severity) {
    case SEV_WARNING:  return "WARNING";
    case SEV_WATCH:    return "WATCH";
    case SEV_ADVISORY: return "ADVISORY";
    default:           return "INFO";
    }
}

/* =============================================================
 *  STORAGE
 * ============================================================= */

static int parse_alert(char *line, Alert *a)
{
    char *f[9];
    int n = split_fields(line, FIELD_SEP, f, 9);
    if (n < 8) return 0;

    memset(a, 0, sizeof *a);
    a->id          = atoi(f[0]);
    a->location_id = atoi(f[1]);
    a->severity    = clampi(atoi(f[2]), SEV_ADVISORY, SEV_WARNING);
    str_copy(a->type,    f[3], sizeof a->type);
    str_copy(a->message, f[4], sizeof a->message);
    str_copy(a->issued,  f[5], sizeof a->issued);
    str_copy(a->expires, f[6], sizeof a->expires);
    a->active      = atoi(f[7]) ? 1 : 0;
    return a->id > 0;
}

int alert_load(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    g_count = 0;
    data_path(FILE_ALERTS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL && g_count < MAX_ALERTS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_alert(line, &g_alerts[g_count])) g_count++;
    }
    fclose(fp);
    return g_count;
}

int alert_save(void)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    FILE *fp;
    int i;

    data_path(FILE_ALERTS, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# id|location_id(0=all)|severity(1=advisory,2=watch,3=warning)|"
                "type|message|issued|expires|active\n");
    for (i = 0; i < g_count; i++) {
        const Alert *a = &g_alerts[i];
        fprintf(fp, "%d|%d|%d|%s|%s|%s|%s|%d\n", a->id, a->location_id, a->severity,
                a->type, a->message, a->issued, a->expires, a->active);
    }
    fclose(fp);
    return replace_file(tmp, path);
}

/* =============================================================
 *  MANUAL ALERTS
 * ============================================================= */

int alert_count(void) { return g_count; }

Alert *alert_at(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return &g_alerts[index];
}

Alert *alert_find(int id)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_alerts[i].id == id) return &g_alerts[i];
    return NULL;
}

int alert_add(Alert *a)
{
    int i, max_id = 0;
    if (g_count >= MAX_ALERTS) return -1;
    for (i = 0; i < g_count; i++)
        if (g_alerts[i].id > max_id) max_id = g_alerts[i].id;
    a->id = max_id + 1;
    sanitize_field(a->type);
    sanitize_field(a->message);
    g_alerts[g_count++] = *a;
    return alert_save() == 0 ? a->id : -1;
}

int alert_delete(int id)
{
    int i;
    for (i = 0; i < g_count; i++) {
        if (g_alerts[i].id == id) {
            for (; i < g_count - 1; i++) g_alerts[i] = g_alerts[i + 1];
            g_count--;
            return alert_save();
        }
    }
    return -1;
}

int alert_toggle(int id)
{
    Alert *a = alert_find(id);
    if (a == NULL) return -1;
    a->active = !a->active;
    return alert_save();
}

/* Active and not yet expired? */
int alert_is_current(const Alert *a, time_t now)
{
    time_t exp;
    if (!a->active) return 0;
    exp = parse_time(a->expires);
    if (exp == (time_t)-1) return 1;          /* no valid expiry -> stays */
    return difftime(exp, now) > 0;
}

/* Current manual alerts that apply to a location (or to all locations). */
int alert_for_location(int location_id, time_t now, Alert out[], int max)
{
    int i, n = 0;
    for (i = 0; i < g_count && n < max; i++) {
        const Alert *a = &g_alerts[i];
        if (!alert_is_current(a, now)) continue;
        if (a->location_id == ALERT_ALL_LOCATIONS || a->location_id == location_id)
            out[n++] = *a;
    }
    return n;
}

/* Delete the alerts of a location that is being removed. */
int alert_remove_location(int location_id)
{
    int i, j = 0, removed = 0;
    for (i = 0; i < g_count; i++) {
        if (g_alerts[i].location_id == location_id && location_id != ALERT_ALL_LOCATIONS) {
            removed++;
            continue;
        }
        g_alerts[j++] = g_alerts[i];
    }
    g_count = j;
    if (removed) alert_save();
    return removed;
}

/* =============================================================
 *  AUTOMATIC DETECTION
 * ============================================================= */

static void push_auto(Alert out[], int *n, int max, int loc_id, int severity,
                      const char *type, const char *message)
{
    char now[TIME_LEN];
    int i;

    /* keep only the most severe alert of each type */
    for (i = 0; i < *n; i++) {
        if (strcmp(out[i].type, type) == 0) {
            if (severity > out[i].severity) {
                out[i].severity = severity;
                str_copy(out[i].message, message, sizeof out[i].message);
            }
            return;
        }
    }
    if (*n >= max) return;

    memset(&out[*n], 0, sizeof out[*n]);
    out[*n].id = 0;
    out[*n].location_id = loc_id;
    out[*n].severity = severity;
    out[*n].active = 1;
    str_copy(out[*n].type, type, sizeof out[*n].type);
    str_copy(out[*n].message, message, sizeof out[*n].message);
    now_string(now, sizeof now);
    str_copy(out[*n].issued, now, sizeof out[*n].issued);
    str_copy(out[*n].expires, "(auto)", sizeof out[*n].expires);
    (*n)++;
}

/*
 * Check the current weather and the next hours against fixed
 * thresholds. Thresholds follow common meteorological practice,
 * e.g. Bangladesh Meteorological Department heat-wave >= 36 C.
 */
int alert_detect(const Location *loc, const Weather *w,
                 const HourlyForecast hours[], int n_hours,
                 Alert out[], int max)
{
    int n = 0, i;
    double max_t = w->temperature, min_t = w->temperature;
    double max_wind = w->wind_speed, min_pres = w->pressure;
    int max_rain = w->rain_chance, fog = (w->condition == COND_FOG);
    int thunder = (w->condition == COND_THUNDERSTORM);
    int max_snow = 0;                /* precipitation chance while freezing */
    double rain_total = 0.0, snow_total = 0.0;
    char msg[MSG_LEN];

    if (w->temperature <= 1.0) { max_snow = max_rain; max_rain = 0; }

    for (i = 0; i < n_hours; i++) {
        const HourlyForecast *h = &hours[i];
        if (h->temperature > max_t) max_t = h->temperature;
        if (h->temperature < min_t) min_t = h->temperature;
        if (h->wind_speed > max_wind) max_wind = h->wind_speed;
        if (h->pressure < min_pres) min_pres = h->pressure;
        if (h->condition == COND_FOG) fog = 1;
        if (h->condition == COND_THUNDERSTORM) thunder = 1;
        if (h->temperature <= 1.0) {          /* falls as snow */
            if (h->rain_chance > max_snow) max_snow = h->rain_chance;
            snow_total += h->rainfall_mm;
        } else {
            if (h->rain_chance > max_rain) max_rain = h->rain_chance;
            rain_total += h->rainfall_mm;
        }
    }

    /* Heat wave */
    if (max_t >= 40.0) {
        snprintf(msg, sizeof msg, "Severe heat wave: temperature may reach %.1f C. "
                 "Avoid going out at noon.", max_t);
        push_auto(out, &n, max, loc->id, SEV_WARNING, "HEAT", msg);
    } else if (max_t >= 36.0) {
        snprintf(msg, sizeof msg, "Heat wave: temperature may reach %.1f C. "
                 "Drink plenty of water.", max_t);
        push_auto(out, &n, max, loc->id, SEV_WATCH, "HEAT", msg);
    }

    /* Cold */
    if (min_t <= -10.0) {
        snprintf(msg, sizeof msg, "Extreme cold: temperature may fall to %.1f C.", min_t);
        push_auto(out, &n, max, loc->id, SEV_WARNING, "COLD", msg);
    } else if (min_t <= 0.0) {
        snprintf(msg, sizeof msg, "Freezing temperature (%.1f C): roads may be icy.", min_t);
        push_auto(out, &n, max, loc->id, SEV_ADVISORY, "COLD", msg);
    } else if (min_t <= 10.0 && fabs(loc->latitude) < 30.0) {
        snprintf(msg, sizeof msg, "Cold wave: temperature may fall to %.1f C. "
                 "Keep warm clothes ready.", min_t);
        push_auto(out, &n, max, loc->id, SEV_WATCH, "COLD", msg);
    }

    /* Cyclone / depression */
    if (min_pres <= 995.0 && max_wind >= 50.0) {
        snprintf(msg, sizeof msg, "Cyclonic storm: pressure %.0f hPa, wind up to %.0f km/h. "
                 "Follow evacuation advice.", min_pres, max_wind);
        push_auto(out, &n, max, loc->id, SEV_WARNING, "CYCLONE", msg);
    } else if (min_pres <= 1000.0 && max_wind >= 35.0) {
        snprintf(msg, sizeof msg, "Low pressure (%.0f hPa) with gusty wind - "
                 "a depression may form.", min_pres);
        push_auto(out, &n, max, loc->id, SEV_WATCH, "CYCLONE", msg);
    }

    /* Strong wind */
    if (max_wind >= 60.0) {
        snprintf(msg, sizeof msg, "Storm wind up to %.0f km/h. Small boats must stay "
                 "close to shore.", max_wind);
        push_auto(out, &n, max, loc->id, SEV_WARNING, "WIND", msg);
    } else if (max_wind >= 40.0) {
        snprintf(msg, sizeof msg, "Strong wind up to %.0f km/h expected.", max_wind);
        push_auto(out, &n, max, loc->id, SEV_WATCH, "WIND", msg);
    }

    /* Snow (1 mm of water is roughly 1 cm of snow) */
    if (snow_total >= 10.0 || max_snow >= 75) {
        snprintf(msg, sizeof msg, "Heavy snowfall expected (about %.0f cm in 24 h). "
                 "Roads and flights may be disrupted.", snow_total);
        push_auto(out, &n, max, loc->id, snow_total >= 20.0 ? SEV_WARNING : SEV_WATCH,
                  "SNOW", msg);
    } else if (snow_total >= 2.0 || max_snow >= 50) {
        snprintf(msg, sizeof msg, "Snowfall likely (chance %d%%). Roads may be slippery.",
                 max_snow);
        push_auto(out, &n, max, loc->id, SEV_ADVISORY, "SNOW", msg);
    }

    /* Heavy rain */
    if (rain_total >= 50.0 || max_rain >= 90) {
        snprintf(msg, sizeof msg, "Heavy rainfall (about %.0f mm in 24 h). "
                 "Water-logging and landslides possible.", rain_total);
        push_auto(out, &n, max, loc->id, SEV_WARNING, "RAIN", msg);
    } else if (max_rain >= 75) {
        snprintf(msg, sizeof msg, "Heavy rain likely (chance %d%%).", max_rain);
        push_auto(out, &n, max, loc->id, SEV_WATCH, "RAIN", msg);
    }

    /* Thunderstorm */
    if (thunder)
        push_auto(out, &n, max, loc->id, SEV_WATCH, "THUNDER",
                  "Thunderstorm with lightning likely. Stay away from open fields and trees.");

    /* Fog */
    if (fog)
        push_auto(out, &n, max, loc->id, SEV_ADVISORY, "FOG",
                  "Dense fog expected. Drive slowly and use fog lights.");

    return n;
}
