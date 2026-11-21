/*
 * weather_single.c
 * Weather Forecasting System - all modules merged into ONE file.
 * Compile:  gcc -std=c99 -O2 weather_single.c -o weather -lm
 * Data files are created automatically in ./data on first run.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

/* ===================== common.h ===================== */
/*
 * common.h
 * ---------------------------------------------------------------
 * Weather Forecasting System - shared constants and limits.
 * Every module includes this header so that all array sizes and
 * file names are defined in exactly one place.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <time.h>

#define APP_NAME     "Weather Forecasting System"
#define APP_VERSION  "1.0.0"

/* ---------- capacity limits ---------- */
#define MAX_USERS         500
#define MAX_LOCATIONS     200
#define MAX_ALERTS        300
#define MAX_FAVORITES     10    /* favourite locations per user  */
#define MAX_HISTORY_SHOW  20    /* history entries shown to user */
#define MAX_SEARCH_RESULT 20

/* ---------- string lengths (including '\0') ---------- */
#define NAME_LEN      50
#define USERNAME_LEN  21        /* 20 characters + '\0'           */
#define PASSWORD_LEN  33        /* 32 characters + '\0'           */
#define HASH_LEN      17        /* 16 hex digits + '\0'           */
#define TIME_LEN      20        /* "YYYY-MM-DD HH:MM:SS" + '\0'    */
#define TYPE_LEN      16
#define MSG_LEN       160
#define LINE_LEN      512
#define PATH_LEN      260

/* ---------- data files (stored inside the data directory) ---------- */
#define FILE_USERS       "users.txt"
#define FILE_ADMIN       "admin.txt"
#define FILE_LOCATIONS   "locations.txt"
#define FILE_WEATHER     "weather.txt"
#define FILE_ALERTS      "alerts.txt"
#define FILE_FAVORITES   "favorites.txt"
#define FILE_HISTORY     "history.txt"
#define FILE_DEVICE_GPS  "device_location.txt"
#define FILE_LOG         "system.log"

#define DEFAULT_DATA_DIR    "data"
#define DEFAULT_REPORT_DIR  "reports"

/* Field separator used in every data file */
#define FIELD_SEP '|'

#ifndef PI
#define PI 3.14159265358979323846
#endif

#endif /* COMMON_H */

/* ===================== location.h ===================== */
/*
 * location.h
 * ---------------------------------------------------------------
 * Location (city) records. Each location stores its coordinates,
 * time zone and a few climate values that the forecast engine
 * uses as a baseline.
 */
#ifndef LOCATION_H
#define LOCATION_H


typedef struct {
    int    id;
    char   name[NAME_LEN];
    char   region[NAME_LEN];     /* division / state, country       */
    double latitude;             /* + north, - south                */
    double longitude;            /* + east,  - west                 */
    double utc_offset;           /* hours, e.g. 6.0 for Bangladesh  */
    double avg_temp;             /* yearly mean temperature (C)     */
    double season_range;         /* hottest month - coldest month   */
    double daily_range;          /* typical day/night difference    */
    int    avg_humidity;         /* %                               */
    int    rain_tendency;        /* 0 (desert) .. 100 (very rainy)  */
} Location;

/* ---------- storage ---------- */
int       location_load(void);
int       location_save(void);
void      location_seed_defaults(void);

/* ---------- access ---------- */
int       location_count(void);
Location *location_at(int index);
Location *location_find(int id);
const char *location_name(int id);

/* ---------- search ---------- */
int       location_search(const char *query, int ids[], int max);
int       location_suggest(const char *query, int ids[], int max);

/* ---------- admin operations ---------- */
int       location_add(Location *loc);          /* returns new id or -1 */
int       location_update(const Location *loc);
int       location_delete(int id);
int       location_name_exists(const char *name, int except_id);

/* ---------- geography ---------- */
double    haversine_km(double lat1, double lon1, double lat2, double lon2);
int       location_nearest(double lat, double lon, double *distance_km);
void      location_local_tm(const Location *loc, time_t t, struct tm *out);
void      location_format_coords(const Location *loc, char *buf, size_t size);

#endif /* LOCATION_H */

/* ===================== weather.h ===================== */
/*
 * weather.h
 * ---------------------------------------------------------------
 * Current weather observation for each location.
 * Records come from the (simulated) automatic weather feed or are
 * entered manually by the administrator.
 */
#ifndef WEATHER_H
#define WEATHER_H


typedef enum {
    COND_CLEAR = 0,
    COND_PARTLY_CLOUDY,
    COND_CLOUDY,
    COND_FOG,
    COND_LIGHT_RAIN,
    COND_HEAVY_RAIN,
    COND_THUNDERSTORM,
    COND_SNOW,
    COND_COUNT
} Condition;

#define SOURCE_AUTO   'S'      /* automatic weather feed (simulated) */
#define SOURCE_MANUAL 'M'      /* entered by the administrator       */

#define AUTO_REFRESH_MINUTES    60   /* auto feed data refreshed after 1 hour */
#define MANUAL_KEEP_MINUTES    360   /* manual data kept for 6 hours          */

typedef struct {
    int       location_id;
    double    temperature;     /* degree Celsius        */
    int       humidity;        /* %                     */
    double    wind_speed;      /* km/h                  */
    int       wind_dir;        /* degrees, 0 = North    */
    int       rain_chance;     /* % probability of rain */
    double    rainfall_mm;     /* expected rain, mm/h   */
    double    pressure;        /* hPa                   */
    Condition condition;
    char      updated[TIME_LEN];
    char      source;          /* SOURCE_AUTO / SOURCE_MANUAL */
} Weather;

/* ---------- storage ---------- */
int      weather_load(void);
int      weather_save(void);

/* ---------- access ---------- */
int      weather_count(void);
Weather *weather_at(int index);
Weather *weather_get(int location_id);
int      weather_set(const Weather *w);          /* insert or replace */
int      weather_remove(int location_id);

/* ---------- automatic feed ---------- */
int      weather_sync(time_t now, int force);    /* returns records refreshed */
long     weather_age_minutes(const Weather *w, time_t now);

/* ---------- helpers ---------- */
Condition   condition_derive(double temp, int humidity, int rain_chance,
                             double wind, double pressure);
const char *condition_name(Condition c);
const char *condition_icon_line(Condition c, int line);   /* line 0..4 */
const char *wind_direction_name(int degrees);

#endif /* WEATHER_H */

/* ===================== forecast.h ===================== */
/*
 * forecast.h
 * ---------------------------------------------------------------
 * Forecast engine: produces hourly (next 24 h) and daily
 * (next 7 days) forecasts for a location.
 *
 * The engine uses a simplified climate model:
 *   value = climate baseline (season + time of day)
 *         + smooth "weather system" variation (deterministic noise)
 *         + correction from the latest observation, which fades
 *           away with time (persistence).
 */
#ifndef FORECAST_H
#define FORECAST_H


#define HOURLY_COUNT 24
#define DAILY_COUNT  7

typedef struct {
    time_t    time;            /* start of the hour                 */
    double    temperature;
    int       humidity;
    double    wind_speed;
    int       wind_dir;
    int       rain_chance;
    double    rainfall_mm;
    double    pressure;
    Condition condition;
} HourlyForecast;

typedef struct {
    time_t    date;            /* local midnight of that day        */
    double    temp_min;
    double    temp_max;
    int       humidity;        /* daily average                     */
    double    wind_speed;      /* strongest wind of the day         */
    int       wind_dir;        /* direction at midday               */
    int       rain_chance;     /* highest probability of the day    */
    double    rainfall_mm;     /* expected total rain               */
    double    pressure;        /* daily average                     */
    Condition condition;
} DailyForecast;

/* Generate a new "observed" reading for the automatic feed. */
void forecast_simulate_current(const Location *loc, time_t now, Weather *out);

/* Forecast for one moment in time (observation may be NULL). */
void forecast_point(const Location *loc, const Weather *obs, time_t t, HourlyForecast *out);

/* Next `hours` hourly values starting at the next full hour. */
int  forecast_hourly(const Location *loc, const Weather *obs, time_t now,
                     HourlyForecast out[], int hours);

/* `days` daily summaries starting with today. */
int  forecast_daily(const Location *loc, const Weather *obs, time_t now,
                    DailyForecast out[], int days);

/* Climate helpers (also used by the advisory module) */
double climate_daily_mean(const Location *loc, int day_of_year);
double climate_rain_factor(const Location *loc, int month);

#endif /* FORECAST_H */

/* ===================== advisory.h ===================== */
/*
 * advisory.h
 * ---------------------------------------------------------------
 * UNIQUE FEATURE: Smart Weather Advisory & Best Day Planner
 *
 *  - "Feels like" temperature (Heat Index / Wind Chill formulas)
 *  - Comfort score (0-100) for any weather
 *  - Practical advice for Health, Agriculture (farmers),
 *    Travel & Transport and Daily life
 *  - Ranks the next 7 days and recommends the best day for
 *    outdoor work, travel or events
 */
#ifndef ADVISORY_H
#define ADVISORY_H


#define MAX_ADVICE 16

typedef enum { ADVICE_GOOD = 0, ADVICE_CAUTION = 1, ADVICE_DANGER = 2 } AdviceLevel;

typedef struct {
    char        category[24];
    AdviceLevel level;
    char        text[MSG_LEN];
} Advice;

double      feels_like(double temp_c, int humidity, double wind_kmh);
const char *heat_stress_label(double feels_c);
int         comfort_score(double temp_c, int humidity, double wind_kmh, int rain_chance);
const char *comfort_label(int score);
int         day_score(const DailyForecast *day);
int         rank_days(const DailyForecast days[], int n, int order[]);  /* best first */

int advisory_generate(const Weather *now,
                      const HourlyForecast hours[], int n_hours,
                      const DailyForecast days[], int n_days,
                      Advice out[], int max);

#endif /* ADVISORY_H */

/* ===================== alert.h ===================== */
/*
 * alert.h
 * ---------------------------------------------------------------
 * Weather alerts.
 *  - Manual alerts : created by the administrator, stored in
 *                    data/alerts.txt, with an expiry time.
 *  - Auto alerts   : detected by the system from current weather
 *                    and the 24-hour forecast (heat wave, storm,
 *                    cyclone, heavy rain, fog, cold wave ...).
 */
#ifndef ALERT_H
#define ALERT_H


typedef enum {
    SEV_ADVISORY = 1,
    SEV_WATCH    = 2,
    SEV_WARNING  = 3
} Severity;

#define ALERT_ALL_LOCATIONS 0

