/*
 * report.c
 * ---------------------------------------------------------------
 * Generates plain-text reports for the administrator:
 *   1. Weather summary of all locations (+ statistics, category-wise
 *      condition count and active alerts)
 *   2. Detailed 24-hour / 7-day forecast report of one location
 *   3. User activity report (accounts, searches, top locations)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "report.h"
#include "location.h"
#include "weather.h"
#include "forecast.h"
#include "alert.h"
#include "advisory.h"
#include "user.h"
#include "userdata.h"
#include "logger.h"
#include "utils.h"

#define RULE "=============================================================================="
#define LINE "------------------------------------------------------------------------------"

/* Build "reports/<prefix>_YYYYMMDD_HHMMSS.txt" and open it. */
static FILE *open_report(const char *prefix, char *out_path, size_t size)
{
    char stamp[32];
    time_t now = time(NULL);
    FILE *fp;

    strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", localtime(&now));
    ensure_dir(get_report_dir());
    snprintf(out_path, size, "%s/%s_%s.txt", get_report_dir(), prefix, stamp);
    fp = fopen(out_path, "w");
    return fp;
}

static void report_title(FILE *fp, const char *title)
{
    char when[TIME_LEN];
    now_string(when, sizeof when);
    fprintf(fp, "%s\n", RULE);
    fprintf(fp, "%*s\n", 39 + (int)strlen(APP_NAME) / 2, APP_NAME);
    fprintf(fp, "%*s\n", 39 + (int)strlen(title) / 2, title);
    fprintf(fp, "%s\n", RULE);
    fprintf(fp, "Generated on : %s\n\n", when);
}

static void loc_time(const Location *l, time_t t, const char *fmt, char *buf, size_t size)
{
    struct tm tmv;
    location_local_tm(l, t, &tmv);
    strftime(buf, size, fmt, &tmv);
}

/* =============================================================
 *  1. WEATHER SUMMARY
 * ============================================================= */

int report_weather_summary(char *out_path, size_t size)
{
    FILE *fp;
    int i, n = 0;
    int hottest = -1, coldest = -1, windiest = -1, wettest = -1, humid = -1;
    int cond_count[COND_COUNT] = {0};
    double temp_sum = 0;
    time_t now = time(NULL);
    int total_alerts = 0;

    weather_sync(now, 0);
    fp = open_report("weather_summary", out_path, size);
    if (fp == NULL) return -1;

    report_title(fp, "WEATHER SUMMARY REPORT");
    fprintf(fp, "Locations    : %d\n\n", location_count());

    fprintf(fp, "1. CURRENT WEATHER - ALL LOCATIONS\n%s\n", LINE);
    fprintf(fp, "%-3s %-13s %6s %6s %4s %10s %5s %7s %-13s\n",
            "ID", "Location", "Temp", "Feels", "Hum", "Wind", "Rain", "hPa", "Condition");
    fprintf(fp, "%s\n", LINE);

    for (i = 0; i < location_count(); i++) {
        const Location *l = location_at(i);
        const Weather *w = weather_get(l->id);
        if (w == NULL) continue;
        fprintf(fp, "%-3d %-13.13s %5.1fC %5.1fC %3d%% %5.1f %-4s %4d%% %7.1f %-13s\n",
                l->id, l->name, w->temperature,
                feels_like(w->temperature, w->humidity, w->wind_speed),
                w->humidity, w->wind_speed, wind_direction_name(w->wind_dir),
                w->rain_chance, w->pressure, condition_name(w->condition));

        if (hottest < 0 || w->temperature > weather_get(hottest)->temperature) hottest = l->id;
        if (coldest < 0 || w->temperature < weather_get(coldest)->temperature) coldest = l->id;
        if (windiest < 0 || w->wind_speed > weather_get(windiest)->wind_speed) windiest = l->id;
        if (wettest < 0 || w->rain_chance > weather_get(wettest)->rain_chance) wettest = l->id;
        if (humid < 0 || w->humidity > weather_get(humid)->humidity) humid = l->id;
        cond_count[w->condition]++;
        temp_sum += w->temperature;
        n++;
    }

    fprintf(fp, "\n2. STATISTICS\n%s\n", LINE);
    if (n > 0) {
        fprintf(fp, "Average temperature : %.1f C\n", temp_sum / n);
        fprintf(fp, "Hottest location    : %-14s %.1f C\n", location_name(hottest),
                weather_get(hottest)->temperature);
        fprintf(fp, "Coldest location    : %-14s %.1f C\n", location_name(coldest),
                weather_get(coldest)->temperature);
        fprintf(fp, "Most humid          : %-14s %d %%\n", location_name(humid),
                weather_get(humid)->humidity);
        fprintf(fp, "Windiest            : %-14s %.1f km/h\n", location_name(windiest),
                weather_get(windiest)->wind_speed);
        fprintf(fp, "Highest rain chance : %-14s %d %%\n", location_name(wettest),
                weather_get(wettest)->rain_chance);
    }

    fprintf(fp, "\n3. CONDITION-WISE SUMMARY\n%s\n", LINE);
    for (i = 0; i < COND_COUNT; i++) {
        int k;
        fprintf(fp, "%-14s : %3d  ", condition_name((Condition)i), cond_count[i]);
        for (k = 0; k < cond_count[i]; k++) fputc('#', fp);
        fputc('\n', fp);
    }

    fprintf(fp, "\n4. ACTIVE ALERTS\n%s\n", LINE);
    for (i = 0; i < alert_count(); i++) {
        const Alert *a = alert_at(i);
        if (!alert_is_current(a, now)) continue;
        fprintf(fp, "[%-8s] %-8s %-14s %s (until %s)\n", severity_name(a->severity), a->type,
                a->location_id == ALERT_ALL_LOCATIONS ? "All" : location_name(a->location_id),
                a->message, a->expires);
        total_alerts++;
    }
    for (i = 0; i < location_count(); i++) {
        const Location *l = location_at(i);
        const Weather *w = weather_get(l->id);
        HourlyForecast h[HOURLY_COUNT];
        Alert found[16];
        int m, j;
        if (w == NULL) continue;
        forecast_hourly(l, w, now, h, HOURLY_COUNT);
        m = alert_detect(l, w, h, HOURLY_COUNT, found, 16);
        for (j = 0; j < m; j++) {
            fprintf(fp, "[%-8s] %-8s %-14s %s (auto)\n", severity_name(found[j].severity),
                    found[j].type, l->name, found[j].message);
            total_alerts++;
        }
    }
    if (total_alerts == 0) fprintf(fp, "No active alerts.\n");

    fprintf(fp, "\n%s\nEnd of report\n", RULE);
    fclose(fp);
    return 0;
}

