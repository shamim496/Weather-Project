# Weather Forecasting System: Project Documentation

**Course:** Software Development Lab I
**Department:** Computer Science and Engineering, Pundra University of Science & Technology
**Language / tools:** C (C99), GCC / MinGW, Code::Blocks / VS Code, Git, GitHub

| Member | Roll Number | Batch |
|---|---|---|
| Md. Tamim Akando | 03225105101063 | 20th |
| Md. Rizuan Isam  | 03225105101060 | 20th |

---

## 1. Project overview and problem statement

People need weather information every day for agriculture, transport, travel, education
and emergency planning. The information is often spread over different sources, and the
raw numbers (for example "humidity 85 %, wind 18 km/h") do not tell a farmer or a
traveller what to *do*.

The **Weather Forecasting System** is a console application written in C that brings all
of this into one place. Users register and log in, search a location and see the current
weather, an hourly forecast for 24 hours and a daily forecast for 7 days, including
temperature, humidity, wind speed and direction, rain probability and amount, atmospheric
pressure and the weather condition. They can keep favourite locations, see their search
history, receive weather alerts, let the system detect their location automatically and
choose a dark or light display.

An administrator module provides admin login, user management, weather data monitoring,
location management, alert management, system monitoring and report generation.
All data is stored in text files using C file handling.

**Unique feature:** the *Smart Weather Advisory & Best Day Planner* converts weather values
into practical advice for health, agriculture, travel and daily life, computes a
"feels-like" temperature and a comfort score, and recommends the best day of the coming
week for outdoor work or travel.

## 2. Requirements and scope

### 2.1 Functional requirements
| ID | Requirement | Module |
|---|---|---|
| FR-1 | Register a user with validated username, full name, password and home location | User |
| FR-2 | Log in with username and password (max. 3 attempts); blocked users are refused | User |
| FR-3 | Search locations by full or partial name, region or ID; suggest close matches | User |
| FR-4 | Show current weather: temperature, feels-like, humidity, wind speed and direction, rain chance and amount, pressure, condition | User |
| FR-5 | Show an hourly forecast (24 h) and a daily forecast (7 days) | User |
| FR-6 | Save up to 10 favourite locations; record and show search history | User |
| FR-7 | Show weather alerts (admin and automatically detected) and notify at login | User |
| FR-8 | Detect the user's location automatically | User |
| FR-9 | Dark / Light / Classic display theme, saved per user | User |
| FR-10 | Smart advisory, comfort score and best day planner (unique feature) | User |
| FR-11 | Admin login and password change | Admin |
| FR-12 | User management: list, view, block/unblock, reset password, delete | Admin |
| FR-13 | Weather data monitoring: view status, manual entry, refresh feed | Admin |
| FR-14 | Location management: add, edit, delete | Admin |
| FR-15 | Alert management: issue, enable/disable, delete, automatic scan and publish | Admin |
| FR-16 | System monitoring: statistics, top searched locations, file health, activity log | Admin |
| FR-17 | Generate weather summary, location forecast and user activity reports | Admin |

### 2.2 Non-functional requirements
* **Portability:** standard C99 only; compiles without warnings on GCC (Linux/macOS) and
  MinGW (Windows / Code::Blocks).
* **Usability:** numbered menus, clear messages, coloured themes, `0` = back everywhere,
  every input validated and asked again if wrong.
* **Reliability:** files are written to a temporary file and renamed (no half-written data);
  missing files are recreated with defaults; the program exits cleanly when input ends.
* **Security:** passwords stored only as salted hashes, password input masked with `*`,
  `|` and new lines removed from user text before saving.
* **Maintainability:** one module (`.c` + `.h`) per responsibility, 142 automated tests.

### 2.3 Scope
*In scope:* 21 built-in locations (12 in Bangladesh and 9 world cities) plus any number
added by the admin (up to 200), offline forecasting, file storage, single-computer use.
*Out of scope:* live data from internet weather services, real GPS hardware, graphical
interface, multi-user network access.

## 3. System design and program workflow

### 3.1 Module structure

```
                              +-----------+
                              |  main.c   |  start-up, main menu, login/register
                              +-----+-----+
                   +----------------+----------------+
                   v                                 v
           +---------------+                 +---------------+
           | user_menu.c   |                 | admin_menu.c  |
           | (User module) |                 | (Admin module)|
           +-------+-------+                 +-------+-------+
                   +---------------+-----------------+
                                   v
                          +-----------------+
                          |    views.c      |  weather screens shared by both
                          +--------+--------+
        +-----------+-----------+--+--------+-----------+-----------+
        v           v           v           v           v           v
   forecast.c  advisory.c   alert.c    weather.c   location.c   detect.c
        |           |           |           |           |           |
        +-----------+-----+-----+-----------+-----------+-----------+
                          v
        user.c   userdata.c   report.c   logger.c      (storage modules)
                          |
                          v
            ui.c (themes, input)     utils.c (strings, time, files, hash, noise)
```

### 3.2 Program workflow

