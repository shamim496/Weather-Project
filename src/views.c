/*
 * views.c
 * ---------------------------------------------------------------
 * All weather display screens: current weather card, hourly and
 * daily forecast tables, smart advisory, city comparison, alert
 * lists and the all-locations weather table.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "views.h"
#include "advisory.h"
#include "location.h"
#include "weather.h"
#include "forecast.h"
#include "alert.h"
#include "ui.h"
#include "utils.h"

/* =============================================================
 *  SMALL HELPERS
 * ============================================================= */

/* Latest weather of a location (the feed is refreshed if needed). */
static const Weather *current_weather(int location_id)
{
    weather_sync(time(NULL), 0);
    return weather_get(location_id);
}

static void loc_time(const Location *l, time_t t, const char *fmt, char *buf, size_t size)
{
    struct tm tmv;
    location_local_tm(l, t, &tmv);
    strftime(buf, size, fmt, &tmv);
}

int view_condition_color(int c)
{
    switch (c) {
    case COND_CLEAR:         return COL_WARNING;    /* yellow sun   */
    case COND_PARTLY_CLOUDY: return COL_ACCENT;
    case COND_CLOUDY:        return COL_NORMAL;
    case COND_FOG:           return COL_MUTED;
    case COND_LIGHT_RAIN:    return COL_INFO;
    case COND_HEAVY_RAIN:    return COL_TITLE;
    case COND_THUNDERSTORM:  return COL_HIGHLIGHT;
    case COND_SNOW:          return COL_NORMAL;
    default:                 return COL_NORMAL;
    }
}

static int temp_color(double t)
{
    if (t >= 36.0) return COL_ERROR;
    if (t >= 30.0) return COL_WARNING;
    if (t <= 10.0) return COL_INFO;
    return COL_SUCCESS;
}

static void print_location_title(const Location *l)
{
    char when[64], coords[48];
    loc_time(l, time(NULL), "%a, %d %b %Y  %H:%M", when, sizeof when);
    location_format_coords(l, coords, sizeof coords);

    ui_color(COL_TITLE);
    printf("  %s", l->name);
    ui_color(COL_MUTED);
    printf("  (%s)\n", l->region);
    printf("  Local time: %s   |   %s   |   UTC%+.1f\n", when, coords, l->utc_offset);
    ui_reset();
}

/* =============================================================
 *  LOCATION SELECTION
 * ============================================================= */

void view_location_list(void)
{
    int i;
    ui_color(COL_TITLE);
    printf("  %-4s %-14s %-33s %s\n", "ID", "Location", "Region", "Coordinates");
    ui_reset();
    for (i = 0; i < location_count(); i++) {
        const Location *l = location_at(i);
        char coords[48];
        location_format_coords(l, coords, sizeof coords);
        printf("  %-4d %-14.14s %-33.33s %s\n", l->id, l->name, l->region, coords);
    }
}

int view_select_location(const char *prompt, int default_id)
{
    char query[NAME_LEN];
    char full_prompt[160];
    int ids[MAX_SEARCH_RESULT];
    int n, i, choice;
    const Location *def = location_find(default_id);

    if (location_count() == 0) {
        ui_msg_error("No locations available. Ask the administrator to add some.");
        return 0;
    }
    ui_color(COL_MUTED);
    printf("  Tip: type a city name or part of it (e.g. \"dha\"), an ID, or ? to list all.\n");
    ui_reset();

    for (;;) {
        if (def != NULL)
            snprintf(full_prompt, sizeof full_prompt, "%s [Enter = %s, 0 = cancel]: ",
                     prompt, def->name);
        else
            snprintf(full_prompt, sizeof full_prompt, "%s [0 = cancel]: ", prompt);

        input_line(full_prompt, query, sizeof query);

        if (query[0] == '\0') {
            if (def != NULL) return def->id;
            continue;
        }
        if (strcmp(query, "0") == 0) return 0;
        if (strcmp(query, "?") == 0) {
            view_location_list();
            continue;
        }
        if (str_is_number(query)) {
            int id = atoi(query);
            if (location_find(id) != NULL) return id;
            ui_msg_error("There is no location with ID %d.", id);
            continue;
        }

        n = location_search(query, ids, MAX_SEARCH_RESULT);
        if (n == 1) return ids[0];
        if (n > 1) {
            ui_msg_info("%d locations match \"%s\":", n, query);
            for (i = 0; i < n; i++) {
                const Location *l = location_find(ids[i]);
                ui_color(COL_ACCENT);
                printf("     %2d. ", i + 1);
                ui_reset();
                printf("%-14s %s\n", l->name, l->region);
            }
            choice = input_int("Choose a number (0 = search again): ", 0, n);
            if (choice > 0) return ids[choice - 1];
            continue;
        }

        n = location_suggest(query, ids, 3);
        if (n > 0) {
            ui_msg_warning("No location named \"%s\". Did you mean:", query);
            for (i = 0; i < n; i++) {
                ui_color(COL_ACCENT);
                printf("     %2d. ", i + 1);
                ui_reset();
                printf("%s\n", location_name(ids[i]));
            }
            choice = input_int("Choose a number (0 = search again): ", 0, n);
            if (choice > 0) return ids[choice - 1];
            continue;
        }
        ui_msg_error("No location found for \"%s\". Type ? to see all locations.", query);
    }
}

