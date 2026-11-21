# Weather Forecasting System (C)

A console-based **weather information and forecasting application** written in **C (C99)**.
Users can register, log in, search any location, see the current weather, hourly and 7-day
forecasts, save favourite locations, check search history, receive weather alerts and let
the program detect their location automatically. An **administrator module** manages users,
locations, weather data and alerts, monitors the system and generates reports.
All data is stored in plain text files using C file handling.

> Software Development Lab I project, Department of CSE,
> Pundra University of Science & Technology.
>
> | Member | Roll | Batch |
> |---|---|---|
> | Md. Tamim Akando | 03225105101063 | 20th |
> | Md. Rizuan Isam  | 03225105101060 | 20th |

---

## Features

### User module
| # | Feature | Where in the program |
|---|---|---|
| 1 | Registration and login (validated input, salted password hash, 3 login attempts) | Main menu 1, 2 |
| 2 | Location search: full or partial name, region, ID, `?` lists all, **"Did you mean?"** suggestions | Dashboard 1 |
| 3 | Current weather: temperature, feels-like, humidity, wind speed and direction, rain chance and amount, pressure, condition with an ASCII weather icon | Dashboard 1 |
| 4 | Hourly forecast for the next 24 hours, with a temperature chart | Dashboard 2 |
| 5 | Daily forecast for the next 7 days, with a min/max range chart and a weekly summary | Dashboard 3 |
| 6 | Favourite locations (up to 10) with a quick weather overview | Dashboard 6 |
| 7 | Search history (newest first, most searched place, clear history) | Dashboard 7 |
| 8 | Weather alerts: admin alerts + **alerts the system detects itself** (heat wave, cold wave, storm, cyclone, heavy rain, snow, thunderstorm, fog) and a notification after login | Dashboard 8 |
| 9 | Automatic location detection: GPS reading, then time zone, then home location. The nearest city is found with the **Haversine formula** | Dashboard 9 |
| 10 | Display themes: **Dark / Light / Classic** (saved per user) | Dashboard 10 |
| 11 | Compare two locations side by side | Dashboard 5 |

### Unique feature: Smart Weather Advisory and Best Day Planner
Turns raw numbers into advice people can act on (Dashboard 4):
* **Feels-like temperature** using the NWS *Heat Index* and *Wind Chill* formulas
* **Comfort score (0-100)** from temperature, humidity, wind and rain
* Practical advice for **Health, Agriculture (farmers), Travel & Transport and Daily life**,
  e.g. *"Good time to spray pesticide (low wind, no rain in 6 hours)"*,
  *"Storm expected - avoid launch/ferry journeys"*, *"Carry an umbrella"*
* **Best Day Planner** ranks the next 7 days and recommends the best day for outdoor work,
  travel or events, and the day to avoid

### Admin module
| Feature | Description |
|---|---|
| Admin login | Separate account (default `admin` / `admin123`, can be changed) |
| User management | List users, view details, block/unblock, reset password, delete |
| Weather data monitoring | Table of all stations (AUTO / MANUAL / STALE), enter or correct data by hand, refresh the automatic feed |
| Location management | Add, edit and delete locations. Climate defaults are copied from the nearest known city |
| Alert management | Issue alerts (one or all locations, severity, expiry), switch them on/off, delete, scan all locations for dangerous weather and publish the results |
| System monitoring | Statistics, most searched locations (ranking), data file health and the recent activity log |
| Report generation | Weather summary (with statistics and a condition-wise summary), detailed forecast of one location, user activity report. Saved as `.txt` in `reports/` |

---

## Project structure