typedef struct {
    int  id;                       /* 0 for auto-detected alerts  */
    int  location_id;              /* 0 = all locations           */
    int  severity;                 /* Severity                    */
    char type[TYPE_LEN];           /* HEAT, STORM, RAIN ...       */
    char message[MSG_LEN];
    char issued[TIME_LEN];
    char expires[TIME_LEN];
    int  active;                   /* 1 = active, 0 = disabled    */
} Alert;

/* ---------- storage ---------- */
int    alert_load(void);
int    alert_save(void);

/* ---------- manual alerts ---------- */
int    alert_count(void);
Alert *alert_at(int index);
Alert *alert_find(int id);
int    alert_add(Alert *a);                    /* returns new id or -1 */
int    alert_delete(int id);
int    alert_toggle(int id);
int    alert_is_current(const Alert *a, time_t now);
int    alert_for_location(int location_id, time_t now, Alert out[], int max);
int    alert_remove_location(int location_id);

/* ---------- automatic detection ---------- */
int    alert_detect(const Location *loc, const Weather *w,
                    const HourlyForecast hours[], int n_hours,
                    Alert out[], int max);

const char *severity_name(int severity);

#endif /* ALERT_H */

/* ===================== detect.h ===================== */
/*
 * detect.h
 * ---------------------------------------------------------------
 * Automatic location detection.
 *
 * Detection order:
 *   1. Environment variables WFS_LAT / WFS_LON (e.g. set by a GPS tool)
 *   2. data/device_location.txt - GPS reading of this device
 *      (simulated sensor file: latitude|longitude|source)
 *   3. The computer's time zone (UTC offset) -> a city in that zone
 *   4. The user's home location (fallback)
 * The nearest known city is found with the Haversine formula.
 */
#ifndef DETECT_H
#define DETECT_H


typedef enum {
    DETECT_NONE = 0,
    DETECT_ENVIRONMENT,
    DETECT_GPS_FILE,
    DETECT_TIMEZONE,
    DETECT_HOME
} DetectMethod;

typedef struct {
    int          location_id;
    DetectMethod method;
    double       latitude;       /* device coordinates (if known) */
    double       longitude;
    double       distance_km;    /* device -> city, -1 if unknown */
} DetectResult;

int         detect_location(int home_location, DetectResult *out);
const char *detect_method_name(DetectMethod m);
int         detect_save_device_coords(double lat, double lon);

#endif /* DETECT_H */

/* ===================== logger.h ===================== */
/*
 * logger.h
 * ---------------------------------------------------------------
 * Activity log (data/system.log). Used by the admin
 * "System Monitoring" screen to show recent events such as
 * logins, failed logins, registrations and data changes.
 */
#ifndef LOGGER_H
#define LOGGER_H


void log_event(const char *type, const char *fmt, ...);
int  log_tail(char lines[][LINE_LEN], int max_lines);   /* newest last */
long log_count_type(const char *type);

#endif /* LOGGER_H */

/* ===================== menus.h ===================== */
/*
 * menus.h
 * ---------------------------------------------------------------
 * Interactive menus of the two modules:
 *   - User module   (after a user logs in)
 *   - Admin module  (after the administrator logs in)
 */
#ifndef MENUS_H
#define MENUS_H

void user_session_run(const char *username);
void admin_session_run(void);

#endif /* MENUS_H */

/* ===================== report.h ===================== */
/*
 * report.h
 * ---------------------------------------------------------------
 * Text report generation (saved in the reports/ folder).
 */
#ifndef REPORT_H
#define REPORT_H


int report_weather_summary(char *out_path, size_t size);
int report_location_forecast(int location_id, char *out_path, size_t size);
int report_user_activity(char *out_path, size_t size);
int report_print_file(const char *path);

#endif /* REPORT_H */

/* ===================== ui.h ===================== */
/*
 * ui.h
 * ---------------------------------------------------------------
 * Console user interface: colour themes (Dark / Light / Classic),
 * screen layout helpers and safe keyboard input functions.
 */
#ifndef UI_H
#define UI_H

#include <stddef.h>

#define UI_WIDTH 78

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT = 1,
    THEME_CLASSIC = 2,      /* no colours - for old terminals */
    THEME_COUNT
} Theme;

typedef enum {
    COL_NORMAL = 0,
    COL_TITLE,
    COL_ACCENT,
    COL_SUCCESS,
    COL_WARNING,
    COL_ERROR,
    COL_MUTED,
    COL_HIGHLIGHT,
    COL_INFO,
    COL_COUNT
} ColorRole;

/* ---------- setup ---------- */
void        ui_init(void);
void        ui_restore(void);
void        ui_set_theme(int theme);
int         ui_get_theme(void);
const char *ui_theme_name(int theme);
void        ui_set_status(const char *status);

/* ---------- colours ---------- */
void ui_color(ColorRole role);
void ui_reset(void);

/* ---------- layout ---------- */
void ui_clear(void);
void ui_banner(void);
void ui_header(const char *title);
void ui_section(const char *title);
void ui_divider(char ch);
void ui_center(const char *text, int width);
void ui_menu_item(int key, const char *text);
void ui_menu_item_tag(int key, const char *text, const char *tag);
void ui_bar(double fraction, int width, char ch);

/* ---------- messages ---------- */
void ui_msg_success(const char *fmt, ...);
void ui_msg_error(const char *fmt, ...);
void ui_msg_warning(const char *fmt, ...);
void ui_msg_info(const char *fmt, ...);
void ui_pause(void);

/* ---------- input (all functions validate and re-ask) ---------- */
int    input_line(const char *prompt, char *buf, size_t size);
int    input_int(const char *prompt, int min, int max);
int    input_int_default(const char *prompt, int min, int max, int def);
double input_double(const char *prompt, double min, double max);
double input_double_default(const char *prompt, double min, double max, double def);
int    input_yes_no(const char *prompt);
void   input_password(const char *prompt, char *buf, size_t size);

#endif /* UI_H */

/* ===================== user.h ===================== */
/*
 * user.h
 * ---------------------------------------------------------------
 * User accounts, registration, login and administrator
 * credentials. Passwords are stored only as salted hashes.
 */
#ifndef USER_H
#define USER_H


typedef struct {
    char username[USERNAME_LEN];
    char password_hash[HASH_LEN];
    char full_name[NAME_LEN];
    int  home_location;            /* location id                */
    int  theme;                    /* Theme (Dark/Light/Classic) */
    int  active;                   /* 1 = active, 0 = blocked    */
    char created[TIME_LEN];
    char last_login[TIME_LEN];
} User;

typedef enum {
    AUTH_OK = 0,
    AUTH_NO_USER,
    AUTH_WRONG_PASSWORD,
    AUTH_BLOCKED
} AuthResult;

#define MIN_PASSWORD_LEN 6
#define MAX_LOGIN_TRIES  3

/* ---------- storage ---------- */
int   user_load(void);
int   user_save(void);

/* ---------- access ---------- */
int   user_count(void);
User *user_at(int index);
User *user_find(const char *username);

/* ---------- validation ---------- */
int   validate_username(const char *username, char *error, size_t size);
int   validate_password(const char *password, char *error, size_t size);
int   validate_full_name(const char *name, char *error, size_t size);

/* ---------- account operations ---------- */
int        user_register(const char *username, const char *password,
                         const char *full_name, int home_location);
AuthResult user_login(const char *username, const char *password);
int        user_change_password(const char *username, const char *new_password);
int        user_check_password(const char *username, const char *password);
int        user_delete(const char *username);
int        user_set_active(const char *username, int active);
int        user_update(const User *u);
int        user_clear_home_location(int location_id, int replacement);

/* ---------- administrator ---------- */
void  admin_ensure_default(void);
int   admin_login(const char *username, const char *password);
int   admin_change_password(const char *new_password);
const char *admin_username(void);

#endif /* USER_H */

/* ===================== userdata.h ===================== */
/*
 * userdata.h
 * ---------------------------------------------------------------
 * Per-user data: favourite locations and search history.
 *
 * data/favorites.txt : username|location_id
 * data/history.txt   : username|location_id|YYYY-MM-DD HH:MM:SS
 */
#ifndef USERDATA_H
#define USERDATA_H


typedef struct {
    char username[USERNAME_LEN];
    int  location_id;
    char time[TIME_LEN];
} HistoryEntry;

/* ---------- favourites ---------- */
int  favorites_list(const char *username, int ids[], int max);
int  favorites_is(const char *username, int location_id);
int  favorites_add(const char *username, int location_id);   /* 0 ok, 1 exists, -1 full, -2 error */
int  favorites_remove(const char *username, int location_id);
int  favorites_remove_user(const char *username);
int  favorites_remove_location(int location_id);
long favorites_total(void);

/* ---------- search history ---------- */
int  history_add(const char *username, int location_id);
int  history_list(const char *username, HistoryEntry out[], int max); /* newest first */
int  history_clear(const char *username);
int  history_remove_location(int location_id);
long history_total(void);
long history_count_user(const char *username);
int  history_top_locations(int ids[], long counts[], int max);
int  history_most_searched_by(const char *username);

#endif /* USERDATA_H */

/* ===================== utils.h ===================== */
/*
 * utils.h
 * ---------------------------------------------------------------
 * General purpose helpers: string handling, time formatting,
 * file paths, password hashing and a deterministic random
 * number generator used by the forecast engine.
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/* ---------- strings ---------- */
void   str_trim(char *s);
void   str_copy(char *dst, const char *src, size_t size);
void   str_to_lower(char *dst, const char *src, size_t size);
int    str_iequals(const char *a, const char *b);
int    str_icontains(const char *haystack, const char *needle);
int    str_is_number(const char *s);
void   sanitize_field(char *s);           /* removes '|' and new lines */
int    split_fields(char *line, char sep, char **fields, int max_fields);
int    edit_distance(const char *a, const char *b);   /* case-insensitive */

/* ---------- time ---------- */
void   format_time(time_t t, char *buf, size_t size);      /* YYYY-MM-DD HH:MM:SS */
void   format_time_short(time_t t, char *buf, size_t size);/* YYYY-MM-DD HH:MM    */
time_t parse_time(const char *s);                          /* returns -1 on error */
void   now_string(char *buf, size_t size);
long   system_utc_offset_seconds(void);

/* ---------- paths & files ---------- */
void        set_data_dir(const char *dir);
const char *get_data_dir(void);
void        set_report_dir(const char *dir);
const char *get_report_dir(void);
void        data_path(const char *file, char *out, size_t size);
int         ensure_dir(const char *path);
int         file_exists(const char *path);
long        file_size(const char *path);
int         replace_file(const char *tmp_path, const char *final_path);

/* ---------- hashing ---------- */
void     hash_password(const char *username, const char *password, char out[HASH_LEN]);

