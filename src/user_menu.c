/*
 * user_menu.c
 * ---------------------------------------------------------------
 * USER MODULE - everything a registered user can do after login:
 * search locations, current weather, hourly & daily forecasts,
 * smart advisory, comparison, favourites, search history, alerts,
 * automatic location detection and personal settings (theme,
 * home location, password, profile).
 */
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "views.h"
#include "user.h"
#include "userdata.h"
#include "location.h"
#include "weather.h"
#include "detect.h"
#include "logger.h"
#include "ui.h"
#include "utils.h"

typedef struct {
    char username[USERNAME_LEN];
    int  selected;                 /* currently selected location id */
} Session;

/* =============================================================
 *  HELPERS
 * ============================================================= */

static User *session_user(const Session *s) { return user_find(s->username); }

static void update_status(const Session *s)
{
    char status[128];
    const User *u = session_user(s);
    snprintf(status, sizeof status, "User: %s  |  Location: %s  |  Theme: %s",
             s->username, location_name(s->selected),
             ui_theme_name(u ? u->theme : THEME_DARK));
    ui_set_status(status);
}

/* Ask for a location, remember it as selected and save it in history. */
static int pick_location(Session *s, const char *prompt)
{
    int id = view_select_location(prompt, s->selected);
    if (id > 0) {
        s->selected = id;
        history_add(s->username, id);
        update_status(s);
    }
    return id;
}

/* Home location + favourites + selected location, without duplicates. */
static int saved_location_ids(const Session *s, int ids[], int max)
{
    int fav[MAX_FAVORITES];
    int n = 0, i, j, nf;
    const User *u = session_user(s);
    int candidates[MAX_FAVORITES + 2];
    int nc = 0;

    if (u && location_find(u->home_location)) candidates[nc++] = u->home_location;
    if (location_find(s->selected)) candidates[nc++] = s->selected;
    nf = favorites_list(s->username, fav, MAX_FAVORITES);
    for (i = 0; i < nf; i++) candidates[nc++] = fav[i];

    for (i = 0; i < nc && n < max; i++) {
        int dup = 0;
        if (location_find(candidates[i]) == NULL) continue;
        for (j = 0; j < n; j++) if (ids[j] == candidates[i]) dup = 1;
        if (!dup) ids[n++] = candidates[i];
    }
    return n;
}

static void quick_glance(const Session *s)
{
    const Weather *w = weather_get(s->selected);
    if (w == NULL) return;
    ui_color(COL_INFO);
    printf("  Now in %s: %.1f C, %s, humidity %d%%, rain chance %d%%\n",
           location_name(s->selected), w->temperature, condition_name(w->condition),
           w->humidity, w->rain_chance);
    ui_reset();
}

/* After choosing a location: show its weather and offer more actions. */
static void location_actions(Session *s, int id)
{
    for (;;) {
        int fav = favorites_is(s->username, id);
        int choice;

        view_current_weather(id);
        ui_section("What next?");
        ui_menu_item(1, "Hourly forecast (next 24 hours)");
        ui_menu_item(2, "Daily forecast (next 7 days)");
        ui_menu_item_tag(3, "Smart advisory & best day planner", "[Unique]");
        ui_menu_item(4, fav ? "Remove from favourites" : "Add to favourites");
        ui_menu_item(0, "Back to dashboard");
        choice = input_int("Your choice: ", 0, 4);

        switch (choice) {
        case 1: view_hourly_forecast(id); ui_pause(); break;
        case 2: view_daily_forecast(id);  ui_pause(); break;
        case 3: view_advisory(id);        ui_pause(); break;
        case 4: {
            int r;
            if (fav) {
                favorites_remove(s->username, id);
                ui_msg_success("%s removed from favourites.", location_name(id));
            } else if ((r = favorites_add(s->username, id)) == 0) {
                ui_msg_success("%s added to favourites.", location_name(id));
            } else if (r == -1) {
                ui_msg_error("You can save at most %d favourites. Remove one first.",
                             MAX_FAVORITES);
            } else {
                ui_msg_error("Could not save the favourite.");
            }
            ui_pause();
            break;
        }
        default:
            return;
        }
    }
}

/* =============================================================
 *  FAVOURITES
 * ============================================================= */

