/*
 * AutoClicker Advanced - screen capture from X11 windows
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Under Xwayland the root window cannot be read (it is black); the content is
 * read from the window under the area instead (phase 0 findings).
 */
#ifndef XCA_CAPTURE_H
#define XCA_CAPTURE_H

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <glib.h>

#include "config.h"

typedef struct {
    int ox, oy;         /* screen position of pixel (0, 0) */
    int w, h;
    unsigned char *rgb; /* 3 bytes per pixel, stride w * 3 */
} Shot;

/* reusable shared-memory image for repeated captures of one area */
typedef struct {
    Window win;
    int w, h;
    XShmSegmentInfo si;
    XImage *img;
    gboolean shm_failed;
} CapCache;

/* captures the screen area r (clipped to the window under its center) */
gboolean cap_area(Display *d, CapCache *cc, const Rect *r, Shot *out);
void cap_cache_free(Display *d, CapCache *cc);

/* captures the whole window under the screen point (rx, ry) */
gboolean cap_window_at(Display *d, int rx, int ry, Shot *out);

void shot_free(Shot *s);

#endif
