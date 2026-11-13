/*
 * test_main.c
 * ---------------------------------------------------------------
 * Automated unit tests for the Weather Forecasting System.
 * Every module except the interactive menus is tested here.
 *
 * Build & run:   make test
 * (uses its own temporary folders, the real data/ is not touched)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "utils.h"
#include "location.h"
#include "weather.h"
#include "forecast.h"
#include "advisory.h"
#include "alert.h"
#include "user.h"
#include "userdata.h"
#include "detect.h"
#include "report.h"
#include "logger.h"

#define TEST_DATA_DIR   "tests/tmp_data"
#define TEST_REPORT_DIR "tests/tmp_reports"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, name)                                                   \
    do {                                                                    \
        if (cond) { g_pass++; printf("  [PASS] %s\n", name); }              \
        else { g_fail++; printf("  [FAIL] %s   (line %d)\n", name, __LINE__); } \
    } while (0)

static void section(const char *title) { printf("\n== %s ==\n", title); }

/* Remove all data files so every test run starts from a clean state. */
static void clean_test_data(void)
{
    const char *files[] = { FILE_USERS, FILE_ADMIN, FILE_LOCATIONS, FILE_WEATHER,
                            FILE_ALERTS, FILE_FAVORITES, FILE_HISTORY,
                            FILE_DEVICE_GPS, FILE_LOG };
    char path[PATH_LEN];
    size_t i;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        data_path(files[i], path, sizeof path);
        remove(path);
    }
}

/* =============================================================
 *  TESTS
 * ============================================================= */

static void test_strings(void)
{
    char s[64];
    char line[64];
    char *f[5];
    int n;

    section("String helpers");
    strcpy(s, "   hello world  \n");
    str_trim(s);
    CHECK(strcmp(s, "hello world") == 0, "str_trim removes spaces at both ends");
    CHECK(str_iequals("Dhaka", "dHAKA"), "str_iequals ignores case");
    CHECK(!str_iequals("Dhaka", "Dhak"), "str_iequals detects different strings");
    CHECK(str_icontains("Cox's Bazar", "BAZAR"), "str_icontains finds substring, any case");
    CHECK(str_is_number("42") && !str_is_number("4a") && !str_is_number(""), "str_is_number");

    strcpy(line, "a||c|d\n");
    n = split_fields(line, '|', f, 5);
    CHECK(n == 4 && strcmp(f[1], "") == 0 && strcmp(f[3], "d") == 0,
          "split_fields keeps empty fields and strips new line");

    strcpy(s, "bad|name\n");
    sanitize_field(s);
    CHECK(strchr(s, '|') == NULL && strchr(s, '\n') == NULL, "sanitize_field removes '|' and new line");

    CHECK(edit_distance("dhaka", "daka") == 1, "edit_distance dhaka/daka = 1");
    CHECK(edit_distance("Bogra", "bogura") == 1, "edit_distance is case-insensitive");
    CHECK(edit_distance("abc", "abc") == 0, "edit_distance of equal strings = 0");
}

static void test_time_and_hash(void)
{
    char buf[TIME_LEN], h1[HASH_LEN], h2[HASH_LEN], h3[HASH_LEN];
    time_t now = time(NULL);
    int i, in_range = 1;

    section("Time, hashing and noise");
    format_time(now, buf, sizeof buf);
    CHECK(parse_time(buf) == now, "format_time / parse_time round trip");
    CHECK(parse_time("2026-13-01 10:00") == (time_t)-1, "parse_time rejects month 13");
    CHECK(parse_time("hello") == (time_t)-1, "parse_time rejects text");

    hash_password("rahim", "secret1", h1);
    hash_password("RAHIM", "secret1", h2);
    hash_password("karim", "secret1", h3);
    CHECK(strlen(h1) == 16, "hash is 16 hex characters");
    CHECK(strcmp(h1, h2) == 0, "hash uses case-insensitive username");
    CHECK(strcmp(h1, h3) != 0, "same password, different user -> different hash (salt)");
    CHECK(strstr(h1, "secret1") == NULL, "hash does not contain the password");

    for (i = 0; i < 1000; i++) {
        double a = noise_signed(make_seed(i, 2, 3));
        double b = noise_unit(make_seed(i, 5, 6));
        if (a < -1.0 || a > 1.0 || b < 0.0 || b >= 1.0) in_range = 0;
    }
    CHECK(in_range, "noise values stay in range");
    CHECK(noise_unit(make_seed(1, 2, 3)) == noise_unit(make_seed(1, 2, 3)), "noise is deterministic");
}

