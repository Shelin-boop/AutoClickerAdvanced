/*
 * AutoClicker Advanced - main window
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: step list instead of the single click options.
 */
#ifndef XCA_MAINWIN_H
#define XCA_MAINWIN_H

#include <gtk/gtk.h>

GtkWidget *mw_create(GtkApplication *app);
/* reload all widgets from cfg (after "Reset preset options") */
void mw_reload(void);
/* update the Start/Stop button texts after the hotkey changed */
void mw_update_hotkey_labels(void);
/* update the screenshot button after its key or countdown changed */
void mw_update_shot_label(void);
/* TRUE while the settings dialog captures a new hotkey */
void mw_set_capturing(gboolean on);

#endif