/* =============================================================
 *  CURRENT WEATHER
 * ============================================================= */

void view_current_weather(int location_id)
{
    const Location *l = location_find(location_id);
    const Weather *w;
    DailyForecast today;
    HourlyForecast hours[HOURLY_COUNT];
    Alert alerts[20];
    char title[96], label[64];
    double feels;
    int i, score, n_alerts;
    int ids[1];

    if (l == NULL) { ui_msg_error("Location not found."); return; }
    w = current_weather(location_id);

    snprintf(title, sizeof title, "Current Weather - %s", l->name);
    ui_header(title);
    print_location_title(l);
    ui_divider('-');
    if (w == NULL) { ui_msg_error("No weather data for this location yet."); return; }

    feels = feels_like(w->temperature, w->humidity, w->wind_speed);
    forecast_daily(l, w, time(NULL), &today, 1);
    forecast_hourly(l, w, time(NULL), hours, HOURLY_COUNT);
    score = comfort_score(w->temperature, w->humidity, w->wind_speed, w->rain_chance);

    for (i = 0; i < 6; i++) {
        ui_color((ColorRole)view_condition_color(w->condition));
        printf("    %s   ", i < 5 ? condition_icon_line(w->condition, i) : "             ");
        ui_reset();
        switch (i) {
        case 0:
            ui_color((ColorRole)view_condition_color(w->condition));
            printf("%s\n", condition_name(w->condition));
            ui_reset();
            break;
        case 1:
            printf("Temperature : ");
            ui_color((ColorRole)temp_color(w->temperature));
            printf("%.1f C", w->temperature);
            ui_reset();
            printf("   (feels like %.1f C - %s)\n", feels, heat_stress_label(feels));
            break;
        case 2:
            printf("Humidity    : %d %%\n", w->humidity);
            break;
        case 3:
            printf("Wind        : %.1f km/h from %s (%d deg)\n",
                   w->wind_speed, wind_direction_name(w->wind_dir), w->wind_dir);
            break;
        case 4:
            printf("Rain        : %d %% chance, %.1f mm/h expected\n",
                   w->rain_chance, w->rainfall_mm);
            break;
        default:
            printf("Pressure    : %.1f hPa\n", w->pressure);
            break;
        }
    }
    ui_divider('-');
    printf("  Today      : min ");
    ui_color(COL_INFO);
    printf("%.1f C", today.temp_min);
    ui_reset();
    printf(" / max ");
    ui_color(COL_ERROR);
    printf("%.1f C", today.temp_max);
    ui_reset();
    printf(",  rain up to %d %%, about %.1f mm in total\n", today.rain_chance, today.rainfall_mm);

    snprintf(label, sizeof label, "%d/100 (%s)", score, comfort_label(score));
    printf("  Comfort    : %-18s [", label);
    ui_color(score >= 60 ? COL_SUCCESS : score >= 40 ? COL_WARNING : COL_ERROR);
    ui_bar(score / 100.0, 20, '#');
    ui_reset();
    printf("]\n");

    ui_color(COL_MUTED);
    printf("  Updated    : %s  (%s, %ld min ago)\n", w->updated,
           w->source == SOURCE_MANUAL ? "entered by administrator" : "automatic weather feed",
           weather_age_minutes(w, time(NULL)));
    ui_reset();

    ids[0] = location_id;
    n_alerts = view_collect_alerts(ids, 1, alerts, 20);
    if (n_alerts > 0) {
        printf("\n");
        ui_msg_warning("%d alert(s) for %s:", n_alerts, l->name);
        for (i = 0; i < n_alerts; i++) view_print_alert(&alerts[i]);
    }
}

