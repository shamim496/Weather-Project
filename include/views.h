/*
 * views.h
 * ---------------------------------------------------------------
 * Screens that show weather information. They are shared by the
 * user module and the administrator module.
 */
#ifndef VIEWS_H
#define VIEWS_H

#include "alert.h"

/* Ask the user for a location (name, part of a name or id). 0 = cancelled */
int  view_select_location(const char *prompt, int default_id);

void view_location_list(void);
void view_current_weather(int location_id);
void view_hourly_forecast(int location_id);
void view_daily_forecast(int location_id);
void view_advisory(int location_id);
void view_compare(int first_id, int second_id);
void view_weather_table(void);

/* Alerts for a set of locations (manual + automatically detected). */
int  view_collect_alerts(const int ids[], int n, Alert out[], int max);
void view_alerts(const int ids[], int n);
void view_print_alert(const Alert *a);

/* Colour role used for a weather condition. */
int  view_condition_color(int condition);

#endif /* VIEWS_H */
