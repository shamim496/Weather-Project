/*
 * userdata.h
 * ---------------------------------------------------------------
 * Per-user data: favourite locations and search history.
 *
 * data/favorites.txt : username|location_id
 * data/history.txt   : username|location_id|YYYY-MM-DD HH:MM:SS
 */
#ifndef USERDATA_H
#define USERDATA_H

#include "common.h"

typedef struct {
    char username[USERNAME_LEN];
    int  location_id;
    char time[TIME_LEN];
} HistoryEntry;

/* ---------- favourites ---------- */
int  favorites_list(const char *username, int ids[], int max);
int  favorites_is(const char *username, int location_id);
int  favorites_add(const char *username, int location_id);   /* 0 ok, 1 exists, -1 full, -2 error */
int  favorites_remove(const char *username, int location_id);
int  favorites_remove_user(const char *username);
int  favorites_remove_location(int location_id);
long favorites_total(void);

/* ---------- search history ---------- */
int  history_add(const char *username, int location_id);
int  history_list(const char *username, HistoryEntry out[], int max); /* newest first */
int  history_clear(const char *username);
int  history_remove_location(int location_id);
long history_total(void);
long history_count_user(const char *username);
int  history_top_locations(int ids[], long counts[], int max);
int  history_most_searched_by(const char *username);

#endif /* USERDATA_H */