static void favorites_menu(Session *s)
{
    for (;;) {
        int ids[MAX_FAVORITES];
        int n, i, choice;

        ui_header("Favourite Locations");
        n = favorites_list(s->username, ids, MAX_FAVORITES);
        weather_sync(time(NULL), 0);

        if (n == 0) {
            ui_msg_info("You have no favourite locations yet.");
        } else {
            ui_color(COL_TITLE);
            printf("  %-3s %-14s %7s  %-14s %5s %5s\n", "#", "Location", "Temp", "Condition",
                   "Hum", "Rain");
            ui_reset();
            for (i = 0; i < n; i++) {
                const Weather *w = weather_get(ids[i]);
                printf("  %-3d %-14.14s ", i + 1, location_name(ids[i]));
                if (w)
                    printf("%6.1fC  %-14s %4d%% %4d%%\n", w->temperature,
                           condition_name(w->condition), w->humidity, w->rain_chance);
                else
                    printf("   (no data)\n");
            }
        }
        printf("  (%d of %d slots used)\n", n, MAX_FAVORITES);

        ui_section("Options");
        ui_menu_item(1, "Open a favourite (weather & forecast)");
        ui_menu_item(2, "Add a location to favourites");
        ui_menu_item(3, "Remove a favourite");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 3);

        if (choice == 0) return;
        if (choice == 1 || choice == 3) {
            int k;
            if (n == 0) { ui_msg_error("The list is empty."); ui_pause(); continue; }
            k = input_int("Favourite number (0 = cancel): ", 0, n);
            if (k == 0) continue;
            if (choice == 1) {
                s->selected = ids[k - 1];
                history_add(s->username, ids[k - 1]);
                update_status(s);
                location_actions(s, ids[k - 1]);
            } else {
                favorites_remove(s->username, ids[k - 1]);
                log_event("FAVORITE", "user '%s' removed %s", s->username, location_name(ids[k - 1]));
                ui_msg_success("%s removed from favourites.", location_name(ids[k - 1]));
                ui_pause();
            }
        } else if (choice == 2) {
            int id = view_select_location("Location to add", s->selected);
            int r;
            if (id == 0) continue;
            r = favorites_add(s->username, id);
            if (r == 0) {
                log_event("FAVORITE", "user '%s' added %s", s->username, location_name(id));
                ui_msg_success("%s added to favourites.", location_name(id));
            } else if (r == 1) {
                ui_msg_warning("%s is already a favourite.", location_name(id));
            } else if (r == -1) {
                ui_msg_error("You can save at most %d favourites.", MAX_FAVORITES);
            } else {
                ui_msg_error("Could not save the favourite.");
            }
            ui_pause();
        }
    }
}

/* =============================================================
 *  SEARCH HISTORY
 * ============================================================= */

static void history_menu(Session *s)
{
    for (;;) {
        HistoryEntry h[MAX_HISTORY_SHOW];
        int n, i, choice, most;

        ui_header("Search History");
        n = history_list(s->username, h, MAX_HISTORY_SHOW);
        if (n == 0) {
            ui_msg_info("Your search history is empty.");
        } else {
            ui_color(COL_TITLE);
            printf("  %-3s %-20s %s\n", "#", "Date & time", "Location");
            ui_reset();
            for (i = 0; i < n; i++)
                printf("  %-3d %-20s %s\n", i + 1, h[i].time, location_name(h[i].location_id));
            printf("\n  Total searches: %ld", history_count_user(s->username));
            most = history_most_searched_by(s->username);
            if (most > 0) printf("   |   Most searched: %s", location_name(most));
            printf("\n");
        }

        ui_section("Options");
        ui_menu_item(1, "Open a location from history");
        ui_menu_item(2, "Clear my search history");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 2);

        if (choice == 0) return;
        if (n == 0) { ui_msg_error("The history is empty."); ui_pause(); continue; }
        if (choice == 1) {
            int k = input_int("Entry number (0 = cancel): ", 0, n);
            if (k == 0) continue;
            if (location_find(h[k - 1].location_id) == NULL) {
                ui_msg_error("That location has been removed by the administrator.");
                ui_pause();
                continue;
            }
            s->selected = h[k - 1].location_id;
            history_add(s->username, s->selected);
            update_status(s);
            location_actions(s, s->selected);
        } else if (input_yes_no("Delete your whole search history? (y/n): ")) {
            history_clear(s->username);
            log_event("HISTORY", "user '%s' cleared search history", s->username);
            ui_msg_success("Search history cleared.");
            ui_pause();
        }
    }
}

/* =============================================================
 *  ALERTS & AUTO DETECTION
 * ============================================================= */

static void alerts_screen(Session *s)
{
    int ids[MAX_FAVORITES + 2];
    int n = saved_location_ids(s, ids, MAX_FAVORITES + 2), i;

    ui_header("Weather Alerts");
    ui_color(COL_MUTED);
    printf("  Checking your home, selected and favourite locations:");
    for (i = 0; i < n; i++) printf("%s %s", i ? "," : "", location_name(ids[i]));
    printf("\n\n");
    ui_reset();
    view_alerts(ids, n);
    ui_pause();
}