static void test_validation(void)
{
    char err[128];

    section("Input validation");
    CHECK(!validate_username("ab", err, sizeof err), "username too short is rejected");
    CHECK(!validate_username("1abc", err, sizeof err), "username starting with digit is rejected");
    CHECK(!validate_username("bad name", err, sizeof err), "username with space is rejected");
    CHECK(!validate_username("admin", err, sizeof err), "reserved username 'admin' is rejected");
    CHECK(validate_username("rahim_01", err, sizeof err), "valid username is accepted");

    CHECK(!validate_password("abc12", err, sizeof err), "password shorter than 6 is rejected");
    CHECK(!validate_password("abcdef", err, sizeof err), "password without digit is rejected");
    CHECK(!validate_password("123456", err, sizeof err), "password without letter is rejected");
    CHECK(!validate_password("abc 123", err, sizeof err), "password with space is rejected");
    CHECK(validate_password("abc123", err, sizeof err), "valid password is accepted");

    CHECK(validate_full_name("Md. Tamim Akando", err, sizeof err), "full name with dot is accepted");
    CHECK(!validate_full_name("X", err, sizeof err), "one-letter name is rejected");
    CHECK(!validate_full_name("R2D2", err, sizeof err), "name with digits is rejected");
}

static void test_locations(void)
{
    int ids[MAX_SEARCH_RESULT], n, id;
    double d;
    Location l;
    const Location *dhaka, *ctg;

    section("Locations");
    n = location_load();
    CHECK(n == 21, "default locations are created on first run (21)");
    location_load();
    CHECK(location_count() == 21, "locations are saved and loaded again");

    n = location_search("dha", ids, MAX_SEARCH_RESULT);
    CHECK(n >= 1 && ids[0] == 1, "search 'dha' finds Dhaka");
    n = location_search("DHAKA", ids, MAX_SEARCH_RESULT);
    CHECK(n >= 1 && ids[0] == 1, "exact name match is listed first");
    n = location_search("bangladesh", ids, MAX_SEARCH_RESULT);
    CHECK(n >= 12, "search by region finds all Bangladeshi cities");
    n = location_search("atlantis", ids, MAX_SEARCH_RESULT);
    CHECK(n == 0, "unknown name gives no result");
    n = location_suggest("Dhakka", ids, 3);
    CHECK(n >= 1 && ids[0] == 1, "suggestion for 'Dhakka' is Dhaka");

    dhaka = location_find(1);
    ctg = location_find(2);
    d = haversine_km(dhaka->latitude, dhaka->longitude, ctg->latitude, ctg->longitude);
    CHECK(d > 200.0 && d < 230.0, "Haversine distance Dhaka-Chattogram is about 214 km");

    id = location_nearest(24.8763, 89.3505, &d);
    CHECK(id == 3 && d < 10.0, "nearest location to Pundra University is Bogura");

    memset(&l, 0, sizeof l);
    strcpy(l.name, "Gaibandha");
    strcpy(l.region, "Rangpur Division, Bangladesh");
    l.latitude = 25.33; l.longitude = 89.53; l.utc_offset = 6.0;
    l.avg_temp = 25.0; l.season_range = 12.0; l.daily_range = 9.0;
    l.avg_humidity = 75; l.rain_tendency = 55;
    id = location_add(&l);
    CHECK(id == 22, "new location gets the next id (22)");
    CHECK(location_name_exists("gaibandha", 0), "duplicate name check is case-insensitive");
    location_load();
    CHECK(location_find(22) != NULL, "added location is stored in the file");
    CHECK(location_delete(22) == 0 && location_find(22) == NULL, "location can be deleted");
}