/* =============================================================
 *  HOURLY FORECAST
 * ============================================================= */

void view_hourly_forecast(int location_id)
{
    const Location *l = location_find(location_id);
    const Weather *w;
    HourlyForecast h[HOURLY_COUNT];
    char title[96], when[32];
    double tmin = 1000, tmax = -1000;
    int i;

    if (l == NULL) { ui_msg_error("Location not found."); return; }
    w = current_weather(location_id);
    forecast_hourly(l, w, time(NULL), h, HOURLY_COUNT);

    snprintf(title, sizeof title, "Hourly Forecast (next 24 hours) - %s", l->name);
    ui_header(title);
    print_location_title(l);
    ui_divider('-');

    for (i = 0; i < HOURLY_COUNT; i++) {
        if (h[i].temperature < tmin) tmin = h[i].temperature;
        if (h[i].temperature > tmax) tmax = h[i].temperature;
    }

    ui_color(COL_TITLE);
    printf("  %-6s %6s %6s %5s %10s %5s %5s  %-13s %s\n",
           "Time", "Temp", "Feels", "Hum", "Wind km/h", "Rain", "mm", "Condition", "Temp. chart");
    ui_reset();

    for (i = 0; i < HOURLY_COUNT; i++) {
        double f = feels_like(h[i].temperature, h[i].humidity, h[i].wind_speed);
        double frac = (tmax - tmin) > 0.1 ? (h[i].temperature - tmin) / (tmax - tmin) : 0.5;
        struct tm tmv;

        location_local_tm(l, h[i].time, &tmv);
        if (tmv.tm_hour == 0) {
            loc_time(l, h[i].time, "%A %d %B", when, sizeof when);
            ui_color(COL_MUTED);
            printf("  ------ %s ------\n", when);
            ui_reset();
        }
        loc_time(l, h[i].time, "%H:%M", when, sizeof when);
        printf("  %-6s ", when);
        ui_color((ColorRole)temp_color(h[i].temperature));
        printf("%5.1fC", h[i].temperature);
        ui_reset();
        printf(" %5.1fC %4d%% %5.0f %-4s %4d%% %5.1f  ",
               f, h[i].humidity, h[i].wind_speed, wind_direction_name(h[i].wind_dir),
               h[i].rain_chance, h[i].rainfall_mm);
        ui_color((ColorRole)view_condition_color(h[i].condition));
        printf("%-13s ", condition_name(h[i].condition));
        ui_color((ColorRole)temp_color(h[i].temperature));
        ui_bar(0.1 + 0.9 * frac, 10, '#');
        ui_reset();
        printf("\n");
    }
    ui_divider('-');
    printf("  Range next 24 h: %.1f C to %.1f C\n", tmin, tmax);
}

/* =============================================================
 *  DAILY FORECAST
 * ============================================================= */

