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

#include "advisory.h"
#include "utils.h"

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
