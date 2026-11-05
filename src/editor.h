/*
 * AutoClicker Advanced - screenshot editor for pattern, frames and exclusion zones
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 */
#ifndef XCA_EDITOR_H
#define XCA_EDITOR_H

#include <gtk/gtk.h>

#include "capture.h"

/*
 * Opens the editor on a copy of the screenshot. on_saved is called after
 * "Save" has written the configuration and the pattern image.
 */
void editor_open(GtkWindow *parent, const Shot *shot, void (*on_saved)(void));
gboolean editor_is_open(void);

#endif
