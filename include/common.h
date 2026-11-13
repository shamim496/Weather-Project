/*
 * common.h
 * ---------------------------------------------------------------
 * Weather Forecasting System - shared constants and limits.
 * Every module includes this header so that all array sizes and
 * file names are defined in exactly one place.
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <time.h>

#define APP_NAME     "Weather Forecasting System"
#define APP_VERSION  "1.0.0"

/* ---------- capacity limits ---------- */
#define MAX_USERS         500
#define MAX_LOCATIONS     200
#define MAX_ALERTS        300
#define MAX_FAVORITES     10    /* favourite locations per user  */
#define MAX_HISTORY_SHOW  20    /* history entries shown to user */
#define MAX_SEARCH_RESULT 20

/* ---------- string lengths (including '\0') ---------- */
#define NAME_LEN      50
#define USERNAME_LEN  21        /* 20 characters + '\0'           */
#define PASSWORD_LEN  33        /* 32 characters + '\0'           */
#define HASH_LEN      17        /* 16 hex digits + '\0'           */
#define TIME_LEN      20        /* "YYYY-MM-DD HH:MM:SS" + '\0'    */
#define TYPE_LEN      16
#define MSG_LEN       160
#define LINE_LEN      512
#define PATH_LEN      260

/* ---------- data files (stored inside the data directory) ---------- */
#define FILE_USERS       "users.txt"
#define FILE_ADMIN       "admin.txt"
#define FILE_LOCATIONS   "locations.txt"
#define FILE_WEATHER     "weather.txt"
#define FILE_ALERTS      "alerts.txt"
#define FILE_FAVORITES   "favorites.txt"
#define FILE_HISTORY     "history.txt"
#define FILE_DEVICE_GPS  "device_location.txt"
#define FILE_LOG         "system.log"

#define DEFAULT_DATA_DIR    "data"
#define DEFAULT_REPORT_DIR  "reports"

/* Field separator used in every data file */
#define FIELD_SEP '|'

#ifndef PI
#define PI 3.14159265358979323846
#endif

#endif /* COMMON_H */