void view_daily_forecast(int location_id)
{
    const Location *l = location_find(location_id);
    const Weather *w;
    DailyForecast d[DAILY_COUNT];
    char title[96], day[16], date[16];
    double wmin = 1000, wmax = -1000, total_rain = 0;
    int i, wettest = 0, hottest = 0;

    if (l == NULL) { ui_msg_error("Location not found."); return; }
    w = current_weather(location_id);
    forecast_daily(l, w, time(NULL), d, DAILY_COUNT);

    snprintf(title, sizeof title, "Daily Forecast (next 7 days) - %s", l->name);
    ui_header(title);
    print_location_title(l);
    ui_divider('-');

    ui_color(COL_TITLE);
    printf("  %-9s %-6s %5s %5s %4s %5s %4s %-8s %6s %s\n",
           "Day", "Date", "Min C", "Max C", "Rain", "mm", "Hum", "Wind", "hPa", "Condition");
    ui_reset();

    for (i = 0; i < DAILY_COUNT; i++) {
        loc_time(l, d[i].date + 43200, "%A", day, sizeof day);
        loc_time(l, d[i].date + 43200, "%d %b", date, sizeof date);
        if (i == 0) str_copy(day, "Today", sizeof day);
        else if (i == 1) str_copy(day, "Tomorrow", sizeof day);

        printf("  %-9s %-6s ", day, date);
        ui_color(COL_INFO);
        printf("%5.1f ", d[i].temp_min);
        ui_color((ColorRole)temp_color(d[i].temp_max));
        printf("%5.1f", d[i].temp_max);
        ui_reset();
        printf(" %3d%% %5.1f %3d%% %3.0f %-4s %6.1f ",
               d[i].rain_chance, d[i].rainfall_mm, d[i].humidity,
               d[i].wind_speed, wind_direction_name(d[i].wind_dir), d[i].pressure);
        ui_color((ColorRole)view_condition_color(d[i].condition));
        printf("%s\n", condition_name(d[i].condition));
        ui_reset();

        if (d[i].temp_min < wmin) wmin = d[i].temp_min;
        if (d[i].temp_max > wmax) wmax = d[i].temp_max;
        if (d[i].rainfall_mm > d[wettest].rainfall_mm) wettest = i;
        if (d[i].temp_max > d[hottest].temp_max) hottest = i;
        total_rain += d[i].rainfall_mm;
    }

    /* temperature range chart: each bar starts at the day's minimum and
       ends at its maximum on a common scale for the whole week */
    ui_section("Temperature range chart (min ---- max)");
    for (i = 0; i < DAILY_COUNT; i++) {
        int width = 40, start, end, k;
        double span = (wmax - wmin) > 0.1 ? (wmax - wmin) : 1.0;
        start = (int)((d[i].temp_min - wmin) / span * width + 0.5);
        end   = (int)((d[i].temp_max - wmin) / span * width + 0.5);
        loc_time(l, d[i].date + 43200, "%a %d", day, sizeof day);
        printf("  %-7s %5.1f |", day, d[i].temp_min);
        for (k = 0; k <= width; k++) {
            if (k >= start && k <= end) {
                ui_color((ColorRole)temp_color(wmin + span * k / width));
                putchar('=');
                ui_reset();
            } else {
                putchar(' ');
            }
        }
        printf("| %5.1f\n", d[i].temp_max);
    }

    ui_divider('-');
    loc_time(l, d[hottest].date + 43200, "%A", day, sizeof day);
    printf("  Week summary: %.1f C to %.1f C, hottest day %s (%.1f C)\n",
           wmin, wmax, day, d[hottest].temp_max);
    loc_time(l, d[wettest].date + 43200, "%A", day, sizeof day);
    if (total_rain >= 0.5)
        printf("  Expected rain this week: %.1f mm, wettest day %s (%.1f mm)\n",
               total_rain, day, d[wettest].rainfall_mm);
    else
        printf("  No significant rain expected this week.\n");
}

/* =============================================================
 *  SMART ADVISORY & BEST DAY PLANNER  (unique feature)
 * ============================================================= */

