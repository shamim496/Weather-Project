/*
 * admin_menu.c
 * ---------------------------------------------------------------
 * ADMIN MODULE - user management, weather data monitoring,
 * location management, alert management, system monitoring,
 * report generation and admin password change.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "menus.h"
#include "views.h"
#include "user.h"
#include "userdata.h"
#include "location.h"
#include "weather.h"
#include "forecast.h"
#include "alert.h"
#include "report.h"
#include "logger.h"
#include "ui.h"
#include "utils.h"

/* =============================================================
 *  1. USER MANAGEMENT
 * ============================================================= */

static void print_user_table(void)
{
    int i;
    if (user_count() == 0) {
        ui_msg_info("No registered users yet.");
        return;
    }
    ui_color(COL_TITLE);
    printf("  %-3s %-16s %-20s %-12s %-8s %s\n",
           "#", "Username", "Full name", "Home", "Status", "Last login");
    ui_reset();
    for (i = 0; i < user_count(); i++) {
        const User *u = user_at(i);
        printf("  %-3d %-16s %-20.20s %-12.12s ", i + 1, u->username, u->full_name,
               location_name(u->home_location));
        ui_color(u->active ? COL_SUCCESS : COL_ERROR);
        printf("%-8s", u->active ? "Active" : "Blocked");
        ui_reset();
        printf(" %s\n", u->last_login);
    }
    printf("\n  Total: %d user(s)\n", user_count());
}

static User *ask_user(void)
{
    char name[USERNAME_LEN];
    User *u;
    input_line("Username (empty = cancel): ", name, sizeof name);
    if (name[0] == '\0') return NULL;
    u = user_find(name);
    if (u == NULL) ui_msg_error("No user named '%s'.", name);
    return u;
}

static void user_details(void)
{
    User *u = ask_user();
    int fav[MAX_FAVORITES], nf, i;
    HistoryEntry h[MAX_HISTORY_SHOW];
    int nh;

    if (u == NULL) return;
    ui_section("User details");
    printf("  Username      : %s\n", u->username);
    printf("  Full name     : %s\n", u->full_name);
    printf("  Status        : %s\n", u->active ? "Active" : "Blocked");
    printf("  Home location : %s\n", location_name(u->home_location));
    printf("  Theme         : %s\n", ui_theme_name(u->theme));
    printf("  Registered    : %s\n", u->created);
    printf("  Last login    : %s\n", u->last_login);
    printf("  Total searches: %ld\n", history_count_user(u->username));

    nf = favorites_list(u->username, fav, MAX_FAVORITES);
    printf("  Favourites    : ");
    if (nf == 0) printf("(none)");
    for (i = 0; i < nf; i++) printf("%s%s", i ? ", " : "", location_name(fav[i]));
    printf("\n");

    nh = history_list(u->username, h, 5);
    printf("  Recent search : ");
    if (nh == 0) printf("(none)\n");
    for (i = 0; i < nh; i++)
        printf("%s%s at %s\n", i ? "                  " : "", location_name(h[i].location_id), h[i].time);
}

static void user_management(void)
{
    for (;;) {
        int choice;
        User *u;
        char err[128], pass[PASSWORD_LEN];

        ui_header("Admin > User Management");
        print_user_table();
        ui_section("Options");
        ui_menu_item(1, "View user details");
        ui_menu_item(2, "Block / unblock a user");
        ui_menu_item(3, "Reset a user's password");
        ui_menu_item(4, "Delete a user");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 4);

        switch (choice) {
        case 0:
            return;
        case 1:
            user_details();
            ui_pause();
            break;
        case 2:
            if ((u = ask_user()) != NULL) {
                int new_state = !u->active;
                user_set_active(u->username, new_state);
                log_event("ADMIN", "user '%s' %s", u->username, new_state ? "unblocked" : "blocked");
                ui_msg_success("User '%s' is now %s.", u->username, new_state ? "ACTIVE" : "BLOCKED");
            }
            ui_pause();
            break;
        case 3:
            if ((u = ask_user()) != NULL) {
                input_line("New password: ", pass, sizeof pass);
                if (!validate_password(pass, err, sizeof err)) {
                    ui_msg_error("%s", err);
                } else {
                    user_change_password(u->username, pass);
                    log_event("ADMIN", "password of '%s' was reset", u->username);
                    ui_msg_success("Password of '%s' has been reset.", u->username);
                }
            }
            ui_pause();
            break;
        case 4:
            if ((u = ask_user()) != NULL) {
                char name[USERNAME_LEN];
                str_copy(name, u->username, sizeof name);
                if (input_yes_no("Delete this user with favourites and history? (y/n): ")) {
                    favorites_remove_user(name);
                    history_clear(name);
                    user_delete(name);
                    log_event("ADMIN", "user '%s' deleted", name);
                    ui_msg_success("User '%s' deleted.", name);
                }
            }
            ui_pause();
            break;
        }
    }
}