```
 START
   |
   v
 load data files (create defaults if missing) -> refresh automatic weather feed
   |
   v
 MAIN MENU --1--> LOGIN --ok--> [alerts notification] --> USER DASHBOARD
   |  |  |          |                                        |  1 search + weather card
   |  |  |          +-- wrong x3 / blocked -> back           |  2 hourly   3 daily
   |  |  |                                                   |  4 advisory 5 compare
   |  |  +--2--> REGISTER (validate) --> optional login      |  6 favourites 7 history
   |  |                                                      |  8 alerts   9 detect
   |  +--3--> ADMIN LOGIN --ok--> ADMIN DASHBOARD            |  10 settings 0 logout
   |                               | 1 users  2 weather data
   |                               | 3 locations  4 alerts
   |                               | 5 monitoring 6 reports
   |                               | 7 password   0 logout
   +--4--> ABOUT
   +--0--> EXIT (log event, restore terminal)
```

Every change (new user, favourite, alert, weather entry ...) is written to its file
immediately, so nothing is lost if the program is closed.

### 3.3 Forecast workflow
```
 location + time t
      |
      v
 climate baseline: yearly mean + seasonal wave + day/night wave
      |
      +-- smooth deterministic noise (weather systems, storms)
      +-- monsoon factor for rain (by month)
      v
 model value  --(+ observation correction * e^(-hours/12))-->  forecast value
      |
      v
 condition derived from temperature, humidity, rain, wind, pressure
```

## 4. Data structures and file format

### 4.1 Main structures (see `include/*.h`)

```c
typedef struct {                 /* location.h */
    int    id;
    char   name[50], region[50];
    double latitude, longitude, utc_offset;
    double avg_temp, season_range, daily_range;  /* climate baseline */
    int    avg_humidity, rain_tendency;
} Location;

typedef struct {                 /* weather.h - current observation */
    int       location_id;
    double    temperature, wind_speed, rainfall_mm, pressure;
    int       humidity, wind_dir, rain_chance;
    Condition condition;         /* enum: CLEAR ... SNOW */
    char      updated[20];
    char      source;            /* 'S' automatic feed, 'M' manual */
} Weather;

typedef struct { time_t time; double temperature; int humidity; ... } HourlyForecast;
typedef struct { time_t date; double temp_min, temp_max; ...       } DailyForecast;

typedef struct {                 /* user.h */
    char username[21], password_hash[17], full_name[50];
    int  home_location, theme, active;
    char created[20], last_login[20];
} User;

typedef struct {                 /* alert.h */
    int  id, location_id, severity, active;   /* location 0 = all */
    char type[16], message[160], issued[20], expires[20];
} Alert;

typedef struct { char category[24]; AdviceLevel level; char text[160]; } Advice;
typedef struct { int location_id; DetectMethod method;
                 double latitude, longitude, distance_km; } DetectResult;
```

Records are kept in **static arrays** inside each module (e.g. `Location g_locations[200]`)
and loaded at start-up. Favourites and history are processed line by line from their
files, so they can grow without limit.

### 4.2 File format
All files are in `data/`, one record per line, fields separated by `|`, `#` = comment.

| File | Fields |
|---|---|
| `users.txt` | username, password_hash, full_name, home_location, theme, active, created, last_login |
| `admin.txt` | username, password_hash |
| `locations.txt` | id, name, region, latitude, longitude, utc_offset, avg_temp, season_range, daily_range, avg_humidity, rain_tendency |
| `weather.txt` | location_id, temperature, humidity, wind_speed, wind_dir, rain_chance, rainfall_mm, pressure, condition, updated, source |
| `alerts.txt` | id, location_id, severity, type, message, issued, expires, active |
| `favorites.txt` | username, location_id |
| `history.txt` | username, location_id, date-time |
| `device_location.txt` | latitude, longitude, source |
| `system.log` | date-time, event type, message |

Example (`locations.txt`):
```
3|Bogura|Rajshahi Division, Bangladesh|24.8465|89.3773|6.0|25.4|12.0|10.0|73|50
```

## 5. Implementation of major features

**Registration and login (`user.c`).** Usernames must start with a letter and have 3-20
letters, digits or `_`; passwords need 6+ characters with letters and digits. The
password is hashed with salted FNV-1a (username as salt, 1000 rounds) and only the hash
is stored. Login allows three attempts; blocked accounts are refused. All logins and
failures are written to `system.log`.

**Location search (`location.c`, `views.c`).** Case-insensitive substring search over name
and region; exact matches come first. If nothing matches, the *Levenshtein edit
distance* is used to suggest close names ("Dhakka" -> "Dhaka").

**Automatic weather feed (`weather.c`).** On start-up and before each view, readings older
than 60 minutes are regenerated by the forecast model. Manual readings from the admin are
kept for 6 hours.

