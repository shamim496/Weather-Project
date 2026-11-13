/*
 * location.h
 * ---------------------------------------------------------------
 * Location (city) records. Each location stores its coordinates,
 * time zone and a few climate values that the forecast engine
 * uses as a baseline.
 */
#ifndef LOCATION_H
#define LOCATION_H

#include "common.h"

typedef struct {
    int    id;
    char   name[NAME_LEN];
    char   region[NAME_LEN];     /* division / state, country       */
    double latitude;             /* + north, - south                */
    double longitude;            /* + east,  - west                 */
    double utc_offset;           /* hours, e.g. 6.0 for Bangladesh  */
    double avg_temp;             /* yearly mean temperature (C)     */
    double season_range;         /* hottest month - coldest month   */
    double daily_range;          /* typical day/night difference    */
    int    avg_humidity;         /* %                               */
    int    rain_tendency;        /* 0 (desert) .. 100 (very rainy)  */
} Location;

/* ---------- storage ---------- */
int       location_load(void);
int       location_save(void);
void      location_seed_defaults(void);

/* ---------- access ---------- */
int       location_count(void);
Location *location_at(int index);
Location *location_find(int id);
const char *location_name(int id);

/* ---------- search ---------- */
int       location_search(const char *query, int ids[], int max);
int       location_suggest(const char *query, int ids[], int max);

/* ---------- admin operations ---------- */
int       location_add(Location *loc);          /* returns new id or -1 */
int       location_update(const Location *loc);
int       location_delete(int id);
int       location_name_exists(const char *name, int except_id);

/* ---------- geography ---------- */
double    haversine_km(double lat1, double lon1, double lat2, double lon2);
int       location_nearest(double lat, double lon, double *distance_km);
void      location_local_tm(const Location *loc, time_t t, struct tm *out);
void      location_format_coords(const Location *loc, char *buf, size_t size);

#endif /* LOCATION_H */