/* =============================================================
 *  2. LOCATION FORECAST
 * ============================================================= */

int report_location_forecast(int location_id, char *out_path, size_t size)
{
    const Location *l = location_find(location_id);
    const Weather *w;
    HourlyForecast h[HOURLY_COUNT];
    DailyForecast d[DAILY_COUNT];
    Advice advice[MAX_ADVICE];
    int order[DAILY_COUNT];
    char prefix[64], buf[48];
    FILE *fp;
    int i, n;
    time_t now = time(NULL);

    if (l == NULL) return -1;
    weather_sync(now, 0);
    w = weather_get(location_id);
    if (w == NULL) return -1;

    /* file name: forecast_<city> with only letters/digits */
    snprintf(prefix, sizeof prefix, "forecast_%s", l->name);
    for (i = 0; prefix[i]; i++)
        if (!isalnum((unsigned char)prefix[i]) && prefix[i] != '_') prefix[i] = '_';

    fp = open_report(prefix, out_path, size);
    if (fp == NULL) return -1;

    forecast_hourly(l, w, now, h, HOURLY_COUNT);
    forecast_daily(l, w, now, d, DAILY_COUNT);

    report_title(fp, "LOCATION FORECAST REPORT");
    location_format_coords(l, buf, sizeof buf);
    fprintf(fp, "Location     : %s (%s)\n", l->name, l->region);
    fprintf(fp, "Coordinates  : %s, UTC%+.1f\n\n", buf, l->utc_offset);

    fprintf(fp, "1. CURRENT WEATHER\n%s\n", LINE);
    fprintf(fp, "Condition    : %s\n", condition_name(w->condition));
    fprintf(fp, "Temperature  : %.1f C (feels like %.1f C)\n", w->temperature,
            feels_like(w->temperature, w->humidity, w->wind_speed));
    fprintf(fp, "Humidity     : %d %%\n", w->humidity);
    fprintf(fp, "Wind         : %.1f km/h from %s\n", w->wind_speed, wind_direction_name(w->wind_dir));
    fprintf(fp, "Rain chance  : %d %% (%.1f mm/h)\n", w->rain_chance, w->rainfall_mm);
    fprintf(fp, "Pressure     : %.1f hPa\n", w->pressure);
    fprintf(fp, "Updated      : %s\n\n", w->updated);

    fprintf(fp, "2. HOURLY FORECAST (NEXT 24 HOURS, LOCAL TIME)\n%s\n", LINE);
    fprintf(fp, "%-11s %6s %5s %10s %5s %6s %8s  %s\n",
            "Time", "Temp", "Hum", "Wind", "Rain", "mm", "hPa", "Condition");
    for (i = 0; i < HOURLY_COUNT; i++) {
        loc_time(l, h[i].time, "%d %b %H:%M", buf, sizeof buf);
        fprintf(fp, "%-11s %5.1fC %4d%% %5.1f %-4s %4d%% %6.1f %8.1f  %s\n", buf,
                h[i].temperature, h[i].humidity, h[i].wind_speed,
                wind_direction_name(h[i].wind_dir), h[i].rain_chance, h[i].rainfall_mm,
                h[i].pressure, condition_name(h[i].condition));
    }

    fprintf(fp, "\n3. DAILY FORECAST (NEXT 7 DAYS)\n%s\n", LINE);
    fprintf(fp, "%-15s %6s %6s %5s %6s %5s %6s %6s  %s\n",
            "Day", "Min", "Max", "Rain", "mm", "Hum", "Wind", "Score", "Condition");
    for (i = 0; i < DAILY_COUNT; i++) {
        loc_time(l, d[i].date + 43200, "%a %d %b %Y", buf, sizeof buf);
        fprintf(fp, "%-15s %5.1fC %5.1fC %4d%% %6.1f %4d%% %6.1f %6d  %s\n", buf,
                d[i].temp_min, d[i].temp_max, d[i].rain_chance, d[i].rainfall_mm,
                d[i].humidity, d[i].wind_speed, day_score(&d[i]),
                condition_name(d[i].condition));
    }

    fprintf(fp, "\n4. SMART ADVISORY\n%s\n", LINE);
    n = advisory_generate(w, h, HOURLY_COUNT, d, DAILY_COUNT, advice, MAX_ADVICE);
    for (i = 0; i < n; i++)
        fprintf(fp, "%-12s %-8s %s\n", advice[i].category,
                advice[i].level == ADVICE_GOOD ? "GOOD" :
                advice[i].level == ADVICE_CAUTION ? "CAUTION" : "DANGER",
                advice[i].text);
    rank_days(d, DAILY_COUNT, order);
    loc_time(l, d[order[0]].date + 43200, "%A %d %B", buf, sizeof buf);
    fprintf(fp, "\nBest day for outdoor activities: %s (score %d)\n", buf, day_score(&d[order[0]]));

    fprintf(fp, "\n%s\nEnd of report\n", RULE);
    fclose(fp);
    return 0;
}