**Forecast engine (`forecast.c`).** A deterministic model (same input, same output):
seasonal wave with different warming and cooling lengths (tropics hottest in May, coldest
in January), day/night sine wave, smooth value noise for passing weather systems, a
monsoon factor for rain, rare storm periods (low pressure, strong wind), humidity
linked to temperature and rain, and rain amount that depends on probability and air
temperature. Admin observations are blended in and fade with `e^(-hours/12)`. Hourly
forecasts are 24 samples; daily forecasts aggregate 24 samples per day (min, max,
average humidity and pressure, strongest wind, highest rain chance, total rain).

**Alerts (`alert.c`).** Admin alerts have a location (or all), severity
(ADVISORY / WATCH / WARNING), type, message and expiry time. Automatic detection checks
the current weather plus the next 24 hours against thresholds: heat wave >= 36 C
(warning >= 40 C), cold wave <= 10 C in the tropics, freezing <= 0 C, cyclone
(pressure <= 995 hPa and wind >= 50 km/h), strong wind >= 40 / 60 km/h, heavy rain
(>= 75 % chance or >= 50 mm/24 h), snow, thunderstorm and fog.

**Automatic location detection (`detect.c`).** Order: environment variables `WFS_LAT` /
`WFS_LON` -> `device_location.txt` (simulated GPS) -> computer time zone -> home location.
For coordinates, the nearest city is found with the **Haversine formula**:
`d = 2R * atan2(sqrt(a), sqrt(1-a))`, where
`a = sin^2(dlat/2) + cos(lat1) cos(lat2) sin^2(dlon/2)` and R = 6371 km.

**Unique feature: Smart Advisory & Best Day Planner (`advisory.c`).**
* *Feels-like*: NWS Heat Index (Rothfusz regression) when T >= 26.7 C and RH >= 40 %;
  Wind Chill when T <= 10 C and wind > 4.8 km/h.
* *Comfort score*: starts at 100, minus 4 points per degree of feels-like above 26 C,
  3 per degree below 18 C, 0.6 per % humidity above 70, 1.2 per km/h wind above 25 and
  0.45 per % rain chance.
* *Advice rules*: e.g. spraying is "good" when the wind stays below 15 km/h and the rain
  chance below 30 % for 6 hours; more than 20 mm of rain in 3 days -> "clear field
  drainage"; storm or wind >= 40 km/h -> "avoid launch/ferry journeys"; rain chance
  >= 40 % in 12 hours -> "carry an umbrella".
* *Best day planner*: every day gets a score from its daytime temperature, humidity, wind
  and rain chance; days are sorted with insertion sort; the best and worst are highlighted.

**Themes (`ui.c`).** ANSI colour codes; each theme defines a background and a colour for
every role (title, accent, success, warning, error ...). On Windows the virtual terminal
mode is switched on; if colours are not possible the Classic theme is used.

**Reports (`report.c`).** Text files named with a time stamp in `reports/`: weather summary
(table, statistics, condition-wise count, active alerts), location forecast (current, 24 h,
7 days, advisory, best day) and user activity (users, searches, top locations).

## 6. Testing and results

* **142 automated unit tests** (`tests/test_main.c`, run with `make test`): all passed.
* **38 manual test cases** (`tests/TEST_CASES.md`): all passed.
* Compiled with `-std=c99 -Wall -Wextra -pedantic` on GCC 13 (Linux) and MinGW-w64
  (Windows) with **0 warnings**.
* **Valgrind**: no memory errors and no leaks.

Sample checks of the formulas:

| Check | Expected | Program |
|---|---|---|
| Heat index 35 C, 70 % | about 50-51 C | 50.3 C |
| Wind chill -10 C, 20 km/h | about -18 C | -17.9 C |
| Distance Dhaka - Chattogram | about 210-215 km | 214 km |
| Nearest city to 24.8510 N, 89.3697 E | Bogura | Bogura (0.9 km) |

## 7. Limitations

1. Weather values come from a simplified climate model, not from real measurements or an
   internet weather service, so they are realistic but not real.
2. Location detection relies on a GPS data file, environment variables or the time zone;
   it does not read real GPS hardware or IP-based location.
3. Console interface only; long tables need a terminal at least 78 columns wide.
4. Data files are not locked, so two copies of the program should not write the same data
   folder at the same time.
5. The password hash is suitable for a lab project but not for a production system
   (no bcrypt/argon2).

## 8. Conclusion

The project meets all objectives of the proposal: it provides current weather information,
searches weather by city or location, displays temperature and conditions with all
important parameters, and keeps a simple, user-friendly interface. On top of the proposed
features (registration/login, current weather, hourly and daily forecast, detailed
parameters, favourites and history, alerts, automatic detection, dark/light theme and a
complete admin module) it adds a Smart Advisory & Best Day Planner that turns weather
data into decisions for farmers, travellers and everyone else. The code is split into
small modules, uses only standard C and file handling, and is covered by automated tests.

## 9. Possible future improvements

* Fetch live data from an online weather API (e.g. with libcurl) and compare it with the
  model's forecast.
* Read real GPS / IP-based location.
* Graphical user interface (GTK or a web front end) and charts.
* SMS / e-mail delivery of severe weather alerts.
* Store data in SQLite and support several users at the same time.
* Learn model parameters from historical data to improve accuracy.
* Bangla language option.