/* ---------- deterministic pseudo random numbers ---------- */
uint32_t hash32(uint32_t x);
uint32_t make_seed(int a, int b, int c);
double   noise_unit(uint32_t seed);     /* value in [0, 1)  */
double   noise_signed(uint32_t seed);   /* value in [-1, 1] */

/* ---------- maths ---------- */
double clampd(double v, double lo, double hi);
int    clampi(int v, int lo, int hi);

#endif /* UTILS_H */

/* ===================== views.h ===================== */
/*
 * views.h
 * ---------------------------------------------------------------
 * Screens that show weather information. They are shared by the
 * user module and the administrator module.
 */
#ifndef VIEWS_H
#define VIEWS_H


/* Ask the user for a location (name, part of a name or id). 0 = cancelled */
int  view_select_location(const char *prompt, int default_id);

void view_location_list(void);
void view_current_weather(int location_id);
void view_hourly_forecast(int location_id);
void view_daily_forecast(int location_id);
void view_advisory(int location_id);
void view_compare(int first_id, int second_id);
void view_weather_table(void);

/* Alerts for a set of locations (manual + automatically detected). */
int  view_collect_alerts(const int ids[], int n, Alert out[], int max);
void view_alerts(const int ids[], int n);
void view_print_alert(const Alert *a);

/* Colour role used for a weather condition. */
int  view_condition_color(int condition);

#endif /* VIEWS_H */

/* ===================== admin_menu.c ===================== */
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

/* ===================== advisory.c ===================== */
/*
 * advisory.c
 * ---------------------------------------------------------------
 * UNIQUE FEATURE: Smart Weather Advisory & Best Day Planner.
 *
 * Turns raw numbers (temperature, humidity, wind, rain) into advice
 * people can act on: farmers know when to spray or harvest,
 * travellers know when river crossings are risky, students know
 * whether to carry an umbrella, and everyone can see which day of
 * the coming week is the best for outdoor plans.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>


/*
 * Apparent ("feels like") temperature:
 *  - Wind Chill (Environment Canada / NWS) when cold and windy
 *  - Heat Index (NWS Rothfusz regression) when hot and humid
 */
double feels_like(double t, int rh, double wind)
{
    if (t <= 10.0 && wind > 4.8) {
        double v = pow(wind, 0.16);
        return 13.12 + 0.6215 * t - 11.37 * v + 0.3965 * t * v;
    }
    if (t >= 26.7 && rh >= 40) {
        double T = t * 9.0 / 5.0 + 32.0;   /* formula works in Fahrenheit */
        double R = (double)rh;
        double hi = -42.379 + 2.04901523 * T + 10.14333127 * R
                  - 0.22475541 * T * R - 0.00683783 * T * T
                  - 0.05481717 * R * R + 0.00122874 * T * T * R
                  + 0.00085282 * T * R * R - 0.00000199 * T * T * R * R;
        return (hi - 32.0) * 5.0 / 9.0;
    }
    return t;
}

const char *heat_stress_label(double f)
{
    if (f >= 54.0) return "Extreme danger";
    if (f >= 41.0) return "Danger";
    if (f >= 32.0) return "Extreme caution";
    if (f >= 27.0) return "Caution";
    if (f <= -25.0) return "Frostbite danger";
    if (f <= 0.0)  return "Freezing";
    if (f <= 12.0) return "Cold";
    return "Comfortable";
}

/*
 * Comfort score 0..100. Starts at 100 and loses points for heat,
 * cold, high humidity, strong wind and chance of rain.
 */
int comfort_score(double temp, int hum, double wind, int rain)
{
    double f = feels_like(temp, hum, wind);
    double s = 100.0;

    if (f > 26.0) s -= (f - 26.0) * 4.0;
    if (f < 18.0) s -= (18.0 - f) * 3.0;
    if (hum > 70) s -= (hum - 70) * 0.6;
    if (hum < 25) s -= (25 - hum) * 0.5;
    if (wind > 25.0) s -= (wind - 25.0) * 1.2;
    s -= rain * 0.45;

    return (int)(clampd(s, 0.0, 100.0) + 0.5);
}

const char *comfort_label(int score)
{
    if (score >= 80) return "Excellent";
    if (score >= 60) return "Good";
    if (score >= 40) return "Fair";
    if (score >= 20) return "Poor";
    return "Very Poor";
}

/* Score of a whole day, based mainly on the daytime temperature. */
int day_score(const DailyForecast *d)
{
    double daytime = 0.7 * d->temp_max + 0.3 * d->temp_min;
    return comfort_score(daytime, d->humidity, d->wind_speed * 0.8, d->rain_chance);
}