/* =============================================================
 *  2. WEATHER DATA MONITORING
 * ============================================================= */

static void manual_weather_entry(void)
{
    int id = view_select_location("Location to update", 0);
    Weather w;
    const Weather *old;
    Condition derived;
    int i, c;

    if (id == 0) return;
    old = weather_get(id);
    memset(&w, 0, sizeof w);
    if (old) w = *old;
    w.location_id = id;

    ui_msg_info("Enter new values for %s (press Enter to keep the value in brackets).",
                location_name(id));
    {
        char p[80];
        snprintf(p, sizeof p, "Temperature C   [%.1f]: ", w.temperature);
        w.temperature = input_double_default(p, -60.0, 60.0, w.temperature);
        snprintf(p, sizeof p, "Humidity %%      [%d]: ", w.humidity);
        w.humidity = input_int_default(p, 0, 100, w.humidity);
        snprintf(p, sizeof p, "Wind speed km/h [%.1f]: ", w.wind_speed);
        w.wind_speed = input_double_default(p, 0.0, 300.0, w.wind_speed);
        snprintf(p, sizeof p, "Wind direction degrees (0=N, 90=E) [%d]: ", w.wind_dir);
        w.wind_dir = input_int_default(p, 0, 359, w.wind_dir);
        snprintf(p, sizeof p, "Rain chance %%   [%d]: ", w.rain_chance);
        w.rain_chance = input_int_default(p, 0, 100, w.rain_chance);
        snprintf(p, sizeof p, "Pressure hPa    [%.1f]: ", w.pressure);
        w.pressure = input_double_default(p, 870.0, 1085.0, w.pressure);
    }
    w.rainfall_mm = w.rain_chance >= 40 ? (w.rain_chance - 40) / 60.0 * (w.rain_chance - 40) / 60.0 * 6.0 : 0.0;

    derived = condition_derive(w.temperature, w.humidity, w.rain_chance, w.wind_speed, w.pressure);
    printf("\n");
    for (i = 0; i < COND_COUNT; i++) ui_menu_item(i + 1, condition_name((Condition)i));
    {
        char p[80];
        snprintf(p, sizeof p, "Condition [Enter = %s]: ", condition_name(derived));
        c = input_int_default(p, 1, COND_COUNT, (int)derived + 1);
    }
    w.condition = (Condition)(c - 1);
    w.source = SOURCE_MANUAL;
    now_string(w.updated, sizeof w.updated);

    if (weather_set(&w) == 0) {
        log_event("WEATHER", "admin updated weather of %s manually", location_name(id));
        ui_msg_success("Weather of %s saved (kept for %d hours, then the automatic feed "
                       "takes over).", location_name(id), MANUAL_KEEP_MINUTES / 60);
    } else {
        ui_msg_error("Could not save weather data.");
    }
}