static void test_weather(void)
{
    Weather w;
    const Weather *got;
    time_t now = time(NULL);
    int refreshed;

    section("Weather data");
    CHECK(strcmp(wind_direction_name(0), "N") == 0, "0 degrees is N");
    CHECK(strcmp(wind_direction_name(45), "NE") == 0, "45 degrees is NE");
    CHECK(strcmp(wind_direction_name(180), "S") == 0, "180 degrees is S");
    CHECK(strcmp(wind_direction_name(225), "SW") == 0, "225 degrees is SW");
    CHECK(strcmp(wind_direction_name(359), "N") == 0, "359 degrees is N");

    CHECK(condition_derive(30, 60, 10, 10, 1012) == COND_CLEAR, "low rain chance -> Clear");
    CHECK(condition_derive(30, 60, 20, 10, 1012) == COND_PARTLY_CLOUDY, "20% rain -> Partly cloudy");
    CHECK(condition_derive(30, 60, 60, 10, 1012) == COND_LIGHT_RAIN, "60% rain -> Light rain");
    CHECK(condition_derive(30, 80, 80, 10, 1010) == COND_HEAVY_RAIN, "80% rain -> Heavy rain");
    CHECK(condition_derive(30, 80, 85, 50, 998) == COND_THUNDERSTORM, "rain + strong wind -> Thunderstorm");
    CHECK(condition_derive(-3, 80, 60, 10, 1010) == COND_SNOW, "rain below freezing -> Snow");
    CHECK(condition_derive(15, 97, 5, 3, 1015) == COND_FOG, "very humid, calm -> Fog");

    weather_load();
    refreshed = weather_sync(now, 0);
    CHECK(refreshed == location_count(), "automatic feed creates a reading for every location");
    CHECK(weather_sync(now, 0) == 0, "fresh readings are not refreshed again");

    got = weather_get(1);
    CHECK(got != NULL && got->humidity >= 0 && got->humidity <= 100 &&
          got->rain_chance >= 0 && got->rain_chance <= 100, "simulated reading has valid ranges");

    w = *got;
    w.temperature = 33.3;
    w.source = SOURCE_MANUAL;
    format_time(now, w.updated, sizeof w.updated);
    CHECK(weather_set(&w) == 0, "manual weather is saved");
    weather_load();
    CHECK(fabs(weather_get(1)->temperature - 33.3) < 0.01 && weather_get(1)->source == SOURCE_MANUAL,
          "manual weather is loaded back from the file");
    weather_sync(now + 3 * 3600, 0);
    CHECK(fabs(weather_get(1)->temperature - 33.3) < 0.01, "manual weather is kept for 3 hours");
    weather_sync(now + 7 * 3600, 0);
    CHECK(weather_get(1)->source == SOURCE_AUTO, "after 6 hours the automatic feed takes over");
    weather_sync(now, 1);           /* back to normal readings */
}