void view_advisory(int location_id)
{
    const Location *l = location_find(location_id);
    const Weather *w;
    HourlyForecast h[HOURLY_COUNT];
    DailyForecast d[DAILY_COUNT];
    Advice advice[MAX_ADVICE];
    int order[DAILY_COUNT];
    char title[96], day[32];
    const char *last_category = "";
    double feels;
    int i, n, score, best, worst;

    if (l == NULL) { ui_msg_error("Location not found."); return; }
    w = current_weather(location_id);
    if (w == NULL) { ui_msg_error("No weather data for this location."); return; }

    forecast_hourly(l, w, time(NULL), h, HOURLY_COUNT);
    forecast_daily(l, w, time(NULL), d, DAILY_COUNT);
    n = advisory_generate(w, h, HOURLY_COUNT, d, DAILY_COUNT, advice, MAX_ADVICE);

    snprintf(title, sizeof title, "Smart Weather Advisory & Best Day Planner - %s", l->name);
    ui_header(title);
    print_location_title(l);

    feels = feels_like(w->temperature, w->humidity, w->wind_speed);
    score = comfort_score(w->temperature, w->humidity, w->wind_speed, w->rain_chance);
    printf("\n  Now: %.1f C, %s, humidity %d%%, wind %.0f km/h, rain chance %d%%\n",
           w->temperature, condition_name(w->condition), w->humidity, w->wind_speed,
           w->rain_chance);
    printf("  Feels like  : %.1f C (%s)\n", feels, heat_stress_label(feels));
    printf("  Comfort now : %3d/100 [", score);
    ui_color(score >= 60 ? COL_SUCCESS : score >= 40 ? COL_WARNING : COL_ERROR);
    ui_bar(score / 100.0, 25, '#');
    ui_reset();
    printf("] %s\n", comfort_label(score));

    for (i = 0; i < n; i++) {
        if (strcmp(advice[i].category, last_category) != 0) {
            ui_section(advice[i].category);
            last_category = advice[i].category;
        }
        if (advice[i].level == ADVICE_GOOD) {
            ui_color(COL_SUCCESS);  printf("   [GOOD]    ");
        } else if (advice[i].level == ADVICE_CAUTION) {
            ui_color(COL_WARNING);  printf("   [CAUTION] ");
        } else {
            ui_color(COL_ERROR);    printf("   [DANGER]  ");
        }
        ui_reset();
        printf("%s\n", advice[i].text);
    }

    /* ---------- best day planner ---------- */
    rank_days(d, DAILY_COUNT, order);
    best = order[0];
    worst = order[DAILY_COUNT - 1];

    ui_section("Best Day Planner - comfort score of the next 7 days");
    for (i = 0; i < DAILY_COUNT; i++) {
        int s = day_score(&d[i]);
        loc_time(l, d[i].date + 43200, "%a %d %b", day, sizeof day);
        printf("   %-10s %3d ", day, s);
        ui_color(s >= 60 ? COL_SUCCESS : s >= 40 ? COL_WARNING : COL_ERROR);
        ui_bar(s / 100.0, 20, '#');
        ui_reset();
        printf(" %-9s %4.0f-%2.0fC rain%3d%%", comfort_label(s), d[i].temp_min,
               d[i].temp_max, d[i].rain_chance);
        if (i == best) {
            ui_color(COL_SUCCESS);
            printf("  <= BEST");
        } else if (i == worst && day_score(&d[worst]) < day_score(&d[best])) {
            ui_color(COL_ERROR);
            printf("  <= avoid");
        }
        ui_reset();
        printf("\n");
    }
    ui_divider('-');
    loc_time(l, d[best].date + 43200, "%A, %d %B", day, sizeof day);
    ui_color(COL_SUCCESS);
    printf("  >> Best day for outdoor plans: %s (score %d, %s)\n",
           day, day_score(&d[best]), comfort_label(day_score(&d[best])));
    if (day_score(&d[worst]) < day_score(&d[best])) {
        loc_time(l, d[worst].date + 43200, "%A, %d %B", day, sizeof day);
        ui_color(COL_WARNING);
        printf("  >> Least suitable day: %s (score %d, %s)\n", day, day_score(&d[worst]),
               condition_name(d[worst].condition));
    }
    ui_reset();
}

/* =============================================================
 *  COMPARE TWO LOCATIONS
 * ============================================================= */

static void compare_row(const char *label, const char *a, const char *b)
{
    ui_color(COL_ACCENT);
    printf("  %-22s", label);
    ui_reset();
    printf(" %-24s %-24s\n", a, b);
}