static void weather_monitoring(void)
{
    for (;;) {
        int choice, id, n;

        ui_header("Admin > Weather Data Monitoring");
        view_weather_table();
        ui_color(COL_MUTED);
        printf("\n  AUTO = automatic feed, MANUAL = entered by admin, STALE = older than %d min\n",
               AUTO_REFRESH_MINUTES);
        ui_reset();
        ui_section("Options");
        ui_menu_item(1, "View detailed weather of a location");
        ui_menu_item(2, "Enter / correct weather data manually");
        ui_menu_item(3, "Refresh all locations from the automatic feed now");
        ui_menu_item(4, "View 7-day forecast of a location");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 4);

        switch (choice) {
        case 0:
            return;
        case 1:
            if ((id = view_select_location("Location", 0)) > 0) {
                view_current_weather(id);
                ui_pause();
            }
            break;
        case 2:
            manual_weather_entry();
            ui_pause();
            break;
        case 3:
            if (input_yes_no("This replaces manual values too. Continue? (y/n): ")) {
                n = weather_sync(time(NULL), 1);
                log_event("WEATHER", "admin refreshed all weather data (%d records)", n);
                ui_msg_success("%d location(s) refreshed from the automatic feed.", n);
                ui_pause();
            }
            break;
        case 4:
            if ((id = view_select_location("Location", 0)) > 0) {
                view_daily_forecast(id);
                ui_pause();
            }
            break;
        }
    }
}

/* =============================================================
 *  3. LOCATION MANAGEMENT
 * ============================================================= */

/* Ask for every field of a location. `edit` = existing values are defaults. */
static int input_location(Location *l, int edit)
{
    char buf[NAME_LEN], p[96];

    for (;;) {
        snprintf(p, sizeof p, "Name%s: ", edit ? " (Enter = keep)" : "");
        input_line(p, buf, sizeof buf);
        if (buf[0] == '\0' && edit) break;
        if (strlen(buf) < 2) { ui_msg_error("Name must have at least 2 characters."); continue; }
        if (location_name_exists(buf, edit ? l->id : 0)) {
            ui_msg_error("A location named '%s' already exists.", buf);
            continue;
        }
        str_copy(l->name, buf, sizeof l->name);
        break;
    }
    snprintf(p, sizeof p, "Region / country%s: ", edit ? " (Enter = keep)" : "");
    input_line(p, buf, sizeof buf);
    if (buf[0] != '\0') str_copy(l->region, buf, sizeof l->region);
    else if (!edit) str_copy(l->region, "Unknown", sizeof l->region);

    if (edit) {
        snprintf(p, sizeof p, "Latitude  [%.4f]: ", l->latitude);
        l->latitude = input_double_default(p, -90.0, 90.0, l->latitude);
        snprintf(p, sizeof p, "Longitude [%.4f]: ", l->longitude);
        l->longitude = input_double_default(p, -180.0, 180.0, l->longitude);
    } else {
        l->latitude  = input_double("Latitude  (-90 to 90)  : ", -90.0, 90.0);
        l->longitude = input_double("Longitude (-180 to 180): ", -180.0, 180.0);
        /* sensible defaults: copy the climate of a nearby known location,
           otherwise estimate it from the latitude */
        {
            double dist;
            int near_id = location_nearest(l->latitude, l->longitude, &dist);
            const Location *near = location_find(near_id);
            double alat = fabs(l->latitude);
            if (near != NULL && dist <= 500.0) {
                l->utc_offset    = near->utc_offset;
                l->avg_temp      = near->avg_temp;
                l->season_range  = near->season_range;
                l->daily_range   = near->daily_range;
                l->avg_humidity  = near->avg_humidity;
                l->rain_tendency = near->rain_tendency;
                ui_msg_info("Climate defaults copied from %s (%.0f km away). "
                            "Press Enter to accept them.", near->name, dist);
            } else {
                l->utc_offset    = floor(l->longitude / 15.0 * 2.0 + 0.5) / 2.0;
                l->avg_temp      = alat <= 20.0 ? 27.0 : 27.0 - 0.45 * (alat - 20.0);
                l->season_range  = 2.0 + 0.4 * alat;
                l->daily_range   = 8.0;
                l->avg_humidity  = 70;
                l->rain_tendency = 50;
                ui_msg_info("Climate defaults estimated from the latitude. "
                            "Press Enter to accept them.");
            }
        }
    }
    snprintf(p, sizeof p, "UTC offset hours [%.1f]: ", l->utc_offset);
    l->utc_offset = input_double_default(p, -12.0, 14.0, l->utc_offset);
    snprintf(p, sizeof p, "Yearly average temperature C [%.1f]: ", l->avg_temp);
    l->avg_temp = input_double_default(p, -30.0, 40.0, l->avg_temp);
    snprintf(p, sizeof p, "Seasonal range C (summer-winter) [%.1f]: ", l->season_range);
    l->season_range = input_double_default(p, 0.0, 50.0, l->season_range);
    snprintf(p, sizeof p, "Day/night range C [%.1f]: ", l->daily_range);
    l->daily_range = input_double_default(p, 0.0, 30.0, l->daily_range);
    snprintf(p, sizeof p, "Average humidity %% [%d]: ", l->avg_humidity);
    l->avg_humidity = input_int_default(p, 0, 100, l->avg_humidity);
    snprintf(p, sizeof p, "Rain tendency 0-100 [%d]: ", l->rain_tendency);
    l->rain_tendency = input_int_default(p, 0, 100, l->rain_tendency);
    return 1;
}