static void test_forecast(void)
{
    const Location *dhaka = location_find(1);
    const Location *sydney = location_find(19);
    const Weather *obs;
    Weather hot;
    HourlyForecast h[HOURLY_COUNT], h2[HOURLY_COUNT], hp;
    DailyForecast d[DAILY_COUNT];
    time_t now = time(NULL);
    int i, ok_time = 1, ok_range = 1, ok_days = 1;

    section("Forecast engine");
    obs = weather_get(1);
    CHECK(forecast_hourly(dhaka, obs, now, h, HOURLY_COUNT) == 24, "hourly forecast has 24 entries");
    CHECK(h[0].time > now && h[0].time <= now + 3600, "first hour is the next full hour");
    for (i = 1; i < HOURLY_COUNT; i++) if (h[i].time - h[i - 1].time != 3600) ok_time = 0;
    CHECK(ok_time, "hourly entries are one hour apart");
    for (i = 0; i < HOURLY_COUNT; i++) {
        if (h[i].humidity < 0 || h[i].humidity > 100 || h[i].rain_chance < 0 ||
            h[i].rain_chance > 100 || h[i].pressure < 940 || h[i].pressure > 1060 ||
            h[i].wind_speed < 0 || h[i].rainfall_mm < 0)
            ok_range = 0;
    }
    CHECK(ok_range, "hourly values are within physical limits");

    forecast_hourly(dhaka, obs, now, h2, HOURLY_COUNT);
    for (i = 0; i < HOURLY_COUNT; i++) {   /* compare field by field (structs have padding) */
        if (h[i].time != h2[i].time || h[i].temperature != h2[i].temperature ||
            h[i].humidity != h2[i].humidity || h[i].rain_chance != h2[i].rain_chance ||
            h[i].wind_speed != h2[i].wind_speed || h[i].pressure != h2[i].pressure ||
            h[i].condition != h2[i].condition)
            ok_time = 0;
    }
    CHECK(ok_time, "forecast is deterministic (same input, same output)");

    CHECK(forecast_daily(dhaka, obs, now, d, DAILY_COUNT) == 7, "daily forecast has 7 days");
    for (i = 0; i < DAILY_COUNT; i++) {
        if (d[i].temp_min > d[i].temp_max) ok_days = 0;
        if (i > 0 && d[i].date - d[i - 1].date != 86400) ok_days = 0;
    }
    CHECK(ok_days, "every day has min <= max and days are 24 h apart");

    /* persistence: a much hotter observation makes the next hours hotter */
    hot = *obs;
    hot.temperature = obs->temperature + 10.0;
    forecast_point(dhaka, &hot, h[0].time, &hp);
    CHECK(hp.temperature > h[0].temperature + 5.0, "observation correction raises the forecast");
    forecast_point(dhaka, &hot, now + 48 * 3600, &hp);
    forecast_point(dhaka, obs, now + 48 * 3600, &h2[0]);
    CHECK(fabs(hp.temperature - h2[0].temperature) < 0.5, "observation correction fades after 2 days");

    CHECK(climate_daily_mean(dhaka, 135) > climate_daily_mean(dhaka, 15) + 5.0,
          "Dhaka is much warmer in May than in January");
    CHECK(climate_daily_mean(sydney, 15) > climate_daily_mean(sydney, 196),
          "seasons are reversed in the southern hemisphere");
    CHECK(climate_rain_factor(dhaka, 7) > climate_rain_factor(dhaka, 12),
          "monsoon (July) is wetter than winter (December)");
}

