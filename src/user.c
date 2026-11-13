/*
 * user.c
 * ---------------------------------------------------------------
 * User account management.
 *
 * data/users.txt:
 *   username|password_hash|full_name|home_location|theme|active|created|last_login
 * data/admin.txt:
 *   username|password_hash        (default account: admin / admin123)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "user.h"
#include "utils.h"

#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin123"

static User g_users[MAX_USERS];
static int  g_count = 0;

static char g_admin_user[USERNAME_LEN] = DEFAULT_ADMIN_USER;
static char g_admin_hash[HASH_LEN]     = "";

/* =============================================================
 *  STORAGE
 * ============================================================= */

static int parse_user(char *line, User *u)
{
    char *f[9];
    int n = split_fields(line, FIELD_SEP, f, 9);
    if (n < 8) return 0;

    memset(u, 0, sizeof *u);
    str_copy(u->username,      f[0], sizeof u->username);
    str_copy(u->password_hash, f[1], sizeof u->password_hash);
    str_copy(u->full_name,     f[2], sizeof u->full_name);
    u->home_location = atoi(f[3]);
    u->theme         = atoi(f[4]);
    u->active        = atoi(f[5]) ? 1 : 0;
    str_copy(u->created,    f[6], sizeof u->created);
    str_copy(u->last_login, f[7], sizeof u->last_login);
    return u->username[0] != '\0';
}

int user_load(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    g_count = 0;
    data_path(FILE_USERS, path, sizeof path);
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    while (fgets(line, sizeof line, fp) != NULL && g_count < MAX_USERS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (parse_user(line, &g_users[g_count])) g_count++;
    }
    fclose(fp);
    return g_count;
}

int user_save(void)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    FILE *fp;
    int i;

    data_path(FILE_USERS, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# username|password_hash|full_name|home_location|theme(0=dark,1=light,"
                "2=classic)|active|created|last_login\n");
    for (i = 0; i < g_count; i++) {
        const User *u = &g_users[i];
        fprintf(fp, "%s|%s|%s|%d|%d|%d|%s|%s\n", u->username, u->password_hash,
                u->full_name, u->home_location, u->theme, u->active,
                u->created, u->last_login[0] ? u->last_login : "-");
    }
    fclose(fp);
    return replace_file(tmp, path);
}

/* =============================================================
 *  ACCESS
 * ============================================================= */

int user_count(void) { return g_count; }

User *user_at(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return &g_users[index];
}

/* Usernames are case-insensitive: "Rahim" and "rahim" are the same. */
User *user_find(const char *username)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (str_iequals(g_users[i].username, username)) return &g_users[i];
    return NULL;
}

/* =============================================================
 *  VALIDATION
 * ============================================================= */

int validate_username(const char *u, char *err, size_t size)
{
    size_t len = strlen(u), i;

    if (len < 3 || len > USERNAME_LEN - 1) {
        snprintf(err, size, "Username must be 3-%d characters long.", USERNAME_LEN - 1);
        return 0;
    }
    if (!isalpha((unsigned char)u[0])) {
        snprintf(err, size, "Username must start with a letter.");
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (!isalnum((unsigned char)u[i]) && u[i] != '_') {
            snprintf(err, size, "Use only letters, digits and underscore (_).");
            return 0;
        }
    }
    if (str_iequals(u, g_admin_user) || str_iequals(u, "admin")) {
        snprintf(err, size, "This username is reserved.");
        return 0;
    }
    return 1;
}

int validate_password(const char *p, char *err, size_t size)
{
    size_t len = strlen(p), i;
    int letter = 0, digit = 0;

    if (len < MIN_PASSWORD_LEN || len > PASSWORD_LEN - 1) {
        snprintf(err, size, "Password must be %d-%d characters long.",
                 MIN_PASSWORD_LEN, PASSWORD_LEN - 1);
        return 0;
    }
    for (i = 0; i < len; i++) {
        if (isalpha((unsigned char)p[i])) letter = 1;
        else if (isdigit((unsigned char)p[i])) digit = 1;
        if (p[i] == ' ' || p[i] == FIELD_SEP) {
            snprintf(err, size, "Password must not contain spaces or '|'.");
            return 0;
        }
    }
    if (!letter || !digit) {
        snprintf(err, size, "Password must contain at least one letter and one digit.");
        return 0;
    }
    return 1;
}

int validate_full_name(const char *name, char *err, size_t size)
{
    size_t len = strlen(name), i;
    if (len < 2 || len > NAME_LEN - 1) {
        snprintf(err, size, "Name must be 2-%d characters long.", NAME_LEN - 1);
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!isalpha(c) && c != ' ' && c != '.' && c != '-' && c != '\'') {
            snprintf(err, size, "Name may contain only letters, spaces, '.', '-' and '.");
            return 0;
        }
    }
    return 1;
}