void view_compare(int first_id, int second_id)
{
    const Location *la = location_find(first_id);
    const Location *lb = location_find(second_id);
    const Weather *wa, *wb;
    DailyForecast da[DAILY_COUNT], db[DAILY_COUNT];
    char a[48], b[48];
    double rain_a = 0, rain_b = 0;
    int i, sa, sb;

    if (la == NULL || lb == NULL) { ui_msg_error("Location not found."); return; }
    weather_sync(time(NULL), 0);
    wa = weather_get(first_id);
    wb = weather_get(second_id);
    if (wa == NULL || wb == NULL) { ui_msg_error("Weather data missing."); return; }

    forecast_daily(la, wa, time(NULL), da, DAILY_COUNT);
    forecast_daily(lb, wb, time(NULL), db, DAILY_COUNT);
    for (i = 0; i < DAILY_COUNT; i++) {
        rain_a += da[i].rainfall_mm;
        rain_b += db[i].rainfall_mm;
    }
    sa = comfort_score(wa->temperature, wa->humidity, wa->wind_speed, wa->rain_chance);
    sb = comfort_score(wb->temperature, wb->humidity, wb->wind_speed, wb->rain_chance);

    ui_header("Compare Two Locations");
    ui_color(COL_TITLE);
    printf("  %-22s %-24s %-24s\n", "", la->name, lb->name);
    ui_reset();
    ui_divider('-');

    compare_row("Condition", condition_name(wa->condition), condition_name(wb->condition));
    snprintf(a, sizeof a, "%.1f C", wa->temperature);
    snprintf(b, sizeof b, "%.1f C", wb->temperature);
    compare_row("Temperature", a, b);
    snprintf(a, sizeof a, "%.1f C", feels_like(wa->temperature, wa->humidity, wa->wind_speed));
    snprintf(b, sizeof b, "%.1f C", feels_like(wb->temperature, wb->humidity, wb->wind_speed));
    compare_row("Feels like", a, b);
    snprintf(a, sizeof a, "%d %%", wa->humidity);
    snprintf(b, sizeof b, "%d %%", wb->humidity);
    compare_row("Humidity", a, b);
    snprintf(a, sizeof a, "%.1f km/h %s", wa->wind_speed, wind_direction_name(wa->wind_dir));
    snprintf(b, sizeof b, "%.1f km/h %s", wb->wind_speed, wind_direction_name(wb->wind_dir));
    compare_row("Wind", a, b);
    snprintf(a, sizeof a, "%d %%", wa->rain_chance);
    snprintf(b, sizeof b, "%d %%", wb->rain_chance);
    compare_row("Rain chance", a, b);
    snprintf(a, sizeof a, "%.1f hPa", wa->pressure);
    snprintf(b, sizeof b, "%.1f hPa", wb->pressure);
    compare_row("Pressure", a, b);
    snprintf(a, sizeof a, "%.1f / %.1f C", da[0].temp_min, da[0].temp_max);
    snprintf(b, sizeof b, "%.1f / %.1f C", db[0].temp_min, db[0].temp_max);
    compare_row("Today min / max", a, b);
    snprintf(a, sizeof a, "%.1f mm", rain_a);
    snprintf(b, sizeof b, "%.1f mm", rain_b);
    compare_row("Rain next 7 days", a, b);
    snprintf(a, sizeof a, "%d (%s)", sa, comfort_label(sa));
    snprintf(b, sizeof b, "%d (%s)", sb, comfort_label(sb));
    compare_row("Comfort score", a, b);
    ui_divider('-');

    printf("  Distance between them: %.0f km\n",
           haversine_km(la->latitude, la->longitude, lb->latitude, lb->longitude));
    if (fabs(wa->temperature - wb->temperature) < 0.5)
        printf("  Both locations have almost the same temperature.\n");
    else
        printf("  %s is %.1f C warmer than %s.\n",
               wa->temperature > wb->temperature ? la->name : lb->name,
               fabs(wa->temperature - wb->temperature),
               wa->temperature > wb->temperature ? lb->name : la->name);
    if (sa != sb)
        printf("  More comfortable right now: %s\n", sa > sb ? la->name : lb->name);
}

/* =============================================================
 *  WEATHER TABLE (all locations)
 * ============================================================= */