/* =============================================================
 *  3. USER ACTIVITY
 * ============================================================= */

int report_user_activity(char *out_path, size_t size)
{
    FILE *fp;
    int i, active = 0, top_ids[10], top_n;
    long top_counts[10];

    fp = open_report("user_activity", out_path, size);
    if (fp == NULL) return -1;

    for (i = 0; i < user_count(); i++)
        if (user_at(i)->active) active++;

    report_title(fp, "USER ACTIVITY REPORT");
    fprintf(fp, "1. SUMMARY\n%s\n", LINE);
    fprintf(fp, "Registered users     : %d\n", user_count());
    fprintf(fp, "Active / blocked     : %d / %d\n", active, user_count() - active);
    fprintf(fp, "Total searches       : %ld\n", history_total());
    fprintf(fp, "Saved favourites     : %ld\n", favorites_total());
    fprintf(fp, "Successful logins    : %ld\n", log_count_type("LOGIN"));
    fprintf(fp, "Failed login attempts: %ld\n\n", log_count_type("LOGIN_FAIL"));

    fprintf(fp, "2. USERS\n%s\n", LINE);
    fprintf(fp, "%-16s %-20s %-12s %-7s %8s %-19s\n",
            "Username", "Full name", "Home", "Status", "Searches", "Last login");
    for (i = 0; i < user_count(); i++) {
        const User *u = user_at(i);
        fprintf(fp, "%-16s %-20.20s %-12.12s %-7s %8ld %-19s\n", u->username, u->full_name,
                location_name(u->home_location), u->active ? "Active" : "Blocked",
                history_count_user(u->username), u->last_login);
    }

    fprintf(fp, "\n3. MOST SEARCHED LOCATIONS\n%s\n", LINE);
    top_n = history_top_locations(top_ids, top_counts, 10);
    if (top_n == 0) fprintf(fp, "No searches yet.\n");
    for (i = 0; i < top_n; i++) {
        long k;
        fprintf(fp, "%2d. %-14s %5ld  ", i + 1, location_name(top_ids[i]), top_counts[i]);
        for (k = 0; k < top_counts[i] && k < 40; k++) fputc('#', fp);
        fputc('\n', fp);
    }

    fprintf(fp, "\n%s\nEnd of report\n", RULE);
    fclose(fp);
    return 0;
}

/* Print a generated report on the screen. */
int report_print_file(const char *path)
{
    char line[LINE_LEN];
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;
    while (fgets(line, sizeof line, fp) != NULL) printf("  %s", line);
    fclose(fp);
    return 0;
}
