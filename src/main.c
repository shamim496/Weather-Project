/*
 * main.c
 * ---------------------------------------------------------------
 * WEATHER FORECASTING SYSTEM
 * Software Development Lab I - Project
 * Department of CSE, Pundra University of Science & Technology
 *
 * Entry point: loads all data files and shows the main menu
 * (user login, registration, admin login, about, exit).
 *
 * Usage:  weather [--data DIR] [--reports DIR] [--help] [--version]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ui.h"
#include "utils.h"
#include "user.h"
#include "location.h"
#include "weather.h"
#include "alert.h"
#include "views.h"
#include "menus.h"
#include "logger.h"

/* =============================================================
 *  START-UP
 * ============================================================= */

static void print_usage(const char *program)
{
    printf("%s %s\n\n", APP_NAME, APP_VERSION);
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  --data DIR      folder for data files    (default: %s)\n", DEFAULT_DATA_DIR);
    printf("  --reports DIR   folder for reports       (default: %s)\n", DEFAULT_REPORT_DIR);
    printf("  --help          show this help\n");
    printf("  --version       show the version\n\n");
    printf("Environment:\n");
    printf("  NO_COLOR=1      disable colours\n");
    printf("  WFS_LAT/WFS_LON device GPS position for automatic location detection\n");
}

static int parse_arguments(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            set_data_dir(argv[++i]);
        } else if (strcmp(argv[i], "--reports") == 0 && i + 1 < argc) {
            set_report_dir(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("%s %s\n", APP_NAME, APP_VERSION);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s (try --help)\n", argv[i]);
            return -1;
        }
    }
    return 1;
}

/* Load every data file. Missing files are created with default content. */
static int load_all_data(void)
{
    if (ensure_dir(get_data_dir()) != 0) {
        fprintf(stderr, "Cannot create data folder '%s'.\n", get_data_dir());
        return -1;
    }
    ensure_dir(get_report_dir());

    location_load();
    weather_load();
    user_load();
    alert_load();
    admin_ensure_default();
    weather_sync(time(NULL), 0);       /* fresh readings for the automatic feed */
    return 0;
}

/* =============================================================
 *  MAIN MENU ACTIONS
 * ============================================================= */

static void do_login(void)
{
    char username[USERNAME_LEN], password[PASSWORD_LEN];
    int attempt;

    ui_header("User Login");
    for (attempt = 1; attempt <= MAX_LOGIN_TRIES; attempt++) {
        AuthResult r;

        input_line("Username: ", username, sizeof username);
        if (username[0] == '\0') return;
        input_password("Password: ", password, sizeof password);

        r = user_login(username, password);
        if (r == AUTH_OK) {
            const User *u = user_find(username);
            log_event("LOGIN", "user '%s' logged in", u->username);
            user_session_run(u->username);
            return;
        }
        if (r == AUTH_BLOCKED) {
            log_event("LOGIN_FAIL", "blocked user '%s' tried to log in", username);
            ui_msg_error("This account is blocked. Please contact the administrator.");
            ui_pause();
            return;
        }
        log_event("LOGIN_FAIL", "wrong username or password for '%s'", username);
        ui_msg_error("Wrong username or password. (%d of %d attempts used)", attempt,
                     MAX_LOGIN_TRIES);
    }
    ui_msg_warning("Too many failed attempts. Returning to the main menu.");
    ui_pause();
}

static void do_register(void)
{
    char username[USERNAME_LEN], name[NAME_LEN];
    char pass[PASSWORD_LEN], confirm[PASSWORD_LEN], err[128];
    int home, r;

    ui_header("Create a New Account");
    ui_color(COL_MUTED);
    printf("  Username: 3-20 letters/digits/_ , starts with a letter.\n");
    printf("  Password: at least %d characters with letters and digits.\n\n", MIN_PASSWORD_LEN);
    ui_reset();

    for (;;) {
        input_line("Choose a username (empty = cancel): ", username, sizeof username);
        if (username[0] == '\0') return;
        if (!validate_username(username, err, sizeof err)) { ui_msg_error("%s", err); continue; }
        if (user_find(username) != NULL) {
            ui_msg_error("Username '%s' is already taken.", username);
            continue;
        }
        break;
    }
    for (;;) {
        input_line("Full name: ", name, sizeof name);
        if (validate_full_name(name, err, sizeof err)) break;
        ui_msg_error("%s", err);
    }
    for (;;) {
        input_password("Password: ", pass, sizeof pass);
        if (!validate_password(pass, err, sizeof err)) { ui_msg_error("%s", err); continue; }
        input_password("Confirm password: ", confirm, sizeof confirm);
        if (strcmp(pass, confirm) == 0) break;
        ui_msg_error("Passwords do not match. Try again.");
    }
    printf("\n");
    ui_msg_info("Choose your home location (weather is shown for it after login).");
    home = view_select_location("Home location", location_count() > 0 ? location_at(0)->id : 0);
    if (home == 0) {
        ui_msg_warning("Registration cancelled.");
        ui_pause();
        return;
    }

    r = user_register(username, pass, name, home);
    if (r == 0) {
        log_event("REGISTER", "new user '%s' registered", username);
        ui_msg_success("Account created! You can now log in as '%s'.", username);
        if (input_yes_no("Log in now? (y/n): ")) {
            user_login(username, pass);
            log_event("LOGIN", "user '%s' logged in", username);
            user_session_run(username);
            return;
        }
    } else if (r == -2) {
        ui_msg_error("User limit reached (%d).", MAX_USERS);
    } else {
        ui_msg_error("Could not save the account.");
    }
    ui_pause();
}