static void test_advisory(void)
{
    DailyForecast days[3];
    int order[3], i, n, found_danger = 0;
    Weather w;
    HourlyForecast h[HOURLY_COUNT];
    DailyForecast d[DAILY_COUNT];
    Advice adv[MAX_ADVICE];
    double f;

    section("Smart advisory (unique feature)");
    f = feels_like(35.0, 70, 5.0);
    CHECK(f > 49.0 && f < 53.0, "heat index of 35 C / 70% is about 50 C");
    f = feels_like(-10.0, 50, 20.0);
    CHECK(f > -19.0 && f < -17.0, "wind chill of -10 C / 20 km/h is about -18 C");
    CHECK(fabs(feels_like(20.0, 50, 10.0) - 20.0) < 0.001, "mild weather: feels like = temperature");

    CHECK(comfort_score(23.0, 50, 8.0, 0) >= 90, "pleasant weather scores 90+");
    CHECK(comfort_score(40.0, 80, 50.0, 90) <= 10, "extreme weather scores 10 or less");
    CHECK(strcmp(comfort_label(85), "Excellent") == 0 && strcmp(comfort_label(10), "Very Poor") == 0,
          "comfort labels");

    memset(days, 0, sizeof days);
    days[0].temp_min = 25; days[0].temp_max = 38; days[0].humidity = 80; days[0].rain_chance = 70;
    days[1].temp_min = 15; days[1].temp_max = 25; days[1].humidity = 55; days[1].rain_chance = 5;
    days[2].temp_min = 20; days[2].temp_max = 30; days[2].humidity = 65; days[2].rain_chance = 30;
    rank_days(days, 3, order);
    CHECK(order[0] == 1 && order[2] == 0, "best day planner ranks the pleasant day first");

    memset(&w, 0, sizeof w);
    w.temperature = 40.0; w.humidity = 70; w.wind_speed = 5.0; w.rain_chance = 5;
    w.condition = COND_CLEAR;
    memset(h, 0, sizeof h);
    for (i = 0; i < HOURLY_COUNT; i++) { h[i].temperature = 38; h[i].humidity = 60; }
    memset(d, 0, sizeof d);
    d[0].temp_max = 41.0;
    n = advisory_generate(&w, h, HOURLY_COUNT, d, DAILY_COUNT, adv, MAX_ADVICE);
    for (i = 0; i < n; i++)
        if (strcmp(adv[i].category, "Health") == 0 && adv[i].level == ADVICE_DANGER) found_danger = 1;
    CHECK(n >= 5, "advisory gives advice for several categories");
    CHECK(found_danger, "extreme heat gives a Health DANGER advice");
}

static int has_alert(const Alert *a, int n, const char *type, int severity)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(a[i].type, type) == 0 && a[i].severity == severity) return 1;
    return 0;
}

static void test_alerts(void)
{
    const Location *dhaka = location_find(1);
    Weather w;
    HourlyForecast h[HOURLY_COUNT];
    Alert out[16], a, list[16];
    time_t now = time(NULL);
    int i, n, id;

    section("Weather alerts");
    memset(&w, 0, sizeof w);
    w.location_id = 1; w.temperature = 28; w.humidity = 60; w.wind_speed = 10;
    w.rain_chance = 10; w.pressure = 1012; w.condition = COND_CLEAR;
    for (i = 0; i < HOURLY_COUNT; i++) {
        memset(&h[i], 0, sizeof h[i]);
        h[i].temperature = 28; h[i].humidity = 60; h[i].wind_speed = 10;
        h[i].rain_chance = 10; h[i].pressure = 1012; h[i].condition = COND_CLEAR;
    }
    CHECK(alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16) == 0, "normal weather gives no alert");

    h[5].temperature = 41.0;
    n = alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16);
    CHECK(has_alert(out, n, "HEAT", SEV_WARNING), "41 C gives a HEAT warning");
    h[5].temperature = 37.0;
    n = alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16);
    CHECK(has_alert(out, n, "HEAT", SEV_WATCH), "37 C gives a HEAT watch");
    h[5].temperature = 28.0;

    h[3].wind_speed = 65.0;
    n = alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16);
    CHECK(has_alert(out, n, "WIND", SEV_WARNING), "65 km/h wind gives a WIND warning");
    h[3].pressure = 990.0;
    n = alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16);
    CHECK(has_alert(out, n, "CYCLONE", SEV_WARNING), "990 hPa + strong wind gives a CYCLONE warning");
    h[3].wind_speed = 10.0; h[3].pressure = 1012.0;

    h[8].temperature = 8.0;
    n = alert_detect(dhaka, &w, h, HOURLY_COUNT, out, 16);
    CHECK(has_alert(out, n, "COLD", SEV_WATCH), "8 C in the tropics gives a COLD wave watch");

    /* manual alerts */
    alert_load();
    memset(&a, 0, sizeof a);
    a.location_id = 7; a.severity = SEV_WARNING; a.active = 1;
    strcpy(a.type, "CYCLONE");
    strcpy(a.message, "Signal 7|move to shelter");
    format_time(now, a.issued, sizeof a.issued);
    format_time(now + 3600, a.expires, sizeof a.expires);
    id = alert_add(&a);
    CHECK(id == 1, "first manual alert gets id 1");
    CHECK(strchr(alert_find(id)->message, '|') == NULL, "alert message is sanitized");
    CHECK(alert_is_current(alert_find(id), now), "new alert is current");
    CHECK(!alert_is_current(alert_find(id), now + 7200), "alert expires after its end time");

    a.location_id = ALERT_ALL_LOCATIONS;
    strcpy(a.type, "OTHER");
    alert_add(&a);
    n = alert_for_location(3, now, list, 16);
    CHECK(n == 1 && list[0].location_id == ALERT_ALL_LOCATIONS, "'all locations' alert reaches Bogura");
    n = alert_for_location(7, now, list, 16);
    CHECK(n == 2, "Barishal gets its own alert and the 'all' alert");

    alert_toggle(id);
    CHECK(!alert_is_current(alert_find(id), now), "deactivated alert is not current");
    alert_load();
    CHECK(alert_count() == 2 && alert_find(id)->active == 0, "alerts are saved to the file");
    CHECK(alert_delete(id) == 0 && alert_find(id) == NULL, "alert can be deleted");
}

