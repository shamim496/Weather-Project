/*
 * report.h
 * ---------------------------------------------------------------
 * Text report generation (saved in the reports/ folder).
 */
#ifndef REPORT_H
#define REPORT_H

#include "common.h"

int report_weather_summary(char *out_path, size_t size);
int report_location_forecast(int location_id, char *out_path, size_t size);
int report_user_activity(char *out_path, size_t size);
int report_print_file(const char *path);

#endif /* REPORT_H */
