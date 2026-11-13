# Test Cases and Results

Testing was done in two ways:

1. **Automated unit tests** (`tests/test_main.c`, 142 checks): run with `make test`
   (or `build.bat test` on Windows). They use their own temporary folders, so the real
   `data/` folder is never changed.
2. **Manual test cases**: the program was run with the sample data in `data/` and each case
   below was checked by hand. The program was also run under *Valgrind* (no memory errors
   or leaks) and compiled with `-Wall -Wextra -pedantic` on GCC (Linux) and MinGW (Windows)
   with **zero warnings**.

## 1. Manual test cases

| ID | Feature | Steps / input | Expected result | Result |
|---|---|---|---|---|
| TC-01 | Menu validation | Main menu: type `abc`, then `9` | "Please enter a whole number between 0 and 4", asks again | Pass |
| TC-02 | Registration: duplicate | Register with username `demo` | "Username 'demo' is already taken." | Pass |
| TC-03 | Registration: rules | Username `ab`; full name `X`; passwords `abc123` / `abc124` | Each one is rejected with a clear message | Pass |
| TC-04 | Registration: success | `new_user`, `New User`, `abc123`, home `Dhakka` -> suggestion 1 | "Did you mean: Dhaka", account created, line added to `users.txt` with a hash (no plain password) | Pass |
| TC-05 | Login: wrong password | `new_user` / `wrongpw1` | "Wrong username or password (1 of 3 attempts used)", `LOGIN_FAIL` in the log | Pass |
| TC-06 | Login: 3 failures | Wrong password 3 times | Back to the main menu after 3 attempts | Pass |
| TC-07 | Login: blocked user | `spam_user` / `spam1234` | "This account is blocked. Please contact the administrator." | Pass |
| TC-08 | Login notification | Log in as `demo` | Welcome message and a list of alerts for home/favourite places | Pass |
| TC-09 | Location search | Search `dha`, `bangladesh`, `3`, `?` | Dhaka; list of 12 Bangladeshi cities to choose from; Bogura; full list | Pass |
| TC-10 | Search suggestion | Search `Dhakka` | "No location named "Dhakka". Did you mean: 1. Dhaka" | Pass |
| TC-11 | Current weather | Open Rajshahi | Icon, condition, temperature, feels-like, humidity, wind speed and direction, rain chance and amount, pressure, today's min/max, comfort score, update time | Pass |
| TC-12 | Hourly forecast | Dashboard 2, Dhaka | 24 rows starting at the next full hour, a date line at midnight, temperature bars, 24-hour range | Pass |
| TC-13 | Daily forecast | Dashboard 3, Dhaka | 7 days (Today, Tomorrow, ...), min <= max, range chart, week summary | Pass |
| TC-14 | Smart advisory (unique) | Dashboard 4, Sylhet | Feels-like, comfort bar, Health / Agriculture / Travel / Daily-life advice, 7-day scores with BEST and avoid marks | Pass |
| TC-15 | Compare | Dashboard 5, Bogura vs Dhaka | Side-by-side table, distance (156 km), warmer and more comfortable city | Pass |
| TC-16 | Favourites | Add Dhaka, Moscow, Khulna; remove Khulna | List shows weather of each; "3 of 10 slots used", then 2 | Pass |
| TC-17 | Favourite limit | Add an 11th favourite | "You can save at most 10 favourites" | Pass |
| TC-18 | Search history | Dashboard 7 after several searches | Newest first, total count, most searched place; clear asks y/n and empties the list | Pass |
| TC-19 | Weather alerts | Dashboard 8 with Moscow as favourite | Automatic alerts (e.g. COLD advisory, RAIN/SNOW) sorted by severity | Pass |
| TC-20 | Auto detection (GPS) | Dashboard 9 with sample `device_location.txt` | "Device GPS sensor", 24.8510, 89.3697, nearest city Bogura (0.9 km) | Pass |
| TC-21 | Auto detection (fallback) | Delete `device_location.txt`, Dashboard 9 | City chosen by the computer's time zone (UTC+6 -> Dhaka, UTC+0 -> London) | Pass |
| TC-22 | Theme | Settings > 1 > Light, log out and in again | Colours change; theme saved in `users.txt` and restored at the next login | Pass |
| TC-23 | Change password | Settings > 4: old `abc123`, new `xyz789` twice | "Password changed successfully"; new password works at next login | Pass |
| TC-24 | Admin login | `admin` / `admin123` | Admin dashboard with users / locations / alerts / searches counts | Pass |
| TC-25 | Block user | Admin > Users > 2 > `rahim_01` | Status BLOCKED in table and file; that user can no longer log in | Pass |
| TC-26 | Manual weather data | Admin > Weather > 2: Dhaka 41.5 C, 55 %, 20 km/h, 180 deg, 10 %, 1004 hPa | Row shows MANUAL; users see 41.5 C; a HEAT WARNING is detected automatically | Pass |
| TC-27 | Refresh feed | Admin > Weather > 3 > y | All locations refreshed, status AUTO | Pass |
| TC-28 | Add location | Admin > Locations > 1: Gaibandha 25.3288, 89.5286 | Climate defaults copied from the nearest city, ID 22 given, weather created at once | Pass |
| TC-29 | Delete location | Admin > Locations > 3 > Cox's Bazar > y | Location, its weather, alerts and favourites removed; users with that home moved to Dhaka | Pass |
| TC-30 | Issue alert | Admin > Alerts > 1: Barishal, CYCLONE, WARNING, 48 h | Alert #1 ACTIVE; shown to users who saved Barishal | Pass |
| TC-31 | Alert on/off and expiry | Toggle alert #2; sample alert #3 | #2 shows OFF and is hidden from users; #3 shows EXPIRED | Pass |
| TC-32 | Alert scan | Admin > Alerts > 4 | List of detected alerts for all locations; option to publish them for 24 h | Pass |
| TC-33 | System monitoring | Admin > 5 | Statistics, top-5 searched cities with bars, data file sizes, last 12 log lines | Pass |
| TC-34 | Reports | Admin > 6 > 1, 2 (Sylhet), 3 | Three `.txt` files created in `reports/` and shown on screen | Pass |
| TC-35 | Delete user | Admin > Users > 4 > `nusrat` > y | User, favourites and history removed from all files | Pass |
| TC-36 | Change admin password | Admin > 7 | Wrong current password refused; new password works after restart | Pass |
| TC-37 | Missing data folder | Start with `--data` pointing to an empty folder | Folder created with 21 locations, default admin and fresh weather | Pass |
| TC-38 | End of input | Pipe a short input file into the program | Program exits cleanly ("Input closed. Goodbye!") instead of looping | Pass |

