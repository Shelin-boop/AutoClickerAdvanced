/*
 * AutoClicker Advanced - start menu entry for the AppImage
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 */
#ifndef XCA_INTEGRATION_H
#define XCA_INTEGRATION_H

#include <glib.h>

/* TRUE when started from the AppImage (APPIMAGE and APPDIR are set) */
gboolean integ_available(void);
/* creates or updates the desktop file and the icon; TRUE if something changed */
gboolean integ_install(void);
void integ_remove(void);
gboolean integ_installed(void);
/* path of the desktop file, newly allocated */
char *integ_desktop_path(void);

#endif