/* Fill order[] with day indexes sorted by score (best first). */
int rank_days(const DailyForecast days[], int n, int order[])
{
    int score[DAILY_COUNT * 2];
    int i, j;
    if (n > DAILY_COUNT * 2) n = DAILY_COUNT * 2;

    for (i = 0; i < n; i++) {
        order[i] = i;
        score[i] = day_score(&days[i]);
    }
    for (i = 1; i < n; i++) {               /* insertion sort, stable */
        int key = order[i];
        j = i - 1;
        while (j >= 0 && score[order[j]] < score[key]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
    return n;
}

static void add_advice(Advice out[], int *count, int max, const char *category,
                       AdviceLevel level, const char *text)
{
    if (*count >= max) return;
    str_copy(out[*count].category, category, sizeof out[*count].category);
    out[*count].level = level;
    str_copy(out[*count].text, text, sizeof out[*count].text);
    (*count)++;
}

int advisory_generate(const Weather *w,
                      const HourlyForecast hours[], int n_hours,
                      const DailyForecast days[], int n_days,
                      Advice out[], int max)
{
    int count = 0, i;
    char text[MSG_LEN];
    double feels = feels_like(w->temperature, w->humidity, w->wind_speed);
    int rain6 = w->rain_chance, rain12 = w->rain_chance;
    double wind6 = w->wind_speed, max_wind24 = w->wind_speed;
    int storm24 = (w->condition == COND_THUNDERSTORM);
    int fog12 = (w->condition == COND_FOG);
    int heavy24 = (w->condition == COND_HEAVY_RAIN);
    double rain3days = 0.0;
    int dry2days = 1;

    for (i = 0; i < n_hours; i++) {
        if (i < 6) {
            if (hours[i].rain_chance > rain6) rain6 = hours[i].rain_chance;
            if (hours[i].wind_speed > wind6) wind6 = hours[i].wind_speed;
        }
        if (i < 12) {
            if (hours[i].rain_chance > rain12) rain12 = hours[i].rain_chance;
            if (hours[i].condition == COND_FOG) fog12 = 1;
        }
        if (hours[i].wind_speed > max_wind24) max_wind24 = hours[i].wind_speed;
        if (hours[i].condition == COND_THUNDERSTORM) storm24 = 1;
        if (hours[i].condition == COND_HEAVY_RAIN) heavy24 = 1;
    }
    for (i = 0; i < n_days && i < 3; i++) rain3days += days[i].rainfall_mm;
    for (i = 0; i < n_days && i < 2; i++) if (days[i].rain_chance >= 30) dry2days = 0;

    /* ---------------- Health ---------------- */
    if (feels >= 41.0) {
        snprintf(text, sizeof text, "Dangerous heat (feels like %.0f C). Stay indoors from "
                 "11am-4pm, drink water every 20 minutes.", feels);
        add_advice(out, &count, max, "Health", ADVICE_DANGER, text);
    } else if (feels >= 32.0) {
        snprintf(text, sizeof text, "Hot and humid (feels like %.0f C). Drink plenty of "
                 "water and rest in the shade.", feels);
        add_advice(out, &count, max, "Health", ADVICE_CAUTION, text);
    } else if (feels <= 0.0) {
        snprintf(text, sizeof text, "Freezing (feels like %.0f C). Wear heavy winter "
                 "clothes, gloves and a cap.", feels);
        add_advice(out, &count, max, "Health", ADVICE_DANGER, text);
    } else if (feels <= 12.0) {
        add_advice(out, &count, max, "Health", ADVICE_CAUTION,
                   "Cold weather. Wear warm clothes; take care of children and the elderly.");
    } else {
        add_advice(out, &count, max, "Health", ADVICE_GOOD,
                   "Temperature is comfortable for most people.");
    }
    if (w->humidity >= 85)
        add_advice(out, &count, max, "Health", ADVICE_CAUTION,
                   "Very humid air - you may feel tired quickly. Wear light cotton clothes.");

    /* ---------------- Agriculture ---------------- */
    if (wind6 < 15.0 && rain6 < 30)
        add_advice(out, &count, max, "Agriculture", ADVICE_GOOD,
                   "Good time to spray pesticide / fertilizer (low wind, no rain in 6 hours).");
    else if (wind6 >= 15.0) {
        snprintf(text, sizeof text, "Avoid spraying now - wind up to %.0f km/h will blow "
                 "the chemicals away.", wind6);
        add_advice(out, &count, max, "Agriculture", ADVICE_CAUTION, text);
    } else {
        snprintf(text, sizeof text, "Avoid spraying now - rain (up to %d%%) may wash the "
                 "chemicals off the crops.", rain6);
        add_advice(out, &count, max, "Agriculture", ADVICE_CAUTION, text);
    }

    if (rain3days >= 20.0) {
        snprintf(text, sizeof text, "About %.0f mm rain expected in 3 days - clear field "
                 "drainage and delay irrigation.", rain3days);
        add_advice(out, &count, max, "Agriculture", ADVICE_CAUTION, text);
    } else if (rain3days < 2.0 && n_days > 0 && days[0].temp_max >= 30.0) {
        add_advice(out, &count, max, "Agriculture", ADVICE_CAUTION,
                   "Hot and dry days ahead - irrigate crops early morning or evening.");
    } else {
        add_advice(out, &count, max, "Agriculture", ADVICE_GOOD,
                   "Soil moisture should be fine - follow the normal irrigation schedule.");
    }
    if (dry2days)
        add_advice(out, &count, max, "Agriculture", ADVICE_GOOD,
                   "Next 2 days look dry - good for harvesting and sun-drying crops.");
    else
        add_advice(out, &count, max, "Agriculture", ADVICE_CAUTION,
                   "Rain possible within 2 days - keep harvested crops covered.");

    /* ---------------- Travel & Transport ---------------- */
    if (storm24 || max_wind24 >= 40.0)
        add_advice(out, &count, max, "Travel", ADVICE_DANGER,
                   "Storm / strong wind expected - avoid launch, ferry and boat journeys.");
    if (fog12)
        add_advice(out, &count, max, "Travel", ADVICE_CAUTION,
                   "Fog expected - drive slowly with fog lights; ferries/flights may be late.");
    if (heavy24)
        add_advice(out, &count, max, "Travel", ADVICE_CAUTION,
                   "Heavy rain - water-logging likely on city roads, allow extra travel time.");
    if (!storm24 && max_wind24 < 40.0 && !fog12 && !heavy24)
        add_advice(out, &count, max, "Travel", ADVICE_GOOD,
                   "Good conditions for road, rail and river travel.");

    /* ---------------- Daily life ---------------- */
    if (rain12 >= 40) {
        snprintf(text, sizeof text, "Carry an umbrella or raincoat (rain chance up to %d%% "
                 "in the next 12 hours).", rain12);
        add_advice(out, &count, max, "Daily Life", ADVICE_CAUTION, text);
    } else {
        add_advice(out, &count, max, "Daily Life", ADVICE_GOOD,
                   "No umbrella needed for the next 12 hours.");
    }
    if (w->condition <= COND_PARTLY_CLOUDY && w->temperature >= 30.0)
        add_advice(out, &count, max, "Daily Life", ADVICE_CAUTION,
                   "Strong sunshine - use sunscreen, a cap and sunglasses outdoors.");
    if (rain6 < 20 && w->humidity < 80)
        add_advice(out, &count, max, "Daily Life", ADVICE_GOOD,
                   "Good weather for drying clothes outside.");
    if (comfort_score(w->temperature, w->humidity, w->wind_speed, w->rain_chance) >= 60)
        add_advice(out, &count, max, "Daily Life", ADVICE_GOOD,
                   "Nice weather for outdoor games, walking and exercise.");

    return count;
}

/* ===================== alert.c ===================== */
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


static Alert g_alerts[MAX_ALERTS];
static int   g_alert_count = 0;

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

    g_alert_count = 0;
    data_path(FILE_ALERTS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL && g_alert_count < MAX_ALERTS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_alert(line, &g_alerts[g_alert_count])) g_alert_count++;
    }
    fclose(fp);
    return g_alert_count;
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
    for (i = 0; i < g_alert_count; i++) {
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

int alert_count(void) { return g_alert_count; }

Alert *alert_at(int index)
{
    if (index < 0 || index >= g_alert_count) return NULL;
    return &g_alerts[index];
}

Alert *alert_find(int id)
{
    int i;
    for (i = 0; i < g_alert_count; i++)
        if (g_alerts[i].id == id) return &g_alerts[i];
    return NULL;
}

int alert_add(Alert *a)
{
    int i, max_id = 0;
    if (g_alert_count >= MAX_ALERTS) return -1;
    for (i = 0; i < g_alert_count; i++)
        if (g_alerts[i].id > max_id) max_id = g_alerts[i].id;
    a->id = max_id + 1;
    sanitize_field(a->type);
    sanitize_field(a->message);
    g_alerts[g_alert_count++] = *a;
    return alert_save() == 0 ? a->id : -1;
}

int alert_delete(int id)
{
    int i;
    for (i = 0; i < g_alert_count; i++) {
        if (g_alerts[i].id == id) {
            for (; i < g_alert_count - 1; i++) g_alerts[i] = g_alerts[i + 1];
            g_alert_count--;
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
    for (i = 0; i < g_alert_count && n < max; i++) {
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
    for (i = 0; i < g_alert_count; i++) {
        if (g_alerts[i].location_id == location_id && location_id != ALERT_ALL_LOCATIONS) {
            removed++;
            continue;
        }
        g_alerts[j++] = g_alerts[i];
    }
    g_alert_count = j;
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

/* ===================== detect.c ===================== */
/*
 * detect.c
 * ---------------------------------------------------------------
 * Automatic location detection (see detect.h for the order used).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


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

/* ===================== forecast.c ===================== */
/*
 * forecast.c
 * ---------------------------------------------------------------
 * Simplified, deterministic weather model.
 *
 * Why deterministic? The same location and time always give the same
 * result, so forecasts do not "jump" every time the screen is opened
 * and the unit tests can check exact behaviour.
 *
 * Model steps for a moment t at location L:
 *  1. Season   : daily mean = avg_temp + season_range/2 * cos(day-of-year)
 *  2. Day/night: + daily_range/2 * sin(hour)  (warmest ~15:00, coldest ~03:00)
 *  3. Weather systems: smooth noise moves temperature, rain, wind and
 *     pressure up and down over hours/days (like passing fronts).
 *  4. Monsoon  : rain probability is scaled by the month for the
 *     northern tropics (wet June-September, dry November-February).
 *  5. Storms   : when the "storm" noise is high, pressure drops and
 *     wind/rain increase (used by the alert system).
 *  6. Persistence: the difference between the latest observation and
 *     the model is added back and fades out over ~12 hours.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>


/* noise channels - each weather variable gets its own random stream */
enum { CH_TEMP = 1, CH_RAIN, CH_HUMIDITY, CH_WIND, CH_PRESSURE, CH_STORM, CH_WIND_DIR };

/*
 * Smooth value noise in [-1, 1]: random values are picked at fixed
 * intervals (period seconds) and blended with a smooth curve, so the
 * result changes gradually instead of jumping.
 */
static double smooth_noise(int loc_id, int channel, time_t t, long period)
{
    long long k = (long long)t / period;
    double frac = (double)((long long)t - k * period) / (double)period;
    double a = noise_signed(make_seed(loc_id, channel, (int)k));
    double b = noise_signed(make_seed(loc_id, channel, (int)(k + 1)));
    double s = frac * frac * (3.0 - 2.0 * frac);        /* smoothstep */
    return a + (b - a) * s;
}

double climate_daily_mean(const Location *loc, int day_of_year)
{
    /*
     * Coldest day (trough) and hottest day (peak) of the year.
     * South Asia and other tropics are hottest just before the monsoon
     * (mid May) and coldest in mid January; other places peak in July.
     * The year is split into a warming half and a cooling half, each
     * following half a cosine wave, so the two halves may differ in length.
     */
    double trough = 15.0;
    double peak   = (fabs(loc->latitude) < 30.0) ? 135.0 : 200.0;
    double d = (double)day_of_year;
    double rise, wave;

    if (loc->latitude < 0) {                 /* southern hemisphere: shift 6 months */
        d -= 182.5;
        if (d < 0) d += 365.0;
    }
    rise = peak - trough;
    if (d >= trough && d < peak) {
        wave = -cos(PI * (d - trough) / rise);                      /* -1 -> +1 */
    } else {
        double since_peak = (d >= peak) ? d - peak : d + 365.0 - peak;
        wave = cos(PI * since_peak / (365.0 - rise));               /* +1 -> -1 */
    }
    return loc->avg_temp + (loc->season_range / 2.0) * wave;
}

/* Monsoon factor for rain probability (month 1..12). */
double climate_rain_factor(const Location *loc, int month)
{
    if (loc->latitude >= 5.0 && loc->latitude < 35.0) {
        if (month >= 6 && month <= 9)   return 1.6;     /* monsoon        */
        if (month == 5 || month == 10)  return 1.1;
        if (month >= 3 && month <= 4)   return 0.7;     /* pre-monsoon    */
        return 0.3;                                     /* dry winter     */
    }
    return 1.0;
}

/* Pure model value (no observation correction). */
static void model_at(const Location *loc, time_t t, HourlyForecast *h)
{
    struct tm lt;
    double hour, rain_factor, daily_range, diurnal, mean, anomaly;
    double rain_base, rain, storm, sn, temp, hum, wind, pressure, alat;

    location_local_tm(loc, t, &lt);
    hour        = lt.tm_hour + lt.tm_min / 60.0;
    rain_factor = climate_rain_factor(loc, lt.tm_mon + 1);
    alat        = fabs(loc->latitude);

    /* storms: rare periods when the storm noise is very high */
    sn    = smooth_noise(loc->id, CH_STORM, t, 2L * 86400L);
    storm = sn > 0.8 ? (sn - 0.8) / 0.2 : 0.0;

    /* rain probability */
    rain_base = loc->rain_tendency * rain_factor;
    if (rain_base > 85.0) rain_base = 85.0;
    rain = rain_base
         + (20.0 + 15.0 * rain_factor) * smooth_noise(loc->id, CH_RAIN, t, 43200L)
         + ((hour >= 13.0 && hour <= 19.0) ? 8.0 * rain_factor : 0.0)   /* afternoon showers */
         + storm * 45.0;
    rain = clampd(rain, 0.0, 100.0);

    /* temperature: dry season has a bigger day/night difference */
    daily_range = loc->daily_range * (1.3 - 0.3 * rain_factor);
    mean    = climate_daily_mean(loc, lt.tm_yday);
    anomaly = 2.5 * smooth_noise(loc->id, CH_TEMP, t, 86400L);
    diurnal = (daily_range / 2.0) * sin(2.0 * PI * (hour - 9.0) / 24.0);
    temp    = mean + anomaly + diurnal;
    if (diurnal > 0) temp -= (rain / 100.0) * daily_range * 0.35;   /* clouds block the sun */

    /* humidity: higher at night and when it rains */
    hum = loc->avg_humidity + (rain_factor - 1.0) * 15.0 - diurnal * 1.8
        + (rain - 40.0) * 0.15 + 5.0 * smooth_noise(loc->id, CH_HUMIDITY, t, 21600L);
    hum = clampd(hum, 10.0, 100.0);

    /* wind: stronger in the afternoon, at high latitudes and in storms */
    wind = 6.0 + 0.12 * alat
         + 5.0 * (1.0 + smooth_noise(loc->id, CH_WIND, t, 28800L))
         + 3.0 * sin(2.0 * PI * (hour - 9.0) / 24.0)
         + storm * 50.0;
    wind = clampd(wind, 0.0, 200.0);

    /* pressure: lower when it rains / storms, small twice-a-day tide */
    pressure = 1013.0 - 0.10 * (rain - 40.0)
             + 5.0 * smooth_noise(loc->id, CH_PRESSURE, t, 2L * 86400L)
             - storm * 20.0
             + 1.0 * cos(4.0 * PI * hour / 24.0);

    h->time        = t;
    h->temperature = temp;
    h->humidity    = (int)(hum + 0.5);
    h->wind_speed  = wind;
    h->wind_dir    = ((int)(180.0 + 179.0 * smooth_noise(loc->id, CH_WIND_DIR, t, 86400L)) + 360) % 360;
    h->rain_chance = (int)(rain + 0.5);
    h->pressure    = pressure;
}

/*
 * Expected rain amount (mm per hour). Higher probability means more
 * intense rain; warm air holds more water, so cold places get less.
 */
static double rain_amount(int rain_chance, double temp, double wind)
{
    double x, warmth;
    if (rain_chance < 40) return 0.0;
    x = (rain_chance - 40) / 60.0;                    /* 0 .. 1 */
    warmth = clampd((temp + 10.0) / 35.0, 0.2, 1.0);  /* Clausius-Clapeyron idea */
    return x * x * 6.0 * warmth * (wind > 40.0 ? 1.5 : 1.0);
}

static void finish_point(HourlyForecast *h)
{
    h->humidity    = clampi(h->humidity, 5, 100);
    h->rain_chance = clampi(h->rain_chance, 0, 100);
    if (h->wind_speed < 0) h->wind_speed = 0;
    h->rainfall_mm = rain_amount(h->rain_chance, h->temperature, h->wind_speed);
    h->condition   = condition_derive(h->temperature, h->humidity, h->rain_chance,
                                      h->wind_speed, h->pressure);
}

void forecast_point(const Location *loc, const Weather *obs, time_t t, HourlyForecast *out)
{
    model_at(loc, t, out);

    /* persistence correction from the latest observation */
    if (obs != NULL) {
        time_t obs_t = parse_time(obs->updated);
        if (obs_t != (time_t)-1) {
            double hours = fabs(difftime(t, obs_t)) / 3600.0;
            if (hours <= 48.0) {
                HourlyForecast m;
                double w = exp(-hours / 12.0);
                model_at(loc, obs_t, &m);
                out->temperature += (obs->temperature - m.temperature) * w;
                out->humidity    += (int)((obs->humidity - m.humidity) * w);
                out->wind_speed  += (obs->wind_speed - m.wind_speed) * w;
                out->rain_chance += (int)((obs->rain_chance - m.rain_chance) * w);
                out->pressure    += (obs->pressure - m.pressure) * w;
            }
        }
    }
    finish_point(out);
}

void forecast_simulate_current(const Location *loc, time_t now, Weather *out)
{
    HourlyForecast h;
    model_at(loc, now, &h);
    finish_point(&h);

    memset(out, 0, sizeof *out);
    out->location_id = loc->id;
    out->temperature = floor(h.temperature * 10.0 + 0.5) / 10.0;
    out->humidity    = h.humidity;
    out->wind_speed  = floor(h.wind_speed * 10.0 + 0.5) / 10.0;
    out->wind_dir    = h.wind_dir;
    out->rain_chance = h.rain_chance;
    out->rainfall_mm = floor(h.rainfall_mm * 10.0 + 0.5) / 10.0;
    out->pressure    = floor(h.pressure * 10.0 + 0.5) / 10.0;
    out->condition   = h.condition;
    out->source      = SOURCE_AUTO;
    format_time(now, out->updated, sizeof out->updated);
}

/* Local midnight (as a normal time_t) of the day containing t. */
static time_t local_midnight(const Location *loc, time_t t)
{
    long long offset = (long long)(loc->utc_offset * 3600.0);
    long long local  = (long long)t + offset;
    long long day    = local / 86400LL;
    if (local < 0 && local % 86400LL != 0) day--;
    return (time_t)(day * 86400LL - offset);
}

int forecast_hourly(const Location *loc, const Weather *obs, time_t now,
                    HourlyForecast out[], int hours)
{
    long long offset = (long long)(loc->utc_offset * 3600.0);
    long long local  = (long long)now + offset;
    time_t start = (time_t)(local - (local % 3600LL) - offset + 3600LL);  /* next full hour */
    int i;

    for (i = 0; i < hours; i++)
        forecast_point(loc, obs, start + (time_t)i * 3600, &out[i]);
    return hours;
}

int forecast_daily(const Location *loc, const Weather *obs, time_t now,
                   DailyForecast out[], int days)
{
    time_t day0 = local_midnight(loc, now);
    int d, h;

    for (d = 0; d < days; d++) {
        DailyForecast *df = &out[d];
        double hum_sum = 0, pres_sum = 0, min_pres = 2000;
        int fog_hours = 0;

        memset(df, 0, sizeof *df);
        df->date = day0 + (time_t)d * 86400;
        df->temp_min = 1000;
        df->temp_max = -1000;

        for (h = 0; h < 24; h++) {
            HourlyForecast hf;
            forecast_point(loc, obs, df->date + (time_t)h * 3600, &hf);

            if (hf.temperature < df->temp_min) df->temp_min = hf.temperature;
            if (hf.temperature > df->temp_max) df->temp_max = hf.temperature;
            if (hf.wind_speed > df->wind_speed) df->wind_speed = hf.wind_speed;
            if (hf.rain_chance > df->rain_chance) df->rain_chance = hf.rain_chance;
            if (hf.pressure < min_pres) min_pres = hf.pressure;
            if (hf.condition == COND_FOG) fog_hours++;
            if (h == 12) df->wind_dir = hf.wind_dir;
            hum_sum  += hf.humidity;
            pres_sum += hf.pressure;
            df->rainfall_mm += hf.rainfall_mm;
        }
        df->humidity = (int)(hum_sum / 24.0 + 0.5);
        df->pressure = pres_sum / 24.0;
        df->condition = condition_derive(df->temp_max, df->humidity, df->rain_chance,
                                         df->wind_speed, min_pres);
        if (fog_hours >= 3 && df->rain_chance < 50) df->condition = COND_FOG;
    }
    return days;
}

/* ===================== location.c ===================== */
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


static Location g_locations[MAX_LOCATIONS];
static int      g_location_count = 0;

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

    g_location_count = 0;
    data_path(FILE_LOCATIONS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) {
        location_seed_defaults();
        return g_location_count;
    }
    while (fgets(line, sizeof line, fp) != NULL && g_location_count < MAX_LOCATIONS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_location(line, &g_locations[g_location_count])) g_location_count++;
    }
    fclose(fp);
    return g_location_count;
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
    for (i = 0; i < g_location_count; i++) {
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
    g_location_count = 0;
    for (i = 0; i < n && i < MAX_LOCATIONS; i++)
        g_locations[g_location_count++] = DEFAULT_LOCATIONS[i];
    location_save();
}

/* =============================================================
 *  ACCESS
 * ============================================================= */

int location_count(void) { return g_location_count; }

Location *location_at(int index)
{
    if (index < 0 || index >= g_location_count) return NULL;
    return &g_locations[index];
}

Location *location_find(int id)
{
    int i;
    for (i = 0; i < g_location_count; i++)
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

    for (i = 0; i < g_location_count && found < max; i++)
        if (str_iequals(g_locations[i].name, query))
            ids[found++] = g_locations[i].id;

    for (i = 0; i < g_location_count && found < max; i++) {
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

    for (i = 0; i < g_location_count; i++) {
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
    for (i = 0; i < g_location_count; i++)
        if (g_locations[i].id != except_id && str_iequals(g_locations[i].name, name))
            return 1;
    return 0;
}

int location_add(Location *loc)
{
    int i, max_id = 0;

    if (g_location_count >= MAX_LOCATIONS) return -1;
    for (i = 0; i < g_location_count; i++)
        if (g_locations[i].id > max_id) max_id = g_locations[i].id;

    loc->id = max_id + 1;
    sanitize_field(loc->name);
    sanitize_field(loc->region);
    g_locations[g_location_count++] = *loc;
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
    for (i = 0; i < g_location_count; i++) {
        if (g_locations[i].id == id) {
            for (; i < g_location_count - 1; i++) g_locations[i] = g_locations[i + 1];
            g_location_count--;
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

    for (i = 0; i < g_location_count; i++) {
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

/* ===================== logger.c ===================== */
/*
 * logger.c
 * ---------------------------------------------------------------
 * Appends one line per event to data/system.log, for example:
 *   2026-11-13 10:20:11 | LOGIN | user 'rahim' logged in
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


void log_event(const char *type, const char *fmt, ...)
{
    char path[PATH_LEN];
    char when[TIME_LEN];
    char message[LINE_LEN];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    sanitize_field(message);

    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "a");
    if (fp == NULL) return;            /* logging must never stop the program */
    now_string(when, sizeof when);
    fprintf(fp, "%s | %-10s | %s\n", when, type, message);
    fclose(fp);
}

/*
 * Read the last max_lines lines of the log. A small ring buffer is
 * used so the whole file never has to be kept in memory.
 */
int log_tail(char lines[][LINE_LEN], int max_lines)
{
    char path[PATH_LEN];
    char buf[LINE_LEN];
    FILE *fp;
    int total = 0, i, start, count;
    static char ring[64][LINE_LEN];

    if (max_lines > 64) max_lines = 64;
    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;

    while (fgets(buf, sizeof buf, fp) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
        str_copy(ring[total % max_lines], buf, LINE_LEN);
        total++;
    }
    fclose(fp);

    count = total < max_lines ? total : max_lines;
    start = total < max_lines ? 0 : total % max_lines;
    for (i = 0; i < count; i++)
        str_copy(lines[i], ring[(start + i) % max_lines], LINE_LEN);
    return count;
}

/* Count how many log lines have the given event type. */
long log_count_type(const char *type)
{
    char path[PATH_LEN];
    char buf[LINE_LEN];
    char pattern[64];
    FILE *fp;
    long count = 0;

    snprintf(pattern, sizeof pattern, "| %-10s |", type);
    data_path(FILE_LOG, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(buf, sizeof buf, fp) != NULL)
        if (strstr(buf, pattern) != NULL) count++;
    fclose(fp);
    return count;
}

/* ===================== main.c ===================== */
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
        if (admin_login(username, password)) {   /* admin used the user login */
            log_event("LOGIN", "administrator logged in");
            admin_session_run();
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
        ui_color(COL_MUTED);
        printf("\n  Admin login: admin / admin123  (option 3)\n");
        ui_reset();
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

/* ===================== report.c ===================== */
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

static void report_loc_time(const Location *l, time_t t, const char *fmt, char *buf, size_t size)
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
        report_loc_time(l, h[i].time, "%d %b %H:%M", buf, sizeof buf);
        fprintf(fp, "%-11s %5.1fC %4d%% %5.1f %-4s %4d%% %6.1f %8.1f  %s\n", buf,
                h[i].temperature, h[i].humidity, h[i].wind_speed,
                wind_direction_name(h[i].wind_dir), h[i].rain_chance, h[i].rainfall_mm,
                h[i].pressure, condition_name(h[i].condition));
    }

    fprintf(fp, "\n3. DAILY FORECAST (NEXT 7 DAYS)\n%s\n", LINE);
    fprintf(fp, "%-15s %6s %6s %5s %6s %5s %6s %6s  %s\n",
            "Day", "Min", "Max", "Rain", "mm", "Hum", "Wind", "Score", "Condition");
    for (i = 0; i < DAILY_COUNT; i++) {
        report_loc_time(l, d[i].date + 43200, "%a %d %b %Y", buf, sizeof buf);
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
    report_loc_time(l, d[order[0]].date + 43200, "%A %d %B", buf, sizeof buf);
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

/* ===================== ui.c ===================== */
/*
 * ui.c
 * ---------------------------------------------------------------
 * Console user interface implementation.
 *
 * Colours are produced with ANSI escape sequences. On Windows 10+
 * the "virtual terminal" mode is switched on so the same sequences
 * work in cmd.exe, PowerShell, Windows Terminal, VS Code and
 * Code::Blocks. When the terminal cannot show colours the program
 * falls back to the Classic (plain text) theme automatically.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L     /* for fileno() and termios */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>      /* _getch */
#include <io.h>         /* _isatty */
#define IS_TTY(f) _isatty(_fileno(f))
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>     /* isatty */
#include <termios.h>    /* hide password characters */
#define IS_TTY(f) isatty(fileno(f))
#endif


static int  g_theme        = THEME_DARK;
static int  g_ansi_ok      = 0;   /* terminal understands colours  */
static int  g_stdout_tty   = 0;
static int  g_stdin_tty    = 0;
static int  g_echo_input   = 0;   /* echo piped input (for test logs) */
static char g_status[128]  = "";

/* Background + default text colour of each theme */
static const char *THEME_BASE[THEME_COUNT] = {
    "\033[0;40;97m",     /* Dark : black background, white text */
    "\033[0;107;30m",    /* Light: white background, black text */
    ""                   /* Classic: terminal default           */
};

/* Text colour for every role in every theme */
static const char *THEME_COLORS[THEME_COUNT][COL_COUNT] = {
    /* DARK */
    { "\033[22;97m", "\033[1;96m", "\033[22;93m", "\033[1;92m", "\033[1;93m",
      "\033[1;91m",  "\033[22;90m", "\033[1;95m", "\033[22;96m" },
    /* LIGHT */
    { "\033[22;30m", "\033[1;34m", "\033[22;35m", "\033[1;32m", "\033[1;33m",
      "\033[1;31m",  "\033[22;90m", "\033[1;35m", "\033[22;36m" },
    /* CLASSIC */
    { "", "", "", "", "", "", "", "", "" }
};

static const char *THEME_NAMES[THEME_COUNT] = { "Dark", "Light", "Classic (no colour)" };

/* =============================================================
 *  SETUP
 * ============================================================= */

void ui_init(void)
{
    const char *echo = getenv("WFS_ECHO_INPUT");

    g_stdout_tty = IS_TTY(stdout) ? 1 : 0;
    g_stdin_tty  = IS_TTY(stdin) ? 1 : 0;
    g_echo_input = (echo != NULL && echo[0] == '1');

#ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        g_ansi_ok = 0;
        if (g_stdout_tty && GetConsoleMode(h, &mode) &&
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            g_ansi_ok = 1;
    }
#else
    g_ansi_ok = g_stdout_tty;
#endif
    if (getenv("NO_COLOR") != NULL) g_ansi_ok = 0;
}

/* Put the terminal back to its normal colours before exiting. */
void ui_restore(void)
{
    if (g_ansi_ok) printf("\033[0m");
#ifdef _WIN32
    if (!g_ansi_ok && g_stdout_tty) system("color 07");
#endif
    fflush(stdout);
}

void ui_set_theme(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) theme = THEME_DARK;
    g_theme = theme;
#ifdef _WIN32
    /* Old Windows console without ANSI support: use the "color" command */
    if (!g_ansi_ok && g_stdout_tty)
        system(theme == THEME_LIGHT ? "color F0" : "color 07");
#endif
    ui_reset();
}

int ui_get_theme(void) { return g_theme; }

const char *ui_theme_name(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) return "Unknown";
    return THEME_NAMES[theme];
}

void ui_set_status(const char *status)
{
    str_copy(g_status, status ? status : "", sizeof g_status);
}

/* =============================================================
 *  COLOURS
 * ============================================================= */

void ui_color(ColorRole role)
{
    if (!g_ansi_ok || role < 0 || role >= COL_COUNT) return;
    fputs(THEME_COLORS[g_theme][role], stdout);
}

void ui_reset(void)
{
    if (!g_ansi_ok) return;
    fputs(THEME_BASE[g_theme], stdout);
}

/* =============================================================
 *  LAYOUT
 * ============================================================= */

void ui_clear(void)
{
    if (!g_stdout_tty) {             /* output redirected: just separate screens */
        printf("\n");
        return;
    }
    if (g_ansi_ok) {
        ui_reset();
        printf("\033[2J\033[3J\033[H");
    } else {
#ifdef _WIN32
        system("cls");
#else
        printf("\033[2J\033[H");
#endif
    }
    fflush(stdout);
}

void ui_divider(char ch)
{
    int i;
    ui_color(COL_MUTED);
    for (i = 0; i < UI_WIDTH; i++) putchar(ch);
    putchar('\n');
    ui_reset();
}

void ui_center(const char *text, int width)
{
    int len = (int)strlen(text);
    int left = (width - len) / 2;
    if (left < 0) left = 0;
    printf("%*s%s", left, "", text);
}

/* Large banner shown on the main menu */
void ui_banner(void)
{
    ui_clear();
    ui_color(COL_WARNING);
    printf("       \\  |  /    ");
    ui_color(COL_TITLE);
    printf(" ________________________________________________________\n");
    ui_color(COL_WARNING);
    printf("     '-.;;;.-'    ");
    ui_color(COL_TITLE);
    printf("|                                                        |\n");
    ui_color(COL_WARNING);
    printf("    -==;;;;;==-   ");
    ui_color(COL_TITLE);
    printf("|      W E A T H E R   F O R E C A S T I N G             |\n");
    ui_color(COL_INFO);
    printf("      .-~~~~-.    ");
    ui_color(COL_TITLE);
    printf("|                    S Y S T E M                         |\n");
    ui_color(COL_INFO);
    printf("   .-(  ~~~~  )-. ");
    ui_color(COL_TITLE);
    printf("|________________________________________________________|\n");
    ui_color(COL_INFO);
    printf("  (_______________)");
    ui_color(COL_MUTED);
    printf("  Version %s  |  Written in C  |  File based storage\n", APP_VERSION);
    ui_color(COL_INFO);
    printf("    ' ' ' ' ' '\n");
    ui_reset();
    ui_divider('=');
}

/* Small header shown on every inner screen */
void ui_header(const char *title)
{
    char date[TIME_LEN];

    ui_clear();
    format_time_short(time(NULL), date, sizeof date);

    ui_color(COL_TITLE);
    printf("+");
    { int i; for (i = 0; i < UI_WIDTH - 2; i++) putchar('='); }
    printf("+\n|");
    ui_color(COL_WARNING);
    printf("  (*) %-40s", "WEATHER FORECASTING SYSTEM");
    ui_color(COL_MUTED);
    printf("%28s  ", date);
    ui_color(COL_TITLE);
    printf("|\n+");
    { int i; for (i = 0; i < UI_WIDTH - 2; i++) putchar('='); }
    printf("+\n");
    ui_reset();

    if (g_status[0] != '\0') {
        ui_color(COL_MUTED);
        printf("  %s\n", g_status);
        ui_reset();
    }
    printf("\n");
    ui_color(COL_HIGHLIGHT);
    printf("  >> %s\n", title);
    ui_reset();
    ui_divider('-');
}

void ui_section(const char *title)
{
    printf("\n");
    ui_color(COL_TITLE);
    printf("  %s\n", title);
    ui_reset();
    ui_divider('-');
}

void ui_menu_item(int key, const char *text)
{
    ui_color(COL_ACCENT);
    printf("   [%2d] ", key);
    ui_reset();
    printf("%s\n", text);
}

void ui_menu_item_tag(int key, const char *text, const char *tag)
{
    ui_color(COL_ACCENT);
    printf("   [%2d] ", key);
    ui_reset();
    printf("%s ", text);
    ui_color(COL_HIGHLIGHT);
    printf("%s\n", tag);
    ui_reset();
}

/* Draw a horizontal bar, e.g. ########.... (fraction 0..1) */
void ui_bar(double fraction, int width, char ch)
{
    int i, filled;
    if (fraction < 0) fraction = 0;
    if (fraction > 1) fraction = 1;
    filled = (int)(fraction * width + 0.5);
    for (i = 0; i < width; i++) putchar(i < filled ? ch : ' ');
}

/* =============================================================
 *  MESSAGES
 * ============================================================= */

static void print_message(ColorRole role, const char *label, const char *fmt, va_list ap)
{
    ui_color(role);
    printf("  %s ", label);
    vprintf(fmt, ap);
    printf("\n");
    ui_reset();
}

void ui_msg_success(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_SUCCESS, "[OK]", fmt, ap);
    va_end(ap);
}

void ui_msg_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_ERROR, "[ERROR]", fmt, ap);
    va_end(ap);
}

void ui_msg_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_WARNING, "[!]", fmt, ap);
    va_end(ap);
}

void ui_msg_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_INFO, "[i]", fmt, ap);
    va_end(ap);
}

/* =============================================================
 *  INPUT
 * ============================================================= */

/* Called when the input stream ends (Ctrl+Z / Ctrl+D or end of a test script). */
static void input_end(void)
{
    printf("\n");
    ui_msg_info("Input closed. Goodbye!");
    ui_restore();
    exit(0);
}

/* Read one line from stdin, remove the new line, flush overflow. */
static int read_raw_line(char *buf, size_t size)
{
    size_t len;

    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) input_end();

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
    } else {
        int c;   /* line was longer than the buffer: discard the rest */
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    if (!g_stdin_tty && g_echo_input) printf("%s\n", buf);
    return (int)len;
}

static void print_prompt(const char *prompt)
{
    ui_color(COL_ACCENT);
    printf("  %s", prompt);
    ui_reset();
    fflush(stdout);
}

int input_line(const char *prompt, char *buf, size_t size)
{
    print_prompt(prompt);
    read_raw_line(buf, size);
    str_trim(buf);
    return (int)strlen(buf);
}

int input_int(const char *prompt, int min, int max)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && v >= min && v <= max)
            return (int)v;
        ui_msg_error("Please enter a whole number between %d and %d.", min, max);
    }
}

