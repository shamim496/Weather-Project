/*
 * ui.c
 * ---------------------------------------------------------------
 * Console user interface implementation.
 *
 * Colours are produced with ANSI escape sequences. On Windows 10+
 * the "virtual terminal" mode is switched on so the same sequences
 * work in cmd.exe, PowerShell, Windows Terminal, VS Code and
 * Code::Blocks. When the terminal cannot show colours the program
 * falls back to the Classic (plain text) theme automatically.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L     /* for fileno() and termios */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>      /* _getch */
#include <io.h>         /* _isatty */
#define IS_TTY(f) _isatty(_fileno(f))
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>     /* isatty */
#include <termios.h>    /* hide password characters */
#define IS_TTY(f) isatty(fileno(f))
#endif

#include "ui.h"
#include "utils.h"
#include "common.h"

static int  g_theme        = THEME_DARK;
static int  g_ansi_ok      = 0;   /* terminal understands colours  */
static int  g_stdout_tty   = 0;
static int  g_stdin_tty    = 0;
static int  g_echo_input   = 0;   /* echo piped input (for test logs) */
static char g_status[128]  = "";

/* Background + default text colour of each theme */
static const char *THEME_BASE[THEME_COUNT] = {
    "\033[0;40;97m",     /* Dark : black background, white text */
    "\033[0;107;30m",    /* Light: white background, black text */
    ""                   /* Classic: terminal default           */
};

/* Text colour for every role in every theme */
static const char *THEME_COLORS[THEME_COUNT][COL_COUNT] = {
    /* DARK */
    { "\033[22;97m", "\033[1;96m", "\033[22;93m", "\033[1;92m", "\033[1;93m",
      "\033[1;91m",  "\033[22;90m", "\033[1;95m", "\033[22;96m" },
    /* LIGHT */
    { "\033[22;30m", "\033[1;34m", "\033[22;35m", "\033[1;32m", "\033[1;33m",
      "\033[1;31m",  "\033[22;90m", "\033[1;35m", "\033[22;36m" },
    /* CLASSIC */
    { "", "", "", "", "", "", "", "", "" }
};

static const char *THEME_NAMES[THEME_COUNT] = { "Dark", "Light", "Classic (no colour)" };

/* =============================================================
 *  SETUP
 * ============================================================= */

void ui_init(void)
{
    const char *echo = getenv("WFS_ECHO_INPUT");

    g_stdout_tty = IS_TTY(stdout) ? 1 : 0;
    g_stdin_tty  = IS_TTY(stdin) ? 1 : 0;
    g_echo_input = (echo != NULL && echo[0] == '1');

#ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        g_ansi_ok = 0;
        if (g_stdout_tty && GetConsoleMode(h, &mode) &&
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            g_ansi_ok = 1;
    }
#else
    g_ansi_ok = g_stdout_tty;
#endif
    if (getenv("NO_COLOR") != NULL) g_ansi_ok = 0;
}

/* Put the terminal back to its normal colours before exiting. */
void ui_restore(void)
{
    if (g_ansi_ok) printf("\033[0m");
#ifdef _WIN32
    if (!g_ansi_ok && g_stdout_tty) system("color 07");
#endif
    fflush(stdout);
}

void ui_set_theme(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) theme = THEME_DARK;
    g_theme = theme;
#ifdef _WIN32
    /* Old Windows console without ANSI support: use the "color" command */
    if (!g_ansi_ok && g_stdout_tty)
        system(theme == THEME_LIGHT ? "color F0" : "color 07");
#endif
    ui_reset();
}

int ui_get_theme(void) { return g_theme; }

const char *ui_theme_name(int theme)
{
    if (theme < 0 || theme >= THEME_COUNT) return "Unknown";
    return THEME_NAMES[theme];
}

void ui_set_status(const char *status)
{
    str_copy(g_status, status ? status : "", sizeof g_status);
}

/* =============================================================
 *  COLOURS
 * ============================================================= */

void ui_color(ColorRole role)
{
    if (!g_ansi_ok || role < 0 || role >= COL_COUNT) return;
    fputs(THEME_COLORS[g_theme][role], stdout);
}

void ui_reset(void)
{
    if (!g_ansi_ok) return;
    fputs(THEME_BASE[g_theme], stdout);
}

/* =============================================================
 *  LAYOUT
 * ============================================================= */

