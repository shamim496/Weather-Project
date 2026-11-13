/*
 * user.h
 * ---------------------------------------------------------------
 * User accounts, registration, login and administrator
 * credentials. Passwords are stored only as salted hashes.
 */
#ifndef USER_H
#define USER_H

#include "common.h"

typedef struct {
    char username[USERNAME_LEN];
    char password_hash[HASH_LEN];
    char full_name[NAME_LEN];
    int  home_location;            /* location id                */
    int  theme;                    /* Theme (Dark/Light/Classic) */
    int  active;                   /* 1 = active, 0 = blocked    */
    char created[TIME_LEN];
    char last_login[TIME_LEN];
} User;

typedef enum {
    AUTH_OK = 0,
    AUTH_NO_USER,
    AUTH_WRONG_PASSWORD,
    AUTH_BLOCKED
} AuthResult;

#define MIN_PASSWORD_LEN 6
#define MAX_LOGIN_TRIES  3

/* ---------- storage ---------- */
int   user_load(void);
int   user_save(void);

/* ---------- access ---------- */
int   user_count(void);
User *user_at(int index);
User *user_find(const char *username);

/* ---------- validation ---------- */
int   validate_username(const char *username, char *error, size_t size);
int   validate_password(const char *password, char *error, size_t size);
int   validate_full_name(const char *name, char *error, size_t size);

/* ---------- account operations ---------- */
int        user_register(const char *username, const char *password,
                         const char *full_name, int home_location);
AuthResult user_login(const char *username, const char *password);
int        user_change_password(const char *username, const char *new_password);
int        user_check_password(const char *username, const char *password);
int        user_delete(const char *username);
int        user_set_active(const char *username, int active);
int        user_update(const User *u);
int        user_clear_home_location(int location_id, int replacement);

/* ---------- administrator ---------- */
void  admin_ensure_default(void);
int   admin_login(const char *username, const char *password);
int   admin_change_password(const char *new_password);
const char *admin_username(void);

#endif /* USER_H */