int input_int_default(const char *prompt, int min, int max, int def)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (buf[0] == '\0') return def;
        v = strtol(buf, &end, 10);
        if (*end == '\0' && v >= min && v <= max) return (int)v;
        ui_msg_error("Please enter a whole number between %d and %d (Enter = %d).",
                     min, max, def);
    }
}

double input_double(const char *prompt, double min, double max)
{
    char buf[64];
    char *end;
    double v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        v = strtod(buf, &end);
        if (buf[0] != '\0' && *end == '\0' && v >= min && v <= max) return v;
        ui_msg_error("Please enter a number between %.1f and %.1f.", min, max);
    }
}

double input_double_default(const char *prompt, double min, double max, double def)
{
    char buf[64];
    char *end;
    double v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (buf[0] == '\0') return def;
        v = strtod(buf, &end);
        if (*end == '\0' && v >= min && v <= max) return v;
        ui_msg_error("Please enter a number between %.1f and %.1f (Enter = %.1f).",
                     min, max, def);
    }
}

int input_yes_no(const char *prompt)
{
    char buf[16];
    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (str_iequals(buf, "y") || str_iequals(buf, "yes")) return 1;
        if (str_iequals(buf, "n") || str_iequals(buf, "no"))  return 0;
        ui_msg_error("Please answer y (yes) or n (no).");
    }
}