```
Weather-Project/
|-- include/              header files (one per module)
|-- src/
|   |-- main.c            start-up and main menu
|   |-- user_menu.c       user module (dashboard, favourites, history, settings ...)
|   |-- admin_menu.c      admin module
|   |-- views.c           weather screens shared by both modules
|   |-- forecast.c        forecast engine (hourly / daily)
|   |-- advisory.c        UNIQUE: smart advisory and best day planner
|   |-- alert.c           manual + automatic alerts
|   |-- weather.c         current weather records + automatic feed
|   |-- location.c        locations, search, Haversine distance
|   |-- detect.c          automatic location detection
|   |-- user.c            accounts, login, admin credentials
|   |-- userdata.c        favourites and search history
|   |-- report.c          text report generation
|   |-- logger.c          activity log
|   |-- ui.c              colours, themes, layout, safe input
|   `-- utils.c           strings, time, files, hashing, noise
|-- data/                 sample data files (see "Data files")
|-- reports/              generated reports
|-- tests/
|   |-- test_main.c       142 automated unit tests
|   `-- TEST_CASES.md     test cases and results
|-- docs/DOCUMENTATION.md full project documentation
|-- Makefile              build on Linux / macOS / MSYS2
|-- build.bat             build on Windows (cmd)
`-- WeatherForecastingSystem.cbp   Code::Blocks project
```

---

## Build

You only need a C compiler (GCC / MinGW). No extra libraries.

**Linux / macOS**
```bash
make            # creates bin/weather
make test       # builds and runs the unit tests
```

**Windows (Command Prompt, MinGW GCC in PATH)**
```bat
build.bat           :: creates bin\weather.exe
build.bat run       :: build and run
build.bat test      :: unit tests
```

**Code::Blocks**: open `WeatherForecastingSystem.cbp` and press **F9** (Build and run).

**VS Code**: open the folder and run `make` (Linux/macOS) or `build.bat` (Windows) in the terminal.

**Manual command** (any OS):
```bash
gcc -std=c99 -Wall -Wextra -Iinclude src/*.c -o weather -lm
```

### Single-file version
`single/weather_single.c` contains the whole program (all headers and sources merged) in
**one C file**, so it can be pasted into an online compiler or opened alone in Code::Blocks:

```bash
gcc -std=c99 -O2 single/weather_single.c -o weather -lm
./weather
```

The `data/` and `reports/` folders are created automatically next to where it runs.

## Run

Run the program **from the project folder** so it finds `data/`:

```bash
./bin/weather                 # Linux / macOS
bin\weather.exe               # Windows
```

Options:
```
--data DIR      use another data folder      (default: data)
--reports DIR   use another reports folder   (default: reports)
--help          show help
--version       show version
```
Environment variables: `NO_COLOR=1` turns colours off; `WFS_LAT` / `WFS_LON` give the device
GPS position for automatic location detection.

If the `data/` folder is missing, the program creates it with 21 default locations and the
default admin account.

---

## Usage guide

### Ready-made accounts (sample data)
| Role | Username | Password | Notes |
|---|---|---|---|
| Admin | `admin` | `admin123` | change it in Admin > Change Admin Password |
| User | `demo` | `demo123` | home Dhaka, has favourites and history |
| User | `karim_farmer` | `karim123` | home Bogura, Light theme |
| User | `nusrat` | `nusrat123` | home Cox's Bazar |
| User | `spam_user` | `spam1234` | **blocked**: shows the blocked-login message |

### A short tour
1. **Main menu > 2** to register (username 3-20 letters/digits/_, password 6+ characters with
   letters and digits, choose a home location).
2. Log in. A notification shows any alerts for your saved places.
3. **1 Search Location**: type `dha`, `bog`, `Cox`, an ID like `3`, or `?` for the list.
   A typo such as `Dhakka` gives *"Did you mean: Dhaka?"*. After the weather card you can open
   the hourly forecast, daily forecast or advisory, or add the place to favourites.
4. **4 Smart Advisory**: feels-like, comfort score, advice per category and the best day of
   the week.
5. **9 Detect My Location** reads `data/device_location.txt` (a simulated GPS reading near
   Bogura). Change it in **Settings > 5** to "move" the device.
6. **10 Settings** changes the theme (Dark / Light / Classic), home location, name or password.
7. Log in as **admin** (main menu 3) to manage everything. Try *Weather Data Monitoring > 2*
   to enter 41 C for Dhaka, then *Alert Management > 4* to see the heat-wave warning the
   system detects by itself.

At every prompt, invalid input is rejected with a message and the question is asked again.
`0` always means *back / cancel*.

---

## Data files

Every file is plain text with one record per line and fields separated by `|`.
Lines starting with `#` are comments.

| File | Format |
|---|---|
| `users.txt` | `username\|password_hash\|full_name\|home_location\|theme\|active\|created\|last_login` |
| `admin.txt` | `username\|password_hash` |
| `locations.txt` | `id\|name\|region\|lat\|lon\|utc_offset\|avg_temp\|season_range\|daily_range\|avg_humidity\|rain_tendency` |
| `weather.txt` | `location_id\|temp\|humidity\|wind_speed\|wind_dir\|rain_chance\|rainfall_mm\|pressure\|condition\|updated\|source` |
| `alerts.txt` | `id\|location_id(0=all)\|severity\|type\|message\|issued\|expires\|active` |
| `favorites.txt` | `username\|location_id` |
| `history.txt` | `username\|location_id\|date time` |
| `device_location.txt` | `latitude\|longitude\|source` (simulated GPS) |
| `system.log` | `date time \| EVENT \| message` (created at run time) |

Files are saved through a temporary file and then renamed, so a crash cannot leave a
half-written data file.

## How the forecast works (short)

The program runs offline, so it uses its own **simplified, deterministic climate model**
(see `src/forecast.c`):

1. **Season**: yearly average plus a seasonal wave (tropics hottest in May, others in July,
   southern hemisphere reversed).
2. **Day and night**: warmest around 15:00 and coldest around 03:00.
3. **Weather systems**: smooth "noise" that always gives the same value for the same place and
   time moves temperature, rain, wind and pressure up and down like passing weather fronts.
4. **Monsoon**: rain chance is scaled by the month for South Asia (wet June to September,
   dry November to February).
5. **Storms**: occasional low pressure with strong wind and heavy rain, which triggers alerts.
6. **Persistence**: if the admin enters a real observation, the forecast starts from it and
   the correction fades out over about 12 hours.

The "automatic weather feed" uses the same model to produce a new reading every hour.
Manual readings entered by the admin are kept for 6 hours.

## Testing

```bash
make test
```
Runs **142 automated tests** covering strings, validation, hashing, locations, Haversine,
weather records, the forecast engine, the advisory formulas, alerts, users and login,
favourites, history, location detection, reports and the log. Manual test cases and results
are in [`tests/TEST_CASES.md`](tests/TEST_CASES.md).

## Limitations
* Weather values are simulated by a climate model; there is no live internet weather source.
* Location detection uses a GPS file / environment variables / the time zone, not real GPS hardware.
* Password hashing (salted FNV-1a) is fine for a lab project but is not a production-grade algorithm.

See [`docs/DOCUMENTATION.md`](docs/DOCUMENTATION.md) for the full documentation (design,
workflow, data structures, testing, limitations and future work).
