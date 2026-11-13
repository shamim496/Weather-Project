/*
 * ui.h
 * ---------------------------------------------------------------
 * Console user interface: colour themes (Dark / Light / Classic),
 * screen layout helpers and safe keyboard input functions.
 */
#ifndef UI_H
#define UI_H

#include <stddef.h>

#define UI_WIDTH 78

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT = 1,
    THEME_CLASSIC = 2,      /* no colours - for old terminals */
    THEME_COUNT
} Theme;

typedef enum {
    COL_NORMAL = 0,
    COL_TITLE,
    COL_ACCENT,
    COL_SUCCESS,
    COL_WARNING,
    COL_ERROR,
    COL_MUTED,
    COL_HIGHLIGHT,
    COL_INFO,
    COL_COUNT
} ColorRole;

/* ---------- setup ---------- */
void        ui_init(void);
void        ui_restore(void);
void        ui_set_theme(int theme);
int         ui_get_theme(void);
const char *ui_theme_name(int theme);
void        ui_set_status(const char *status);

/* ---------- colours ---------- */
void ui_color(ColorRole role);
void ui_reset(void);

/* ---------- layout ---------- */
void ui_clear(void);
void ui_banner(void);
void ui_header(const char *title);
void ui_section(const char *title);
void ui_divider(char ch);
void ui_center(const char *text, int width);
void ui_menu_item(int key, const char *text);
void ui_menu_item_tag(int key, const char *text, const char *tag);
void ui_bar(double fraction, int width, char ch);

/* ---------- messages ---------- */
void ui_msg_success(const char *fmt, ...);
void ui_msg_error(const char *fmt, ...);
void ui_msg_warning(const char *fmt, ...);
void ui_msg_info(const char *fmt, ...);
void ui_pause(void);

/* ---------- input (all functions validate and re-ask) ---------- */
int    input_line(const char *prompt, char *buf, size_t size);
int    input_int(const char *prompt, int min, int max);
int    input_int_default(const char *prompt, int min, int max, int def);
double input_double(const char *prompt, double min, double max);
double input_double_default(const char *prompt, double min, double max, double def);
int    input_yes_no(const char *prompt);
void   input_password(const char *prompt, char *buf, size_t size);

#endif /* UI_H */