static void detect_screen(Session *s)
{
    DetectResult r;
    const User *u = session_user(s);
    const Location *l;

    ui_header("Automatic Location Detection");
    printf("  Detecting your location...\n\n");
    if (detect_location(u ? u->home_location : 0, &r) == 0) {
        ui_msg_error("Location could not be detected. Please search manually.");
        ui_pause();
        return;
    }
    l = location_find(r.location_id);
    printf("  Method used   : %s\n", detect_method_name(r.method));
    if (r.distance_km >= 0) {
        printf("  Device GPS    : %.4f, %.4f\n", r.latitude, r.longitude);
        printf("  Nearest city  : %s (%.1f km away)\n", l->name, r.distance_km);
    } else {
        printf("  Detected city : %s\n", l->name);
    }
    ui_msg_success("Your location is set to %s.", l->name);
    s->selected = l->id;
    history_add(s->username, l->id);
    log_event("DETECT", "user '%s' detected location %s", s->username, l->name);
    update_status(s);
    printf("\n");
    if (input_yes_no("Show the weather for this location? (y/n): "))
        location_actions(s, l->id);
}

/* =============================================================
 *  SETTINGS
 * ============================================================= */

static void settings_menu(Session *s)
{
    for (;;) {
        User *u = session_user(s);
        User copy;
        char err[128], buf[PASSWORD_LEN], buf2[PASSWORD_LEN], name[NAME_LEN];
        int fav_ids[MAX_FAVORITES];
        int choice;

        if (u == NULL) return;
        ui_header("Settings & Profile");
        printf("  Username      : %s\n", u->username);
        printf("  Full name     : %s\n", u->full_name);
        printf("  Home location : %s\n", location_name(u->home_location));
        printf("  Display theme : %s\n", ui_theme_name(u->theme));
        printf("  Member since  : %s\n", u->created);
        printf("  Favourites    : %d    Searches: %ld\n",
               favorites_list(u->username, fav_ids, MAX_FAVORITES),
               history_count_user(u->username));

        ui_section("Options");
        ui_menu_item(1, "Change display theme (Dark / Light / Classic)");
        ui_menu_item(2, "Change home location");
        ui_menu_item(3, "Change full name");
        ui_menu_item(4, "Change password");
        ui_menu_item(5, "Set device GPS position (simulates moving the device)");
        ui_menu_item(0, "Back");
        choice = input_int("Your choice: ", 0, 5);
        copy = *u;

        switch (choice) {
        case 0:
            return;
        case 1: {
            int t;
            printf("\n");
            for (t = 0; t < THEME_COUNT; t++) ui_menu_item(t + 1, ui_theme_name(t));
            t = input_int("Choose theme (0 = cancel): ", 0, THEME_COUNT);
            if (t == 0) break;
            copy.theme = t - 1;
            user_update(&copy);
            ui_set_theme(copy.theme);
            update_status(s);
            ui_clear();
            ui_msg_success("Theme changed to %s.", ui_theme_name(copy.theme));
            ui_pause();
            break;
        }
        case 2: {
            int id = view_select_location("New home location", u->home_location);
            if (id == 0) break;
            copy.home_location = id;
            user_update(&copy);
            ui_msg_success("Home location changed to %s.", location_name(id));
            ui_pause();
            break;
        }
        case 3:
            input_line("New full name: ", name, sizeof name);
            if (!validate_full_name(name, err, sizeof err)) {
                ui_msg_error("%s", err);
            } else {
                str_copy(copy.full_name, name, sizeof copy.full_name);
                user_update(&copy);
                ui_msg_success("Name updated.");
            }
            ui_pause();
            break;
        case 4:
            input_password("Current password: ", buf, sizeof buf);
            if (!user_check_password(u->username, buf)) {
                ui_msg_error("Current password is wrong.");
                ui_pause();
                break;
            }
            input_password("New password: ", buf, sizeof buf);
            if (!validate_password(buf, err, sizeof err)) {
                ui_msg_error("%s", err);
                ui_pause();
                break;
            }
            input_password("Confirm new password: ", buf2, sizeof buf2);
            if (strcmp(buf, buf2) != 0) {
                ui_msg_error("Passwords do not match.");
            } else {
                user_change_password(u->username, buf);
                log_event("PASSWORD", "user '%s' changed password", u->username);
                ui_msg_success("Password changed successfully.");
            }
            ui_pause();
            break;
        case 5: {
            double lat, lon;
            ui_msg_info("Enter the coordinates the device's GPS should report.");
            lat = input_double("Latitude  (-90 to 90)  : ", -90.0, 90.0);
            lon = input_double("Longitude (-180 to 180): ", -180.0, 180.0);
            if (detect_save_device_coords(lat, lon) == 0)
                ui_msg_success("Device position saved. Use 'Detect My Location' to test it.");
            else
                ui_msg_error("Could not save the position.");
            ui_pause();
            break;
        }
        }
    }
}