## 2. Automated unit test results

```
Weather Forecasting System - unit tests
=======================================

== String helpers ==
  [PASS] str_trim removes spaces at both ends
  [PASS] str_iequals ignores case
  [PASS] str_iequals detects different strings
  [PASS] str_icontains finds substring, any case
  [PASS] str_is_number
  [PASS] split_fields keeps empty fields and strips new line
  [PASS] sanitize_field removes '|' and new line
  [PASS] edit_distance dhaka/daka = 1
  [PASS] edit_distance is case-insensitive
  [PASS] edit_distance of equal strings = 0

== Time, hashing and noise ==
  [PASS] format_time / parse_time round trip
  [PASS] parse_time rejects month 13
  [PASS] parse_time rejects text
  [PASS] hash is 16 hex characters
  [PASS] hash uses case-insensitive username
  [PASS] same password, different user -> different hash (salt)
  [PASS] hash does not contain the password
  [PASS] noise values stay in range
  [PASS] noise is deterministic

== Locations ==
  [PASS] default locations are created on first run (21)
  [PASS] locations are saved and loaded again
  [PASS] search 'dha' finds Dhaka
  [PASS] exact name match is listed first
  [PASS] search by region finds all Bangladeshi cities
  [PASS] unknown name gives no result
  [PASS] suggestion for 'Dhakka' is Dhaka
  [PASS] Haversine distance Dhaka-Chattogram is about 214 km
  [PASS] nearest location to Pundra University is Bogura
  [PASS] new location gets the next id (22)
  [PASS] duplicate name check is case-insensitive
  [PASS] added location is stored in the file
  [PASS] location can be deleted

== Input validation ==
  [PASS] username too short is rejected
  [PASS] username starting with digit is rejected
  [PASS] username with space is rejected
  [PASS] reserved username 'admin' is rejected
  [PASS] valid username is accepted
  [PASS] password shorter than 6 is rejected
  [PASS] password without digit is rejected
  [PASS] password without letter is rejected
  [PASS] password with space is rejected
  [PASS] valid password is accepted
  [PASS] full name with dot is accepted
  [PASS] one-letter name is rejected
  [PASS] name with digits is rejected

== Weather data ==
  [PASS] 0 degrees is N
  [PASS] 45 degrees is NE
  [PASS] 180 degrees is S
  [PASS] 225 degrees is SW
  [PASS] 359 degrees is N
  [PASS] low rain chance -> Clear
  [PASS] 20% rain -> Partly cloudy
  [PASS] 60% rain -> Light rain
  [PASS] 80% rain -> Heavy rain
  [PASS] rain + strong wind -> Thunderstorm
  [PASS] rain below freezing -> Snow
  [PASS] very humid, calm -> Fog
  [PASS] automatic feed creates a reading for every location
  [PASS] fresh readings are not refreshed again
  [PASS] simulated reading has valid ranges
  [PASS] manual weather is saved
  [PASS] manual weather is loaded back from the file
  [PASS] manual weather is kept for 3 hours
  [PASS] after 6 hours the automatic feed takes over

== Forecast engine ==
  [PASS] hourly forecast has 24 entries
  [PASS] first hour is the next full hour
  [PASS] hourly entries are one hour apart
  [PASS] hourly values are within physical limits
  [PASS] forecast is deterministic (same input, same output)
  [PASS] daily forecast has 7 days
  [PASS] every day has min <= max and days are 24 h apart
  [PASS] observation correction raises the forecast
  [PASS] observation correction fades after 2 days
  [PASS] Dhaka is much warmer in May than in January
  [PASS] seasons are reversed in the southern hemisphere
  [PASS] monsoon (July) is wetter than winter (December)

== Smart advisory (unique feature) ==
  [PASS] heat index of 35 C / 70% is about 50 C
  [PASS] wind chill of -10 C / 20 km/h is about -18 C
  [PASS] mild weather: feels like = temperature
  [PASS] pleasant weather scores 90+
  [PASS] extreme weather scores 10 or less
  [PASS] comfort labels
  [PASS] best day planner ranks the pleasant day first
  [PASS] advisory gives advice for several categories
  [PASS] extreme heat gives a Health DANGER advice

== Weather alerts ==
  [PASS] normal weather gives no alert
  [PASS] 41 C gives a HEAT warning
  [PASS] 37 C gives a HEAT watch
  [PASS] 65 km/h wind gives a WIND warning
  [PASS] 990 hPa + strong wind gives a CYCLONE warning
  [PASS] 8 C in the tropics gives a COLD wave watch
  [PASS] first manual alert gets id 1
  [PASS] alert message is sanitized
  [PASS] new alert is current
  [PASS] alert expires after its end time
  [PASS] 'all locations' alert reaches Bogura
  [PASS] Barishal gets its own alert and the 'all' alert
  [PASS] deactivated alert is not current
  [PASS] alerts are saved to the file
  [PASS] alert can be deleted

== Users & login ==
  [PASS] user registration succeeds
  [PASS] duplicate username (any case) is refused
  [PASS] second user registration succeeds
  [PASS] users are saved and loaded
  [PASS] password is not stored as plain text
  [PASS] correct password logs in
  [PASS] username is case-insensitive
  [PASS] wrong password is refused
  [PASS] unknown user is refused
  [PASS] last login time is recorded
  [PASS] blocked user cannot log in
  [PASS] unblocked user can log in again
  [PASS] password change works
  [PASS] default admin login works
  [PASS] wrong admin password is refused
  [PASS] new admin password is saved
  [PASS] user can be deleted

== Favourites & search history ==
  [PASS] add favourite Dhaka
  [PASS] adding the same favourite again is detected
  [PASS] add favourite Sylhet
  [PASS] favourites are listed in order
  [PASS] favourite check ignores username case
  [PASS] favourite can be removed
  [PASS] favourites are limited to 10
  [PASS] 11th favourite is refused
  [PASS] deleting a location removes it from favourites
  [PASS] history is listed newest first
  [PASS] history counts
  [PASS] most searched location is Dhaka (2 times)
  [PASS] user's most searched location
  [PASS] clearing history keeps other users

== Automatic location detection ==
  [PASS] device GPS position is saved
  [PASS] GPS file -> Bogura
  [PASS] distance to the detected city is reported
  [PASS] invalid latitude is refused
  [PASS] fallback without GPS works

== Reports & activity log ==
  [PASS] weather summary report is created
  [PASS] location forecast report is created
  [PASS] report for unknown location fails
  [PASS] user activity report is created
  [PASS] log events are counted by type
  [PASS] log_tail returns the newest line last

=======================================
Result: 142 passed, 0 failed, 142 total
```
