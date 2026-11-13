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

#include "location.h"
#include "weather.h"

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