static void test_users(void)
{
    section("Users & login");
    user_load();
    admin_ensure_default();
    CHECK(user_register("rahim_01", "rahim123", "Rahim Uddin", 3) == 0, "user registration succeeds");
    CHECK(user_register("RAHIM_01", "other123", "Someone", 1) == -1, "duplicate username (any case) is refused");
    CHECK(user_register("karim", "karim123", "Karim Mia", 1) == 0, "second user registration succeeds");
    user_load();
    CHECK(user_count() == 2, "users are saved and loaded");
    CHECK(strcmp(user_find("rahim_01")->password_hash, "rahim123") != 0, "password is not stored as plain text");

    CHECK(user_login("rahim_01", "rahim123") == AUTH_OK, "correct password logs in");
    CHECK(user_login("Rahim_01", "rahim123") == AUTH_OK, "username is case-insensitive");
    CHECK(user_login("rahim_01", "wrong123") == AUTH_WRONG_PASSWORD, "wrong password is refused");
    CHECK(user_login("nobody", "abc123") == AUTH_NO_USER, "unknown user is refused");
    CHECK(strcmp(user_find("rahim_01")->last_login, "-") != 0, "last login time is recorded");

    user_set_active("karim", 0);
    CHECK(user_login("karim", "karim123") == AUTH_BLOCKED, "blocked user cannot log in");
    user_set_active("karim", 1);
    CHECK(user_login("karim", "karim123") == AUTH_OK, "unblocked user can log in again");

    user_change_password("karim", "newpass9");
    CHECK(user_login("karim", "newpass9") == AUTH_OK && user_login("karim", "karim123") != AUTH_OK,
          "password change works");

    CHECK(admin_login("admin", "admin123"), "default admin login works");
    CHECK(!admin_login("admin", "wrong"), "wrong admin password is refused");
    admin_change_password("boss2026");
    admin_ensure_default();
    CHECK(admin_login("admin", "boss2026"), "new admin password is saved");

    CHECK(user_delete("karim") == 0 && user_find("karim") == NULL, "user can be deleted");
}