/* =============================================================
 *  ACCOUNT OPERATIONS
 * ============================================================= */

/* Returns 0 on success, -1 username taken, -2 storage full, -3 file error. */
int user_register(const char *username, const char *password,
                  const char *full_name, int home_location)
{
    User u;

    if (user_find(username) != NULL) return -1;
    if (g_count >= MAX_USERS) return -2;

    memset(&u, 0, sizeof u);
    str_copy(u.username, username, sizeof u.username);
    hash_password(username, password, u.password_hash);
    str_copy(u.full_name, full_name, sizeof u.full_name);
    sanitize_field(u.full_name);
    u.home_location = home_location;
    u.theme = 0;
    u.active = 1;
    now_string(u.created, sizeof u.created);
    str_copy(u.last_login, "-", sizeof u.last_login);

    g_users[g_count++] = u;
    return user_save() == 0 ? 0 : -3;
}

AuthResult user_login(const char *username, const char *password)
{
    char hash[HASH_LEN];
    User *u = user_find(username);

    if (u == NULL) return AUTH_NO_USER;
    hash_password(u->username, password, hash);
    if (strcmp(hash, u->password_hash) != 0) return AUTH_WRONG_PASSWORD;
    if (!u->active) return AUTH_BLOCKED;

    now_string(u->last_login, sizeof u->last_login);
    user_save();
    return AUTH_OK;
}

int user_check_password(const char *username, const char *password)
{
    char hash[HASH_LEN];
    User *u = user_find(username);
    if (u == NULL) return 0;
    hash_password(u->username, password, hash);
    return strcmp(hash, u->password_hash) == 0;
}

int user_change_password(const char *username, const char *new_password)
{
    User *u = user_find(username);
    if (u == NULL) return -1;
    hash_password(u->username, new_password, u->password_hash);
    return user_save();
}

int user_delete(const char *username)
{
    int i;
    for (i = 0; i < g_count; i++) {
        if (str_iequals(g_users[i].username, username)) {
            for (; i < g_count - 1; i++) g_users[i] = g_users[i + 1];
            g_count--;
            return user_save();
        }
    }
    return -1;
}

int user_set_active(const char *username, int active)
{
    User *u = user_find(username);
    if (u == NULL) return -1;
    u->active = active ? 1 : 0;
    return user_save();
}

int user_update(const User *changed)
{
    User *u = user_find(changed->username);
    if (u == NULL) return -1;
    *u = *changed;
    sanitize_field(u->full_name);
    return user_save();
}

/* When a location is deleted, users who had it as home get a replacement. */
int user_clear_home_location(int location_id, int replacement)
{
    int i, changed = 0;
    for (i = 0; i < g_count; i++) {
        if (g_users[i].home_location == location_id) {
            g_users[i].home_location = replacement;
            changed++;
        }
    }
    if (changed) user_save();
    return changed;
}

/* =============================================================
 *  ADMINISTRATOR
 * ============================================================= */

static int admin_save(void)
{
    char path[PATH_LEN];
    FILE *fp;
    data_path(FILE_ADMIN, path, sizeof path);
    fp = fopen(path, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "# username|password_hash   (default login: admin / admin123)\n");
    fprintf(fp, "%s|%s\n", g_admin_user, g_admin_hash);
    fclose(fp);
    return 0;
}

/* Load admin.txt, or create it with the default account. */
void admin_ensure_default(void)
{
    char path[PATH_LEN];
    char line[LINE_LEN];
    FILE *fp;

    data_path(FILE_ADMIN, path, sizeof path);
    fp = fopen(path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof line, fp) != NULL) {
            char *f[2];
            if (line[0] == '#') continue;
            if (split_fields(line, FIELD_SEP, f, 2) == 2) {
                str_copy(g_admin_user, f[0], sizeof g_admin_user);
                str_copy(g_admin_hash, f[1], sizeof g_admin_hash);
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    str_copy(g_admin_user, DEFAULT_ADMIN_USER, sizeof g_admin_user);
    hash_password(DEFAULT_ADMIN_USER, DEFAULT_ADMIN_PASS, g_admin_hash);
    admin_save();
}

int admin_login(const char *username, const char *password)
{
    char hash[HASH_LEN];
    if (!str_iequals(username, g_admin_user)) return 0;
    hash_password(g_admin_user, password, hash);
    return strcmp(hash, g_admin_hash) == 0;
}

int admin_change_password(const char *new_password)
{
    hash_password(g_admin_user, new_password, g_admin_hash);
    return admin_save();
}

const char *admin_username(void) { return g_admin_user; }