static void location_management(void)
{
    for (;;) {
        int choice, id;
        Location l;

        ui_header("Admin > Location Management");
        view_location_list();
        printf("\n  Total: %d location(s)\n", location_count());
        ui_section("Options");
        ui_menu_item(1, "Add a new location");
        ui_menu_item(2, "Edit a location");
        ui_menu_item(3, "Delete a location");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 3);

        switch (choice) {
        case 0:
            return;
        case 1:
            if (location_count() >= MAX_LOCATIONS) {
                ui_msg_error("Maximum number of locations reached.");
                ui_pause();
                break;
            }
            memset(&l, 0, sizeof l);
            input_location(&l, 0);
            id = location_add(&l);
            if (id > 0) {
                weather_sync(time(NULL), 0);          /* create its first reading */
                log_event("LOCATION", "admin added location %s (id %d)", l.name, id);
                ui_msg_success("Location '%s' added with ID %d.", l.name, id);
            } else {
                ui_msg_error("Could not add the location.");
            }
            ui_pause();
            break;
        case 2:
            if ((id = view_select_location("Location to edit", 0)) == 0) break;
            l = *location_find(id);
            input_location(&l, 1);
            if (location_update(&l) == 0) {
                log_event("LOCATION", "admin edited location %s (id %d)", l.name, id);
                ui_msg_success("Location updated.");
            } else {
                ui_msg_error("Could not update the location.");
            }
            ui_pause();
            break;
        case 3:
            if ((id = view_select_location("Location to delete", 0)) == 0) break;
            if (location_count() <= 1) {
                ui_msg_error("At least one location must remain.");
                ui_pause();
                break;
            }
            str_copy(l.name, location_name(id), sizeof l.name);
            if (input_yes_no("Delete it with its weather data, alerts and favourites? (y/n): ")) {
                int replacement;
                location_delete(id);
                weather_remove(id);
                alert_remove_location(id);
                favorites_remove_location(id);
                replacement = location_at(0)->id;
                user_clear_home_location(id, replacement);
                log_event("LOCATION", "admin deleted location %s (id %d)", l.name, id);
                ui_msg_success("Location '%s' deleted. Affected users now have home: %s.",
                               l.name, location_name(replacement));
            }
            ui_pause();
            break;
        }
    }
}

/* =============================================================
 *  4. ALERT MANAGEMENT
 * ============================================================= */

static void print_alert_table(void)
{
    time_t now = time(NULL);
    int i;

    if (alert_count() == 0) {
        ui_msg_info("No alerts have been issued yet.");
        return;
    }
    ui_color(COL_TITLE);
    printf("  %-3s %-8s %-8s %-12s %-8s %-16s %s\n",
           "ID", "Severity", "Type", "Location", "Status", "Expires", "Message");
    ui_reset();
    for (i = 0; i < alert_count(); i++) {
        const Alert *a = alert_at(i);
        const char *status = !a->active ? "OFF" : alert_is_current(a, now) ? "ACTIVE" : "EXPIRED";
        printf("  %-3d %-8s %-8.8s %-12.12s ", a->id, severity_name(a->severity), a->type,
               a->location_id == ALERT_ALL_LOCATIONS ? "All" : location_name(a->location_id));
        ui_color(strcmp(status, "ACTIVE") == 0 ? COL_SUCCESS : COL_MUTED);
        printf("%-8s", status);
        ui_reset();
        printf(" %-16.16s %.30s%s\n", a->expires, a->message, strlen(a->message) > 30 ? "..." : "");
    }
}

