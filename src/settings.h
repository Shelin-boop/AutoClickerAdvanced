/*
 * AutoClicker Advanced - settings dialog
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: built in code, new settings for the step sequence.
 */
#ifndef XCA_SETTINGS_H
#define XCA_SETTINGS_H

#include <gtk/gtk.h>

void settings_run(GtkWindow *parent);
/* hotkey capture result from the hotkey thread (main thread) */
void settings_on_capture(int mod, int key, gboolean done);

#endif