void ui_clear(void)
{
    if (!g_stdout_tty) {             /* output redirected: just separate screens */
        printf("\n");
        return;
    }
    if (g_ansi_ok) {
        ui_reset();
        printf("\033[2J\033[3J\033[H");
    } else {
#ifdef _WIN32
        system("cls");
#else
        printf("\033[2J\033[H");
#endif
    }
    fflush(stdout);
}

void ui_divider(char ch)
{
    int i;
    ui_color(COL_MUTED);
    for (i = 0; i < UI_WIDTH; i++) putchar(ch);
    putchar('\n');
    ui_reset();
}

void ui_center(const char *text, int width)
{
    int len = (int)strlen(text);
    int left = (width - len) / 2;
    if (left < 0) left = 0;
    printf("%*s%s", left, "", text);
}

/* Large banner shown on the main menu */
void ui_banner(void)
{
    ui_clear();
    ui_color(COL_WARNING);
    printf("       \\  |  /    ");
    ui_color(COL_TITLE);
    printf(" ________________________________________________________\n");
    ui_color(COL_WARNING);
    printf("     '-.;;;.-'    ");
    ui_color(COL_TITLE);
    printf("|                                                        |\n");
    ui_color(COL_WARNING);
    printf("    -==;;;;;==-   ");
    ui_color(COL_TITLE);
    printf("|      W E A T H E R   F O R E C A S T I N G             |\n");
    ui_color(COL_INFO);
    printf("      .-~~~~-.    ");
    ui_color(COL_TITLE);
    printf("|                    S Y S T E M                         |\n");
    ui_color(COL_INFO);
    printf("   .-(  ~~~~  )-. ");
    ui_color(COL_TITLE);
    printf("|________________________________________________________|\n");
    ui_color(COL_INFO);
    printf("  (_______________)");
    ui_color(COL_MUTED);
    printf("  Version %s  |  Written in C  |  File based storage\n", APP_VERSION);
    ui_color(COL_INFO);
    printf("    ' ' ' ' ' '\n");
    ui_reset();
    ui_divider('=');
}

/* Small header shown on every inner screen */
void ui_header(const char *title)
{
    char date[TIME_LEN];

    ui_clear();
    format_time_short(time(NULL), date, sizeof date);

    ui_color(COL_TITLE);
    printf("+");
    { int i; for (i = 0; i < UI_WIDTH - 2; i++) putchar('='); }
    printf("+\n|");
    ui_color(COL_WARNING);
    printf("  (*) %-40s", "WEATHER FORECASTING SYSTEM");
    ui_color(COL_MUTED);
    printf("%28s  ", date);
    ui_color(COL_TITLE);
    printf("|\n+");
    { int i; for (i = 0; i < UI_WIDTH - 2; i++) putchar('='); }
    printf("+\n");
    ui_reset();

    if (g_status[0] != '\0') {
        ui_color(COL_MUTED);
        printf("  %s\n", g_status);
        ui_reset();
    }
    printf("\n");
    ui_color(COL_HIGHLIGHT);
    printf("  >> %s\n", title);
    ui_reset();
    ui_divider('-');
}

void ui_section(const char *title)
{
    printf("\n");
    ui_color(COL_TITLE);
    printf("  %s\n", title);
    ui_reset();
    ui_divider('-');
}

void ui_menu_item(int key, const char *text)
{
    ui_color(COL_ACCENT);
    printf("   [%2d] ", key);
    ui_reset();
    printf("%s\n", text);
}

void ui_menu_item_tag(int key, const char *text, const char *tag)
{
    ui_color(COL_ACCENT);
    printf("   [%2d] ", key);
    ui_reset();
    printf("%s ", text);
    ui_color(COL_HIGHLIGHT);
    printf("%s\n", tag);
    ui_reset();
}

/* Draw a horizontal bar, e.g. ########.... (fraction 0..1) */
void ui_bar(double fraction, int width, char ch)
{
    int i, filled;
    if (fraction < 0) fraction = 0;
    if (fraction > 1) fraction = 1;
    filled = (int)(fraction * width + 0.5);
    for (i = 0; i < width; i++) putchar(i < filled ? ch : ' ');
}

/* =============================================================
 *  MESSAGES
 * ============================================================= */

static void print_message(ColorRole role, const char *label, const char *fmt, va_list ap)
{
    ui_color(role);
    printf("  %s ", label);
    vprintf(fmt, ap);
    printf("\n");
    ui_reset();
}

void ui_msg_success(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_SUCCESS, "[OK]", fmt, ap);
    va_end(ap);
}

void ui_msg_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_ERROR, "[ERROR]", fmt, ap);
    va_end(ap);
}