/* Password input: characters are shown as '*' when typing in a terminal. */
void input_password(const char *prompt, char *buf, size_t size)
{
    size_t len = 0;

    if (!g_stdin_tty) {                /* input comes from a file / pipe */
        print_prompt(prompt);
        read_raw_line(buf, size);
        return;
    }

    print_prompt(prompt);
#ifdef _WIN32
    for (;;) {
        int c = _getch();
        if (c == '\r' || c == '\n') break;
        if (c == 3) input_end();                    /* Ctrl+C */
        if (c == 8) {                               /* Backspace */
            if (len > 0) { len--; printf("\b \b"); }
            continue;
        }
        if (c == 0 || c == 224) { _getch(); continue; } /* arrow / function keys */
        if (len + 1 < size && c >= 32 && c < 127) {
            buf[len++] = (char)c;
            putchar('*');
        }
        fflush(stdout);
    }
#else
    {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= (tcflag_t)~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        for (;;) {
            int c = getchar();
            if (c == EOF) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); input_end(); }
            if (c == '\n' || c == '\r') break;
            if (c == 127 || c == 8) {
                if (len > 0) { len--; printf("\b \b"); fflush(stdout); }
                continue;
            }
            if (len + 1 < size && c >= 32 && c < 127) {
                buf[len++] = (char)c;
                putchar('*');
                fflush(stdout);
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
#endif
    buf[len] = '\0';
    printf("\n");
}

void ui_pause(void)
{
    char buf[8];
    printf("\n");
    ui_color(COL_MUTED);
    printf("  Press Enter to continue...");
    ui_reset();
    read_raw_line(buf, sizeof buf);
}

/* ===================== user.c ===================== */
/*
 * user.c
 * ---------------------------------------------------------------
 * User account management.
 *
 * data/users.txt:
 *   username|password_hash|full_name|home_location|theme|active|created|last_login
 * data/admin.txt:
 *   username|password_hash        (default account: admin / admin123)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin123"

static User g_users[MAX_USERS];
static int  g_user_count = 0;

static char g_admin_user[USERNAME_LEN] = DEFAULT_ADMIN_USER;
static char g_admin_hash[HASH_LEN]     = "";

/* =============================================================
 *  STORAGE
 * ============================================================= */

static int parse_user(char *line, User *u)
{
    char *f[9];
    int n = split_fields(line, FIELD_SEP, f, 9);
    if (n < 8) return 0;

    memset(u, 0, sizeof *u);
    str_copy(u->username,      f[0], sizeof u->username);
    str_copy(u->password_hash, f[1], sizeof u->password_hash);
    str_copy(u->full_name,     f[2], sizeof u->full_name);
    u->home_location = atoi(f[3]);
    u->theme         = atoi(f[4]);
    u->active        = atoi(f[5]) ? 1 : 0;
    str_copy(u->created,    f[6], sizeof u->created);
    str_copy(u->last_login, f[7], sizeof u->last_login);
    return u->username[0] != '\0';
}

int user_load(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    g_user_count = 0;
    data_path(FILE_USERS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL && g_user_count < MAX_USERS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_user(line, &g_users[g_user_count])) g_user_count++;
    }
    fclose(fp);
    return g_user_count;
}