/* =============================================================
 *  DASHBOARD
 * ============================================================= */

void user_session_run(const char *username)
{
    Session s;
    User *u = user_find(username);
    int ids[MAX_FAVORITES + 2];
    Alert alerts[40];
    int n, n_alerts, choice;

    if (u == NULL) return;
    memset(&s, 0, sizeof s);
    str_copy(s.username, u->username, sizeof s.username);
    s.selected = location_find(u->home_location) ? u->home_location
               : (location_count() > 0 ? location_at(0)->id : 0);

    ui_set_theme(u->theme);
    update_status(&s);
    weather_sync(time(NULL), 0);

    /* welcome screen with alert notification */
    ui_header("Login Successful");
    ui_msg_success("Welcome back, %s!", u->full_name);
    n = saved_location_ids(&s, ids, MAX_FAVORITES + 2);
    n_alerts = view_collect_alerts(ids, n, alerts, 40);
    if (n_alerts > 0) {
        int i;
        ui_msg_warning("NOTIFICATION: %d weather alert(s) for your saved locations:", n_alerts);
        for (i = 0; i < n_alerts && i < 3; i++) view_print_alert(&alerts[i]);
        if (n_alerts > 3) printf("   ... see 'Weather Alerts' for all of them.\n");
    } else {
        ui_msg_info("No weather alerts for your saved locations.");
    }
    ui_pause();

    for (;;) {
        u = session_user(&s);
        if (u == NULL) return;
        update_status(&s);
        ui_header("User Dashboard");
        quick_glance(&s);
        printf("\n");
        ui_menu_item(1,  "Search Location & Current Weather");
        ui_menu_item(2,  "Hourly Forecast (next 24 hours)");
        ui_menu_item(3,  "Daily Forecast (next 7 days)");
        ui_menu_item_tag(4, "Smart Advisory & Best Day Planner", "[Unique]");
        ui_menu_item(5,  "Compare Two Locations");
        ui_menu_item(6,  "Favourite Locations");
        ui_menu_item(7,  "Search History");
        ui_menu_item(8,  "Weather Alerts");
        ui_menu_item(9,  "Detect My Location Automatically");
        ui_menu_item(10, "Settings (Theme / Profile / Password)");
        ui_menu_item(0,  "Logout");
        printf("\n");
        choice = input_int("Enter your choice: ", 0, 10);

        switch (choice) {
        case 1:
            ui_header("Search Location");
            if (pick_location(&s, "Enter location name") > 0) {
                log_event("SEARCH", "user '%s' searched %s", s.username, location_name(s.selected));
                location_actions(&s, s.selected);
            }
            break;
        case 2:
            ui_header("Hourly Forecast");
            if (pick_location(&s, "Location") > 0) { view_hourly_forecast(s.selected); ui_pause(); }
            break;
        case 3:
            ui_header("Daily Forecast");
            if (pick_location(&s, "Location") > 0) { view_daily_forecast(s.selected); ui_pause(); }
            break;
        case 4:
            ui_header("Smart Advisory & Best Day Planner");
            if (pick_location(&s, "Location") > 0) { view_advisory(s.selected); ui_pause(); }
            break;
        case 5: {
            int a, b;
            ui_header("Compare Two Locations");
            a = view_select_location("First location", s.selected);
            if (a == 0) break;
            b = view_select_location("Second location", 0);
            if (b == 0) break;
            if (a == b) { ui_msg_warning("Please choose two different locations."); ui_pause(); break; }
            history_add(s.username, a);
            history_add(s.username, b);
            view_compare(a, b);
            ui_pause();
            break;
        }
        case 6:  favorites_menu(&s); break;
        case 7:  history_menu(&s);   break;
        case 8:  alerts_screen(&s);  break;
        case 9:  detect_screen(&s);  break;
        case 10: settings_menu(&s);  break;
        default:
            log_event("LOGOUT", "user '%s' logged out", s.username);
            ui_set_theme(THEME_DARK);
            ui_set_status("");
            return;
        }
    }
}