static void test_userdata(void)
{
    int ids[MAX_FAVORITES], i, n;
    HistoryEntry h[MAX_HISTORY_SHOW];
    int top[5];
    long counts[5];

    section("Favourites & search history");
    CHECK(favorites_add("rahim_01", 1) == 0, "add favourite Dhaka");
    CHECK(favorites_add("rahim_01", 1) == 1, "adding the same favourite again is detected");
    CHECK(favorites_add("rahim_01", 6) == 0, "add favourite Sylhet");
    n = favorites_list("rahim_01", ids, MAX_FAVORITES);
    CHECK(n == 2 && ids[0] == 1 && ids[1] == 6, "favourites are listed in order");
    CHECK(favorites_is("RAHIM_01", 6), "favourite check ignores username case");
    favorites_remove("rahim_01", 1);
    CHECK(!favorites_is("rahim_01", 1) && favorites_is("rahim_01", 6), "favourite can be removed");
    for (i = 2; i <= 12; i++) favorites_add("rahim_01", i);
    CHECK(favorites_list("rahim_01", ids, MAX_FAVORITES) == MAX_FAVORITES, "favourites are limited to 10");
    CHECK(favorites_add("rahim_01", 20) == -1, "11th favourite is refused");
    favorites_remove_location(6);
    CHECK(!favorites_is("rahim_01", 6), "deleting a location removes it from favourites");

    history_add("rahim_01", 3);
    history_add("rahim_01", 1);
    history_add("rahim_01", 1);
    history_add("someone", 5);
    n = history_list("rahim_01", h, MAX_HISTORY_SHOW);
    CHECK(n == 3 && h[0].location_id == 1 && h[2].location_id == 3, "history is listed newest first");
    CHECK(history_count_user("rahim_01") == 3 && history_total() == 4, "history counts");
    n = history_top_locations(top, counts, 5);
    CHECK(n == 3 && top[0] == 1 && counts[0] == 2, "most searched location is Dhaka (2 times)");
    CHECK(history_most_searched_by("rahim_01") == 1, "user's most searched location");
    history_clear("rahim_01");
    CHECK(history_count_user("rahim_01") == 0 && history_total() == 1, "clearing history keeps other users");
}

static void test_detect(void)
{
    DetectResult r;
    section("Automatic location detection");
    CHECK(detect_save_device_coords(24.8763, 89.3505) == 0, "device GPS position is saved");
    CHECK(detect_location(1, &r) == 3 && r.method == DETECT_GPS_FILE, "GPS file -> Bogura");
    CHECK(r.distance_km >= 0.0 && r.distance_km < 10.0, "distance to the detected city is reported");
    CHECK(detect_save_device_coords(100.0, 0.0) != 0, "invalid latitude is refused");

    {   /* without GPS: time zone or home location is used */
        char path[PATH_LEN];
        data_path(FILE_DEVICE_GPS, path, sizeof path);
        remove(path);
    }
    CHECK(detect_location(1, &r) > 0 &&
          (r.method == DETECT_TIMEZONE || r.method == DETECT_HOME), "fallback without GPS works");
}

static void test_reports_and_log(void)
{
    char path[PATH_LEN], lines[5][LINE_LEN];
    section("Reports & activity log");
    CHECK(report_weather_summary(path, sizeof path) == 0 && file_size(path) > 500,
          "weather summary report is created");
    CHECK(report_location_forecast(1, path, sizeof path) == 0 && file_size(path) > 500,
          "location forecast report is created");
    CHECK(report_location_forecast(999, path, sizeof path) != 0, "report for unknown location fails");
    CHECK(report_user_activity(path, sizeof path) == 0 && file_size(path) > 200,
          "user activity report is created");

    log_event("TEST", "first");
    log_event("TEST", "second");
    CHECK(log_count_type("TEST") == 2, "log events are counted by type");
    CHECK(log_tail(lines, 5) >= 2 && strstr(lines[log_tail(lines, 5) - 1], "second") != NULL,
          "log_tail returns the newest line last");
}

int main(void)
{
    printf("Weather Forecasting System - unit tests\n");
    printf("=======================================\n");

    set_data_dir(TEST_DATA_DIR);
    set_report_dir(TEST_REPORT_DIR);
    ensure_dir("tests");
    ensure_dir(TEST_DATA_DIR);
    ensure_dir(TEST_REPORT_DIR);
    clean_test_data();

    test_strings();
    test_time_and_hash();
    test_locations();
    test_validation();
    test_weather();
    test_forecast();
    test_advisory();
    test_alerts();
    test_users();
    test_userdata();
    test_detect();
    test_reports_and_log();

    printf("\n=======================================\n");
    printf("Result: %d passed, %d failed, %d total\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