void ui_msg_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_WARNING, "[!]", fmt, ap);
    va_end(ap);
}

void ui_msg_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print_message(COL_INFO, "[i]", fmt, ap);
    va_end(ap);
}

/* =============================================================
 *  INPUT
 * ============================================================= */

/* Called when the input stream ends (Ctrl+Z / Ctrl+D or end of a test script). */
static void input_end(void)
{
    printf("\n");
    ui_msg_info("Input closed. Goodbye!");
    ui_restore();
    exit(0);
}

/* Read one line from stdin, remove the new line, flush overflow. */
static int read_raw_line(char *buf, size_t size)
{
    size_t len;

    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) input_end();

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
    } else {
        int c;   /* line was longer than the buffer: discard the rest */
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    if (!g_stdin_tty && g_echo_input) printf("%s\n", buf);
    return (int)len;
}

static void print_prompt(const char *prompt)
{
    ui_color(COL_ACCENT);
    printf("  %s", prompt);
    ui_reset();
    fflush(stdout);
}

int input_line(const char *prompt, char *buf, size_t size)
{
    print_prompt(prompt);
    read_raw_line(buf, size);
    str_trim(buf);
    return (int)strlen(buf);
}

int input_int(const char *prompt, int min, int max)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && v >= min && v <= max)
            return (int)v;
        ui_msg_error("Please enter a whole number between %d and %d.", min, max);
    }
}

int input_int_default(const char *prompt, int min, int max, int def)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (buf[0] == '\0') return def;
        v = strtol(buf, &end, 10);
        if (*end == '\0' && v >= min && v <= max) return (int)v;
        ui_msg_error("Please enter a whole number between %d and %d (Enter = %d).",
                     min, max, def);
    }
}

double input_double(const char *prompt, double min, double max)
{
    char buf[64];
    char *end;
    double v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        v = strtod(buf, &end);
        if (buf[0] != '\0' && *end == '\0' && v >= min && v <= max) return v;
        ui_msg_error("Please enter a number between %.1f and %.1f.", min, max);
    }
}

double input_double_default(const char *prompt, double min, double max, double def)
{
    char buf[64];
    char *end;
    double v;

    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (buf[0] == '\0') return def;
        v = strtod(buf, &end);
        if (*end == '\0' && v >= min && v <= max) return v;
        ui_msg_error("Please enter a number between %.1f and %.1f (Enter = %.1f).",
                     min, max, def);
    }
}

int input_yes_no(const char *prompt)
{
    char buf[16];
    for (;;) {
        print_prompt(prompt);
        read_raw_line(buf, sizeof buf);
        str_trim(buf);
        if (str_iequals(buf, "y") || str_iequals(buf, "yes")) return 1;
        if (str_iequals(buf, "n") || str_iequals(buf, "no"))  return 0;
        ui_msg_error("Please answer y (yes) or n (no).");
    }
}

/* Password input: characters are shown as '*' when typing in a terminal. */
void input_password(const char *prompt, char *buf, size_t size)
{
    size_t len = 0;

    if (!g_stdin_tty) {                /* input comes from a file / pipe */
        print_prompt(prompt);
        read_raw_line(buf, size);
        return;
    }

    print_prompt(prompt);
#ifdef _WIN32
    for (;;) {
        int c = _getch();
        if (c == '\r' || c == '\n') break;
        if (c == 3) input_end();                    /* Ctrl+C */
        if (c == 8) {                               /* Backspace */
            if (len > 0) { len--; printf("\b \b"); }
            continue;
        }
        if (c == 0 || c == 224) { _getch(); continue; } /* arrow / function keys */
        if (len + 1 < size && c >= 32 && c < 127) {
            buf[len++] = (char)c;
            putchar('*');
        }
        fflush(stdout);
    }
#else
    {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= (tcflag_t)~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        for (;;) {
            int c = getchar();
            if (c == EOF) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); input_end(); }
            if (c == '\n' || c == '\r') break;
            if (c == 127 || c == 8) {
                if (len > 0) { len--; printf("\b \b"); fflush(stdout); }
                continue;
            }
            if (len + 1 < size && c >= 32 && c < 127) {
                buf[len++] = (char)c;
                putchar('*');
                fflush(stdout);
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
#endif
    buf[len] = '\0';
    printf("\n");
}

void ui_pause(void)
{
    char buf[8];
    printf("\n");
    ui_color(COL_MUTED);
    printf("  Press Enter to continue...");
    ui_reset();
    read_raw_line(buf, sizeof buf);
}
