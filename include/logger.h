/*
 * logger.h
 * ---------------------------------------------------------------
 * Activity log (data/system.log). Used by the admin
 * "System Monitoring" screen to show recent events such as
 * logins, failed logins, registrations and data changes.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include "common.h"

void log_event(const char *type, const char *fmt, ...);
int  log_tail(char lines[][LINE_LEN], int max_lines);   /* newest last */
long log_count_type(const char *type);

#endif /* LOGGER_H */
