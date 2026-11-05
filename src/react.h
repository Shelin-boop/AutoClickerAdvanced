/*
 * AutoClicker Advanced - pattern reactions
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 */
#ifndef XCA_REACT_H
#define XCA_REACT_H

#include <X11/Xlib.h>
#include <glib.h>

#include "capture.h"
#include "config.h"
#include "match.h"

#define REACT_KNOWN 32
#define REACT_LOCKS 16

typedef struct {
    int x, y;
    gint64 t; /* last seen (known) or end of the lock */
} RPoint;

typedef struct {
    FrameCfg f;
    CapCache cache;
    RPoint known[REACT_KNOWN];
    int nk;
    RPoint lock[REACT_LOCKS];
    int nl;
    gboolean started; /* first check done (baseline) */
    float best;       /* best similarity of the last check */
    int ms;           /* duration of the last check */
    gint64 checks;
} React;

void react_init(React *r, const FrameCfg *f);
void react_free(React *r, Display *d);

/*
 * Captures and searches the frame. Returns TRUE with the screen position and
 * similarity of a newly appeared pattern that should be clicked.
 */
gboolean react_check(React *r, Display *d, const Pattern *p, const Config *c, int *tx, int *ty, float *score);

/* after a click at (x, y): no further click there for the lock time */
void react_clicked(React *r, const Config *c, int x, int y);

#endif
