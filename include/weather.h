/*
 * weather.h
 * ---------------------------------------------------------------
 * Current weather observation for each location.
 * Records come from the (simulated) automatic weather feed or are
 * entered manually by the administrator.
 */
#ifndef WEATHER_H
#define WEATHER_H

#include "common.h"

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