int user_save(void)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    FILE *fp;
    int i;

    data_path(FILE_USERS, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# username|password_hash|full_name|home_location|theme(0=dark,1=light,"
                "2=classic)|active|created|last_login\n");
    for (i = 0; i < g_user_count; i++) {
        const User *u = &g_users[i];
        fprintf(fp, "%s|%s|%s|%d|%d|%d|%s|%s\n", u->username, u->password_hash,
                u->full_name, u->home_location, u->theme, u->active,
                u->created, u->last_login[0] ? u->last_login : "-");
    }
    fclose(fp);
    return replace_file(tmp, path);
}

/* =============================================================
 *  ACCESS
 * ============================================================= */

int user_count(void) { return g_user_count; }

User *user_at(int index)
{
    if (index < 0 || index >= g_user_count) return NULL;
    return &g_users[index];
}

/* Usernames are case-insensitive: "Rahim" and "rahim" are the same. */
User *user_find(const char *username)
{
    int i;
    for (i = 0; i < g_user_count; i++)
        if (str_iequals(g_users[i].username, username)) return &g_users[i];
    return NULL;
}

/* =============================================================
 *  VALIDATION
 * ============================================================= */

int validate_username(const char *u, char *err, size_t size)
{
    size_t len = strlen(u), i;

    if (len < 3 || len > USERNAME_LEN - 1) {
        snprintf(err, size, "Username must be 3-%d characters long.", USERNAME_LEN - 1);
        return 0;
    }
    if (!isalpha((unsigned char)u[0])) {
        snprintf(err, size, "Username must start with a letter.");
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!isalnum((unsigned char)u[i]) && u[i] != '_') {
            snprintf(err, size, "Use only letters, digits and underscore (_).");
            return 0;
        }
    }
    if (str_iequals(u, g_admin_user) || str_iequals(u, "admin")) {
        snprintf(err, size, "This username is reserved.");
        return 0;
    }
    return 1;
}

int validate_password(const char *p, char *err, size_t size)
{
    size_t len = strlen(p), i;
    int letter = 0, digit = 0;

    if (len < MIN_PASSWORD_LEN || len > PASSWORD_LEN - 1) {
        snprintf(err, size, "Password must be %d-%d characters long.",
                 MIN_PASSWORD_LEN, PASSWORD_LEN - 1);
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (isalpha((unsigned char)p[i])) letter = 1;
        else if (isdigit((unsigned char)p[i])) digit = 1;
        if (p[i] == ' ' || p[i] == FIELD_SEP) {
            snprintf(err, size, "Password must not contain spaces or '|'.");
            return 0;
        }
    }
    if (!letter || !digit) {
        snprintf(err, size, "Password must contain at least one letter and one digit.");
        return 0;
    }
    return 1;
}

int validate_full_name(const char *name, char *err, size_t size)
{
    size_t len = strlen(name), i;
    if (len < 2 || len > NAME_LEN - 1) {
        snprintf(err, size, "Name must be 2-%d characters long.", NAME_LEN - 1);
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!isalpha(c) && c != ' ' && c != '.' && c != '-' && c != '\'') {
            snprintf(err, size, "Name may contain only letters, spaces, '.', '-' and '.");
            return 0;
        }
    }
    return 1;
}

/* =============================================================
 *  ACCOUNT OPERATIONS
 * ============================================================= */

/* Returns 0 on success, -1 username taken, -2 storage full, -3 file error. */
int user_register(const char *username, const char *password,
                  const char *full_name, int home_location)
{
    User u;

    if (user_find(username) != NULL) return -1;
    if (g_user_count >= MAX_USERS) return -2;

    memset(&u, 0, sizeof u);
    str_copy(u.username, username, sizeof u.username);
    hash_password(username, password, u.password_hash);
    str_copy(u.full_name, full_name, sizeof u.full_name);
    sanitize_field(u.full_name);
    u.home_location = home_location;
    u.theme = 0;
    u.active = 1;
    now_string(u.created, sizeof u.created);
    str_copy(u.last_login, "-", sizeof u.last_login);

    g_users[g_user_count++] = u;
    return user_save() == 0 ? 0 : -3;
}

AuthResult user_login(const char *username, const char *password)
{
    char hash[HASH_LEN];
    User *u = user_find(username);

    if (u == NULL) return AUTH_NO_USER;
    hash_password(u->username, password, hash);
    if (strcmp(hash, u->password_hash) != 0) return AUTH_WRONG_PASSWORD;
    if (!u->active) return AUTH_BLOCKED;

    now_string(u->last_login, sizeof u->last_login);
    user_save();
    return AUTH_OK;
}

int user_check_password(const char *username, const char *password)
{
    char hash[HASH_LEN];
    User *u = user_find(username);
    if (u == NULL) return 0;
    hash_password(u->username, password, hash);
    return strcmp(hash, u->password_hash) == 0;
}

int user_change_password(const char *username, const char *new_password)
{
    User *u = user_find(username);
    if (u == NULL) return -1;
    hash_password(u->username, new_password, u->password_hash);
    return user_save();
}

int user_delete(const char *username)
{
    int i;
    for (i = 0; i < g_user_count; i++) {
        if (str_iequals(g_users[i].username, username)) {
            for (; i < g_user_count - 1; i++) g_users[i] = g_users[i + 1];
            g_user_count--;
            return user_save();
        }
    }
    return -1;
}

int user_set_active(const char *username, int active)
{
    User *u = user_find(username);
    if (u == NULL) return -1;
    u->active = active ? 1 : 0;
    return user_save();
}

int user_update(const User *changed)
{
    User *u = user_find(changed->username);
    if (u == NULL) return -1;
    *u = *changed;
    sanitize_field(u->full_name);
    return user_save();
}

/* When a location is deleted, users who had it as home get a replacement. */
int user_clear_home_location(int location_id, int replacement)
{
    int i, changed = 0;
    for (i = 0; i < g_user_count; i++) {
        if (g_users[i].home_location == location_id) {
            g_users[i].home_location = replacement;
            changed++;
        }
    }
    if (changed) user_save();
    return changed;
}

/* =============================================================
 *  ADMINISTRATOR
 * ============================================================= */