static void create_alert(void)
{
    static const char *TYPES[] = { "HEAT", "COLD", "RAIN", "FLOOD", "STORM", "CYCLONE",
                                   "THUNDER", "FOG", "OTHER" };
    const int n_types = (int)(sizeof TYPES / sizeof TYPES[0]);
    Alert a;
    int i, t, hours, scope;
    time_t now = time(NULL);

    memset(&a, 0, sizeof a);
    ui_section("New alert");
    ui_menu_item(1, "One location");
    ui_menu_item(2, "All locations");
    scope = input_int("Alert for (0 = cancel): ", 0, 2);
    if (scope == 0) return;
    if (scope == 1) {
        a.location_id = view_select_location("Location", 0);
        if (a.location_id == 0) return;
    } else {
        a.location_id = ALERT_ALL_LOCATIONS;
    }

    printf("\n");
    for (i = 0; i < n_types; i++) ui_menu_item(i + 1, TYPES[i]);
    t = input_int("Alert type: ", 1, n_types);
    str_copy(a.type, TYPES[t - 1], sizeof a.type);

    printf("\n");
    ui_menu_item(1, "ADVISORY - be aware");
    ui_menu_item(2, "WATCH    - be prepared");
    ui_menu_item(3, "WARNING  - take action now");
    a.severity = input_int("Severity: ", 1, 3);

    for (;;) {
        input_line("Message: ", a.message, sizeof a.message);
        if (strlen(a.message) >= 5) break;
        ui_msg_error("Please write a message (at least 5 characters).");
    }
    hours = input_int_default("Valid for how many hours? [24]: ", 1, 720, 24);
    format_time(now, a.issued, sizeof a.issued);
    format_time(now + (time_t)hours * 3600, a.expires, sizeof a.expires);
    a.active = 1;

    if (alert_add(&a) > 0) {
        log_event("ALERT", "admin issued %s %s alert for %s", severity_name(a.severity), a.type,
                  a.location_id ? location_name(a.location_id) : "all locations");
        ui_msg_success("Alert #%d issued. Users will see it until %s.", a.id, a.expires);
    } else {
        ui_msg_error("Could not save the alert.");
    }
}

/* Scan every location for automatic alerts; optionally publish them. */
static void scan_alerts(void)
{
    Alert found[MAX_ALERTS];
    int n = 0, i;
    time_t now = time(NULL);

    weather_sync(now, 0);
    for (i = 0; i < location_count() && n < MAX_ALERTS; i++) {
        const Location *l = location_at(i);
        const Weather *w = weather_get(l->id);
        HourlyForecast h[HOURLY_COUNT];
        if (w == NULL) continue;
        forecast_hourly(l, w, now, h, HOURLY_COUNT);
        n += alert_detect(l, w, h, HOURLY_COUNT, found + n, MAX_ALERTS - n);
    }
    ui_section("Automatically detected alerts (next 24 hours)");
    if (n == 0) {
        ui_msg_success("No dangerous weather detected at any location.");
        return;
    }
    for (i = 0; i < n; i++) view_print_alert(&found[i]);
    printf("\n");
    if (input_yes_no("Publish these as official alerts valid for 24 hours? (y/n): ")) {
        int published = 0;
        for (i = 0; i < n; i++) {
            Alert a = found[i];
            format_time(now, a.issued, sizeof a.issued);
            format_time(now + 24 * 3600, a.expires, sizeof a.expires);
            if (alert_add(&a) > 0) published++;
        }
        log_event("ALERT", "admin published %d automatic alert(s)", published);
        ui_msg_success("%d alert(s) published.", published);
    }
}