static void do_admin_login(void)
{
    char username[USERNAME_LEN], password[PASSWORD_LEN];
    int attempt;

    ui_header("Administrator Login");
    for (attempt = 1; attempt <= MAX_LOGIN_TRIES; attempt++) {
        input_line("Admin username: ", username, sizeof username);
        if (username[0] == '\0') return;
        input_password("Password: ", password, sizeof password);
        if (admin_login(username, password)) {
            log_event("LOGIN", "administrator logged in");
            admin_session_run();
            return;
        }
        log_event("LOGIN_FAIL", "wrong admin credentials (attempt %d)", attempt);
        ui_msg_error("Wrong admin username or password. (%d of %d attempts used)",
                     attempt, MAX_LOGIN_TRIES);
    }
    ui_msg_warning("Too many failed attempts.");
    ui_pause();
}

static void do_about(void)
{
    ui_header("About This System");
    printf("  %s, version %s\n\n", APP_NAME, APP_VERSION);
    printf("  A console application written in C that provides current weather,\n");
    printf("  hourly and 7-day forecasts, alerts and smart advice for many locations.\n");
    printf("  All data is stored in text files using C file handling.\n\n");
    ui_color(COL_TITLE);
    printf("  User module\n");
    ui_reset();
    printf("   - Registration and secure login (hashed passwords)\n");
    printf("   - Location search with \"Did you mean?\" suggestions\n");
    printf("   - Current weather: temperature, humidity, wind, rain, pressure\n");
    printf("   - Hourly (24 h) and daily (7 day) forecasts\n");
    printf("   - Favourite locations, search history and weather alerts\n");
    printf("   - Automatic location detection (GPS file / time zone)\n");
    printf("   - Dark / Light / Classic display themes\n");
    ui_color(COL_HIGHLIGHT);
    printf("   - UNIQUE: Smart Advisory (feels-like, comfort score, farming,\n");
    printf("     travel and health advice) + Best Day Planner\n");
    ui_color(COL_TITLE);
    printf("\n  Admin module\n");
    ui_reset();
    printf("   - User, location, weather data and alert management\n");
    printf("   - System monitoring and text report generation\n\n");
    ui_color(COL_MUTED);
    printf("  Default admin login: admin / admin123 (please change it).\n");
    printf("  Data folder: %s   |   Reports folder: %s\n", get_data_dir(), get_report_dir());
    ui_reset();
}

/* =============================================================
 *  MAIN
 * ============================================================= */

int main(int argc, char *argv[])
{
    int r = parse_arguments(argc, argv);
    if (r <= 0) return r == 0 ? 0 : 1;

    ui_init();
    ui_set_theme(THEME_DARK);
    if (load_all_data() != 0) return 1;
    log_event("START", "program started");

    for (;;) {
        int choice;

        ui_set_status("");
        ui_banner();
        printf("\n");
        ui_menu_item(1, "User Login");
        ui_menu_item(2, "Register New Account");
        ui_menu_item(3, "Admin Login");
        ui_menu_item(4, "About this System");
        ui_menu_item(0, "Exit");
        printf("\n");
        choice = input_int("Enter your choice: ", 0, 4);

        switch (choice) {
        case 1: do_login();       break;
        case 2: do_register();    break;
        case 3: do_admin_login(); break;
        case 4: do_about(); ui_pause(); break;
        default:
            log_event("STOP", "program closed");
            ui_clear();
            ui_msg_success("Thank you for using the %s. Stay safe!", APP_NAME);
            ui_restore();
            return 0;
        }
    }
}