static int admin_save(void)
{
    char path[PATH_LEN];
    FILE *fp;
    data_path(FILE_ADMIN, path, sizeof path);
    fp = fopen(path, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# username|password_hash   (default login: admin / admin123)\n");
    fprintf(fp, "%s|%s\n", g_admin_user, g_admin_hash);
    fclose(fp);
    return 0;
}

/* Load admin.txt, or create it with the default account. */
void admin_ensure_default(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    data_path(FILE_ADMIN, path, sizeof path);
    fp = fopen(path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof line, fp) != NULL) {
            char *f[2];
            if (line[0] == '#') continue;
            if (split_fields(line, FIELD_SEP, f, 2) == 2) {
                str_copy(g_admin_user, f[0], sizeof g_admin_user);
                str_copy(g_admin_hash, f[1], sizeof g_admin_hash);
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    str_copy(g_admin_user, DEFAULT_ADMIN_USER, sizeof g_admin_user);
    hash_password(DEFAULT_ADMIN_USER, DEFAULT_ADMIN_PASS, g_admin_hash);
    admin_save();
}

int admin_login(const char *username, const char *password)
{
    char hash[HASH_LEN];
    if (!str_iequals(username, g_admin_user)) return 0;
    hash_password(g_admin_user, password, hash);
    return strcmp(hash, g_admin_hash) == 0;
}

int admin_change_password(const char *new_password)
{
    hash_password(g_admin_user, new_password, g_admin_hash);
    return admin_save();
}

const char *admin_username(void) { return g_admin_user; }

/* ===================== user_menu.c ===================== */
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

/* ===================== userdata.c ===================== */
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

/* ===================== utils.c ===================== */
/*
 * utils.c
 * ---------------------------------------------------------------
 * Implementation of the helper functions declared in utils.h.
 * All functions are written in portable C99 so that the project
 * compiles with GCC on Linux, macOS and Windows (MinGW).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>     /* _mkdir */
#endif


static char g_data_dir[PATH_LEN]   = DEFAULT_DATA_DIR;
static char g_report_dir[PATH_LEN] = DEFAULT_REPORT_DIR;

/* =============================================================
 *  STRING HELPERS
 * ============================================================= */

/* Remove leading and trailing white space (in place). */
void str_trim(char *s)
{
    char *start;
    size_t len;

    if (s == NULL) return;

    start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

/* Safe string copy that always terminates the destination. */
void str_copy(char *dst, const char *src, size_t size)
{
    if (dst == NULL || size == 0) return;
    if (src == NULL) { dst[0] = '\0'; return; }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

void str_to_lower(char *dst, const char *src, size_t size)
{
    size_t i;
    if (size == 0) return;
    for (i = 0; src[i] != '\0' && i < size - 1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

int str_iequals(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Case-insensitive "does haystack contain needle?" */
int str_icontains(const char *haystack, const char *needle)
{
    char h[LINE_LEN], n[LINE_LEN];
    str_to_lower(h, haystack, sizeof h);
    str_to_lower(n, needle, sizeof n);
    return strstr(h, n) != NULL;
}

int str_is_number(const char *s)
{
    if (s == NULL || *s == '\0') return 0;
    if (*s == '-' || *s == '+') s++;
    if (*s == '\0') return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Data files use '|' as separator, so it must never appear inside a field. */
void sanitize_field(char *s)
{
    for (; *s; s++) {
        if (*s == FIELD_SEP) *s = '/';
        else if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
    }
}

/*
 * Split a line into fields (in place). Unlike strtok() this keeps
 * empty fields, e.g. "a||c" gives three fields: "a", "", "c".
 * Returns the number of fields found.
 */
int split_fields(char *line, char sep, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;
    size_t len = strlen(line);

    /* strip the end-of-line characters */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';

    if (max_fields <= 0) return 0;
    fields[count++] = p;
    while (*p) {
        if (*p == sep) {
            *p = '\0';
            if (count >= max_fields) break;
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

/* Levenshtein edit distance, used for "Did you mean ...?" suggestions. */
int edit_distance(const char *a, const char *b)
{
    int prev[64], curr[64];
    int la = (int)strlen(a), lb = (int)strlen(b);
    int i, j;

    if (la > 63) la = 63;
    if (lb > 63) lb = 63;

    for (j = 0; j <= lb; j++) prev[j] = j;
    for (i = 1; i <= la; i++) {
        curr[0] = i;
        for (j = 1; j <= lb; j++) {
            int cost = (tolower((unsigned char)a[i - 1]) ==
                        tolower((unsigned char)b[j - 1])) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int best = del < ins ? del : ins;
            curr[j] = best < sub ? best : sub;
        }
        memcpy(prev, curr, sizeof(int) * (size_t)(lb + 1));
    }
    return prev[lb];
}

/* =============================================================
 *  TIME HELPERS
 * ============================================================= */

void format_time(time_t t, char *buf, size_t size)
{
    struct tm *tmv = localtime(&t);
    if (tmv == NULL) { str_copy(buf, "unknown", size); return; }
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tmv);
}

void format_time_short(time_t t, char *buf, size_t size)
{
    struct tm *tmv = localtime(&t);
    if (tmv == NULL) { str_copy(buf, "unknown", size); return; }
    strftime(buf, size, "%Y-%m-%d %H:%M", tmv);
}

/* Parse "YYYY-MM-DD HH:MM[:SS]" (local time). Returns -1 on error. */
time_t parse_time(const char *s)
{
    struct tm tmv;
    int y, mo, d, h = 0, mi = 0, se = 0;
    int n;

    if (s == NULL) return (time_t)-1;
    n = sscanf(s, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &se);
    if (n < 3) return (time_t)-1;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59)
        return (time_t)-1;

    memset(&tmv, 0, sizeof tmv);
    tmv.tm_year  = y - 1900;
    tmv.tm_mon   = mo - 1;
    tmv.tm_mday  = d;
    tmv.tm_hour  = h;
    tmv.tm_min   = mi;
    tmv.tm_sec   = se;
    tmv.tm_isdst = -1;
    return mktime(&tmv);
}

void now_string(char *buf, size_t size)
{
    format_time(time(NULL), buf, size);
}

/* Offset of the computer's clock from UTC, in seconds (e.g. +21600 for UTC+6). */
long system_utc_offset_seconds(void)
{
    time_t now = time(NULL);
    struct tm lt = *localtime(&now);
    struct tm gt = *gmtime(&now);
    gt.tm_isdst = lt.tm_isdst;
    return (long)difftime(mktime(&lt), mktime(&gt));
}

/* =============================================================
 *  PATH & FILE HELPERS
 * ============================================================= */

void set_data_dir(const char *dir)   { str_copy(g_data_dir, dir, sizeof g_data_dir); }
const char *get_data_dir(void)       { return g_data_dir; }
void set_report_dir(const char *dir) { str_copy(g_report_dir, dir, sizeof g_report_dir); }
const char *get_report_dir(void)     { return g_report_dir; }

/* Build "<data_dir>/<file>" */
void data_path(const char *file, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", g_data_dir, file);
}

/* Create a directory if it does not exist. Returns 0 on success. */
int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return 0;
#ifdef _WIN32
    return _mkdir(path) == 0 ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 ? 0 : -1;
#endif
}

int file_exists(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

long file_size(const char *path)
{
    long size;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return -1;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    return size;
}

/*
 * Replace final_path with tmp_path. On Windows rename() fails when
 * the destination exists, so the old file is removed first.
 */
int replace_file(const char *tmp_path, const char *final_path)
{
    remove(final_path);
    return rename(tmp_path, final_path) == 0 ? 0 : -1;
}

/* =============================================================
 *  PASSWORD HASHING
 * ============================================================= */

/*
 * Salted FNV-1a (64 bit) hash, repeated 1000 times.
 * The username is used as salt so two users with the same password
 * get different hashes. Plain text passwords are never stored.
 * (Good enough for a lab project - not a replacement for bcrypt.)
 */
void hash_password(const char *username, const char *password, char out[HASH_LEN])
{
    unsigned long long h = 1469598103934665603ULL;   /* FNV offset basis */
    const unsigned long long prime = 1099511628211ULL;
    char salted[LINE_LEN];
    char lower_user[USERNAME_LEN];
    int round;
    const char *p;

    str_to_lower(lower_user, username, sizeof lower_user);
    snprintf(salted, sizeof salted, "%s:%s:WFS", lower_user, password);

    for (round = 0; round < 1000; round++) {
        for (p = salted; *p; p++) {
            h ^= (unsigned char)*p;
            h *= prime;
        }
        h ^= (unsigned long long)round;
        h *= prime;
    }
    /* print as two 32-bit halves - works with every C runtime */
    snprintf(out, HASH_LEN, "%08lx%08lx",
             (unsigned long)((h >> 32) & 0xFFFFFFFFUL),
             (unsigned long)(h & 0xFFFFFFFFUL));
}

/* =============================================================
 *  DETERMINISTIC NOISE (same input -> same output on every PC)
 * ============================================================= */

uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t make_seed(int a, int b, int c)
{
    uint32_t h = hash32((uint32_t)a * 73856093U);
    h = hash32(h ^ ((uint32_t)b * 19349663U));
    h = hash32(h ^ ((uint32_t)c * 83492791U));
    return h;
}

double noise_unit(uint32_t seed)
{
    return (double)(hash32(seed) & 0xFFFFFFU) / 16777216.0;
}

double noise_signed(uint32_t seed)
{
    return noise_unit(seed) * 2.0 - 1.0;
}

/* =============================================================
 *  MATHS
 * ============================================================= */

double clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ===================== views.c ===================== */
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

/* ===================== weather.c ===================== */
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


static Weather g_weather[MAX_LOCATIONS];
static int     g_weather_count = 0;

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

    g_weather_count = 0;
    data_path(FILE_WEATHER, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;

    while (fgets(line, sizeof line, fp) != NULL && g_weather_count < MAX_LOCATIONS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_weather(line, &g_weather[g_weather_count])) g_weather_count++;
    }
    fclose(fp);
    return g_weather_count;
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
    for (i = 0; i < g_weather_count; i++) {
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

int weather_count(void) { return g_weather_count; }

Weather *weather_at(int index)
{
    if (index < 0 || index >= g_weather_count) return NULL;
    return &g_weather[index];
}

Weather *weather_get(int location_id)
{
    int i;
    for (i = 0; i < g_weather_count; i++)
        if (g_weather[i].location_id == location_id) return &g_weather[i];
    return NULL;
}

int weather_set(const Weather *w)
{
    Weather *existing = weather_get(w->location_id);
    if (existing != NULL) {
        *existing = *w;
    } else {
        if (g_weather_count >= MAX_LOCATIONS) return -1;
        g_weather[g_weather_count++] = *w;
    }
    return weather_save();
}

int weather_remove(int location_id)
{
    int i;
    for (i = 0; i < g_weather_count; i++) {
        if (g_weather[i].location_id == location_id) {
            for (; i < g_weather_count - 1; i++) g_weather[i] = g_weather[i + 1];
            g_weather_count--;
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
            } else if (g_weather_count < MAX_LOCATIONS) {
                g_weather[g_weather_count++] = fresh;
            }
            refreshed++;
        }
    }
    /* remove records that belong to deleted locations */
    for (i = 0; i < g_weather_count; i++) {
        if (location_find(g_weather[i].location_id) == NULL) {
            int j;
            for (j = i; j < g_weather_count - 1; j++) g_weather[j] = g_weather[j + 1];
            g_weather_count--;
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