static void alert_management(void)
{
    for (;;) {
        int choice, id;

        ui_header("Admin > Alert Management");
        print_alert_table();
        ui_section("Options");
        ui_menu_item(1, "Issue a new alert");
        ui_menu_item(2, "Activate / deactivate an alert");
        ui_menu_item(3, "Delete an alert");
        ui_menu_item(4, "Scan all locations for dangerous weather");
        ui_menu_item(5, "View full message of an alert");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 5);

        switch (choice) {
        case 0:
            return;
        case 1:
            create_alert();
            ui_pause();
            break;
        case 2:
            id = input_int("Alert ID (0 = cancel): ", 0, 1000000);
            if (id == 0) break;
            if (alert_toggle(id) == 0) {
                ui_msg_success("Alert #%d is now %s.", id, alert_find(id)->active ? "ON" : "OFF");
                log_event("ALERT", "admin toggled alert #%d", id);
            } else {
                ui_msg_error("No alert with ID %d.", id);
            }
            ui_pause();
            break;
        case 3:
            id = input_int("Alert ID (0 = cancel): ", 0, 1000000);
            if (id == 0) break;
            if (alert_find(id) == NULL) {
                ui_msg_error("No alert with ID %d.", id);
            } else if (input_yes_no("Delete this alert? (y/n): ")) {
                alert_delete(id);
                log_event("ALERT", "admin deleted alert #%d", id);
                ui_msg_success("Alert #%d deleted.", id);
            }
            ui_pause();
            break;
        case 4:
            scan_alerts();
            ui_pause();
            break;
        case 5:
            id = input_int("Alert ID (0 = cancel): ", 0, 1000000);
            if (id == 0) break;
            if (alert_find(id) != NULL) view_print_alert(alert_find(id));
            else ui_msg_error("No alert with ID %d.", id);
            ui_pause();
            break;
        }
    }
}

/* =============================================================
 *  5. SYSTEM MONITORING
 * ============================================================= */

static void system_monitoring(void)
{
    static const char *FILES[] = { FILE_USERS, FILE_ADMIN, FILE_LOCATIONS, FILE_WEATHER,
                                   FILE_ALERTS, FILE_FAVORITES, FILE_HISTORY,
                                   FILE_DEVICE_GPS, FILE_LOG };
    char lines[15][LINE_LEN];
    int ids[5], i, n, active_users = 0, active_alerts = 0, manual = 0, stale = 0;
    long counts[5], max_count;
    time_t now = time(NULL);

    ui_header("Admin > System Monitoring");

    for (i = 0; i < user_count(); i++) if (user_at(i)->active) active_users++;
    for (i = 0; i < alert_count(); i++) if (alert_is_current(alert_at(i), now)) active_alerts++;
    for (i = 0; i < weather_count(); i++) {
        const Weather *w = weather_at(i);
        if (w->source == SOURCE_MANUAL) manual++;
        else if (weather_age_minutes(w, now) >= AUTO_REFRESH_MINUTES) stale++;
    }

    ui_color(COL_TITLE);
    printf("  SYSTEM STATISTICS\n");
    ui_reset();
    printf("  Users            : %-5d (active %d, blocked %d)\n", user_count(), active_users,
           user_count() - active_users);
    printf("  Locations        : %-5d Weather records: %d (manual %d, stale %d)\n",
           location_count(), weather_count(), manual, stale);
    printf("  Alerts           : %-5d (currently active %d)\n", alert_count(), active_alerts);
    printf("  Searches         : %-5ld Favourites saved: %ld\n", history_total(), favorites_total());
    printf("  Logins           : %-5ld Failed logins: %ld\n", log_count_type("LOGIN"),
           log_count_type("LOGIN_FAIL"));
    printf("  Data folder      : %s    Reports folder: %s\n", get_data_dir(), get_report_dir());

    ui_section("Most searched locations");
    n = history_top_locations(ids, counts, 5);
    max_count = n > 0 ? counts[0] : 1;
    if (n == 0) printf("  No searches yet.\n");
    for (i = 0; i < n; i++) {
        printf("  %d. %-14s %4ld ", i + 1, location_name(ids[i]), counts[i]);
        ui_color(COL_ACCENT);
        ui_bar((double)counts[i] / (double)max_count, 30, '#');
        ui_reset();
        printf("\n");
    }

    ui_section("Data files");
    for (i = 0; i < (int)(sizeof FILES / sizeof FILES[0]); i++) {
        char path[PATH_LEN];
        long size;
        data_path(FILES[i], path, sizeof path);
        size = file_size(path);
        printf("  %-22s ", FILES[i]);
        if (size >= 0) {
            ui_color(COL_SUCCESS);
            printf("OK      ");
            ui_reset();
            printf("%8ld bytes\n", size);
        } else {
            ui_color(COL_WARNING);
            printf("missing (created when needed)\n");
            ui_reset();
        }
    }

    ui_section("Recent activity (system.log)");
    n = log_tail(lines, 12);
    if (n == 0) printf("  No activity recorded yet.\n");
    for (i = 0; i < n; i++)              /* skip the year to save space */
        printf("  %.76s\n", strlen(lines[i]) > 5 ? lines[i] + 5 : lines[i]);
}

