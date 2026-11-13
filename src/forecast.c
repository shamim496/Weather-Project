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

#include "forecast.h"
#include "utils.h"

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
