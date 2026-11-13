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

#include "forecast.h"

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