/* =============================================================
 *  6. REPORTS
 * ============================================================= */

static void reports_menu(void)
{
    for (;;) {
        char path[PATH_LEN];
        int choice, id, r = -1;

        ui_header("Admin > Weather Report Generation");
        ui_msg_info("Reports are saved as text files in the '%s' folder.", get_report_dir());
        printf("\n");
        ui_menu_item(1, "Weather summary report (all locations + statistics)");
        ui_menu_item(2, "Detailed forecast report of one location");
        ui_menu_item(3, "User activity report");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 3);

        if (choice == 0) return;
        if (choice == 1) r = report_weather_summary(path, sizeof path);
        if (choice == 2) {
            id = view_select_location("Location", 0);
            if (id == 0) continue;
            r = report_location_forecast(id, path, sizeof path);
        }
        if (choice == 3) r = report_user_activity(path, sizeof path);

        if (r == 0) {
            log_event("REPORT", "admin generated %s", path);
            ui_msg_success("Report saved: %s", path);
            if (input_yes_no("Show the report now? (y/n): ")) {
                printf("\n");
                report_print_file(path);
            }
        } else {
            ui_msg_error("Could not create the report (check the reports folder).");
        }
        ui_pause();
    }
}

/* =============================================================
 *  7. CHANGE ADMIN PASSWORD
 * ============================================================= */

static void change_admin_password(void)
{
    char cur[PASSWORD_LEN], p1[PASSWORD_LEN], p2[PASSWORD_LEN], err[128];

    ui_header("Admin > Change Password");
    input_password("Current password: ", cur, sizeof cur);
    if (!admin_login(admin_username(), cur)) {
        ui_msg_error("Current password is wrong.");
        return;
    }
    input_password("New password: ", p1, sizeof p1);
    if (!validate_password(p1, err, sizeof err)) {
        ui_msg_error("%s", err);
        return;
    }
    input_password("Confirm new password: ", p2, sizeof p2);
    if (strcmp(p1, p2) != 0) {
        ui_msg_error("Passwords do not match.");
        return;
    }
    if (admin_change_password(p1) == 0) {
        log_event("ADMIN", "administrator password changed");
        ui_msg_success("Administrator password changed.");
    } else {
        ui_msg_error("Could not save the new password.");
    }
}

/* =============================================================
 *  ADMIN DASHBOARD
 * ============================================================= */

void admin_session_run(void)
{
    for (;;) {
        int choice, i, active_alerts = 0;
        time_t now = time(NULL);

        for (i = 0; i < alert_count(); i++) if (alert_is_current(alert_at(i), now)) active_alerts++;
        ui_set_status("Administrator panel");
        ui_header("Administrator Dashboard");
        ui_color(COL_INFO);
        printf("  Users: %d   |   Locations: %d   |   Active alerts: %d   |   Searches: %ld\n\n",
               user_count(), location_count(), active_alerts, history_total());
        ui_reset();
        ui_menu_item(1, "User Management");
        ui_menu_item(2, "Weather Data Monitoring");
        ui_menu_item(3, "Location Management");
        ui_menu_item(4, "Alert Management");
        ui_menu_item(5, "System Monitoring");
        ui_menu_item(6, "Generate Weather Reports");
        ui_menu_item(7, "Change Admin Password");
        ui_menu_item(0, "Logout");
        printf("\n");
        choice = input_int("Enter your choice: ", 0, 7);

        switch (choice) {
        case 1: user_management();      break;
        case 2: weather_monitoring();   break;
        case 3: location_management();  break;
        case 4: alert_management();     break;
        case 5: system_monitoring(); ui_pause(); break;
        case 6: reports_menu();         break;
        case 7: change_admin_password(); ui_pause(); break;
        default:
            log_event("LOGOUT", "administrator logged out");
            ui_set_status("");
            return;
        }
    }
}