void view_weather_table(void)
{
    int i;
    time_t now = time(NULL);

    ui_color(COL_TITLE);
    printf("  %-3s %-12s %6s %4s %6s %4s %7s %-13s %-5s %s\n",
           "ID", "Location", "Temp", "Hum", "Wind", "Rain", "hPa", "Condition", "Time", "Status");
    ui_reset();
    for (i = 0; i < location_count(); i++) {
        const Location *l = location_at(i);
        const Weather *w = weather_get(l->id);
        if (w == NULL) {
            printf("  %-3d %-12.12s %s\n", l->id, l->name, "-- no data --");
            continue;
        }
        printf("  %-3d %-12.12s ", l->id, l->name);
        ui_color((ColorRole)temp_color(w->temperature));
        printf("%5.1fC", w->temperature);
        ui_reset();
        printf(" %3d%% %6.1f %3d%% %7.1f ", w->humidity, w->wind_speed, w->rain_chance,
               w->pressure);
        ui_color((ColorRole)view_condition_color(w->condition));
        printf("%-13.13s ", condition_name(w->condition));
        ui_reset();
        printf("%-5.5s ", strlen(w->updated) >= 16 ? w->updated + 11 : w->updated);
        if (w->source == SOURCE_MANUAL) {
            ui_color(COL_HIGHLIGHT);
            printf("MANUAL");
        } else if (weather_age_minutes(w, now) >= AUTO_REFRESH_MINUTES) {
            ui_color(COL_WARNING);
            printf("STALE");
        } else {
            ui_color(COL_SUCCESS);
            printf("AUTO");
        }
        ui_reset();
        printf("\n");
    }
}

/* =============================================================
 *  ALERTS
 * ============================================================= */

void view_print_alert(const Alert *a)
{
    int role = a->severity == SEV_WARNING ? COL_ERROR :
               a->severity == SEV_WATCH   ? COL_WARNING : COL_INFO;
    ui_color((ColorRole)role);
    printf("   [%-8s] %-7s ", severity_name(a->severity), a->type);
    ui_reset();
    printf("%s: %s\n",
           a->location_id == ALERT_ALL_LOCATIONS ? "All locations" : location_name(a->location_id),
           a->message);
    ui_color(COL_MUTED);
    if (a->id > 0)
        printf("%25sIssued %s by admin, valid until %s\n", "", a->issued, a->expires);
    else
        printf("%25sDetected automatically by the system\n", "");
    ui_reset();
}

/*
 * Collect current manual alerts and automatically detected alerts
 * for a list of locations. An "all locations" alert is listed once.
 */
int view_collect_alerts(const int ids[], int n, Alert out[], int max)
{
    time_t now = time(NULL);
    int count = 0, i, j, k;

    weather_sync(now, 0);
    for (i = 0; i < n && count < max; i++) {
        const Location *l = location_find(ids[i]);
        const Weather *w = weather_get(ids[i]);
        Alert found[20];
        int m;

        if (l == NULL) continue;

        /* manual alerts */
        m = alert_for_location(ids[i], now, found, 20);
        for (j = 0; j < m && count < max; j++) {
            int duplicate = 0;
            for (k = 0; k < count; k++)
                if (out[k].id != 0 && out[k].id == found[j].id) duplicate = 1;
            if (!duplicate) out[count++] = found[j];
        }

        /* automatic alerts */
        if (w != NULL) {
            HourlyForecast h[HOURLY_COUNT];
            forecast_hourly(l, w, now, h, HOURLY_COUNT);
            m = alert_detect(l, w, h, HOURLY_COUNT, found, 20);
            for (j = 0; j < m && count < max; j++) out[count++] = found[j];
        }
    }

    /* most severe first */
    for (i = 1; i < count; i++) {
        Alert key = out[i];
        j = i - 1;
        while (j >= 0 && out[j].severity < key.severity) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return count;
}

void view_alerts(const int ids[], int n)
{
    Alert alerts[60];
    int count = view_collect_alerts(ids, n, alerts, 60);
    int i;

    if (count == 0) {
        ui_msg_success("No weather alerts for these locations. Stay safe!");
        return;
    }
    ui_msg_warning("%d alert(s) found (most severe first):", count);
    printf("\n");
    for (i = 0; i < count; i++) view_print_alert(&alerts[i]);
}
