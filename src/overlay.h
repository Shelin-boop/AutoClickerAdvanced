/*
 * AutoClicker Advanced - on-screen overlays (click-through X11 windows)
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 * All functions must be called from the GTK main thread.
 */
#ifndef XCA_OVERLAY_H
#define XCA_OVERLAY_H

#include <X11/Xlib.h>
#include <glib.h>

#include "config.h"

gboolean ov_init(void);
Display *ov_display(void);

/* position markers for P1..P3 (index 0..2); x < 0 hides that marker */
void ov_markers_set(int idx, int x, int y, gboolean enabled);
void ov_markers_show(gboolean show);

/* reaction frames with their exclusion zones; shown together with the markers */
void ov_frames_set(int idx, const FrameCfg *f, gboolean enabled);

/* yellow banner at the top of the screen; title NULL hides it */
void ov_banner(const char *title, const char *text);

/* large ring that follows the mouse while a position is picked; idx < 0 hides */
void ov_pick(int idx, int x, int y);

#endif
