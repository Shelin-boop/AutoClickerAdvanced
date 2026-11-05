/*
 * AutoClicker Advanced - global hotkeys (XInput2 raw events)
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: raw events, position picking, own callbacks.
 */
#ifndef XCA_HOTKEYS_H
#define XCA_HOTKEYS_H

#include <glib.h>

typedef struct {
    /* start/stop hotkey pressed (TRUE) or released (FALSE) */
    void (*hotkey)(gboolean pressed);
    /* position picking finished: ok FALSE = cancelled */
    void (*pick_done)(int step, int x, int y, gboolean ok);
    /* hotkey capture for the settings dialog: done TRUE when the main key is set */
    void (*capture)(int mod, int key, gboolean done);
    /* screenshot hotkey pressed */
    void (*shot)(void);
} HotkeyCallbacks;

/* callbacks are invoked in the GTK main thread */
void hk_start(const HotkeyCallbacks *cb);
void hk_set_picking(int step); /* 0 = not picking */
int hk_picking(void);
void hk_capture_begin(void);
void hk_capture_cancel(void);

#endif
