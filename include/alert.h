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

#include "forecast.h"

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
