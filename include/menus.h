/*
 * menus.h
 * ---------------------------------------------------------------
 * Interactive menus of the two modules:
 *   - User module   (after a user logs in)
 *   - Admin module  (after the administrator logs in)
 */
#ifndef MENUS_H
#define MENUS_H

void user_session_run(const char *username);
void admin_session_run(void);

#endif /* MENUS_H */
