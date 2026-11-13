/*
 * utils.h
 * ---------------------------------------------------------------
 * General purpose helpers: string handling, time formatting,
 * file paths, password hashing and a deterministic random
 * number generator used by the forecast engine.
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "common.h"

/* ---------- strings ---------- */
void   str_trim(char *s);
void   str_copy(char *dst, const char *src, size_t size);
void   str_to_lower(char *dst, const char *src, size_t size);
int    str_iequals(const char *a, const char *b);
int    str_icontains(const char *haystack, const char *needle);
int    str_is_number(const char *s);
void   sanitize_field(char *s);           /* removes '|' and new lines */
int    split_fields(char *line, char sep, char **fields, int max_fields);
int    edit_distance(const char *a, const char *b);   /* case-insensitive */

/* ---------- time ---------- */
void   format_time(time_t t, char *buf, size_t size);      /* YYYY-MM-DD HH:MM:SS */
void   format_time_short(time_t t, char *buf, size_t size);/* YYYY-MM-DD HH:MM    */
time_t parse_time(const char *s);                          /* returns -1 on error */
void   now_string(char *buf, size_t size);
long   system_utc_offset_seconds(void);

/* ---------- paths & files ---------- */
void        set_data_dir(const char *dir);
const char *get_data_dir(void);
void        set_report_dir(const char *dir);
const char *get_report_dir(void);
void        data_path(const char *file, char *out, size_t size);
int         ensure_dir(const char *path);
int         file_exists(const char *path);
long        file_size(const char *path);
int         replace_file(const char *tmp_path, const char *final_path);

/* ---------- hashing ---------- */
void     hash_password(const char *username, const char *password, char out[HASH_LEN]);

/* ---------- deterministic pseudo random numbers ---------- */
uint32_t hash32(uint32_t x);
uint32_t make_seed(int a, int b, int c);
double   noise_unit(uint32_t seed);     /* value in [0, 1)  */
double   noise_signed(uint32_t seed);   /* value in [-1, 1] */

/* ---------- maths ---------- */
double clampd(double v, double lo, double hi);
int    clampi(int v, int lo, int hi);

#endif /* UTILS_H */
