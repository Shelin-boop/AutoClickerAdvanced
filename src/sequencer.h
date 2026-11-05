/*
 * AutoClicker Advanced - line scheduler
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: every line has its own interval; lines that are due at
 * the same time are clicked one after another.
 */
#ifndef XCA_SEQUENCER_H
#define XCA_SEQUENCER_H

#include <glib.h>

#include "config.h"

typedef struct {
    gboolean running;
    int step;          /* line of the last click, -1 none */
    gint64 clicks;     /* clicks sent */
    gint64 line_clicks[NSTEPS];
    gboolean m_paused; /* mouse position line waits (mouse moving) */
    gboolean p_paused; /* position lines wait (mouse moving) */
    int missed;        /* clicks without a target window */
    int unshielded;    /* position clicks where the mouse could not be held off */
    gboolean manual;   /* all lines wait: the user clicks */
    /* reactions */
    float r_best[NFRAMES];   /* best similarity of the last check */
    int r_ms[NFRAMES];       /* duration of the last check */
    gint64 r_checks[NFRAMES];
    int lr_step;             /* last reaction: line, -1 none */
    int lr_x, lr_y;
    float lr_score;
    char lr_time[16];        /* hh:mm:ss */
} SeqStatus;

/*
 * Starts with a copy of the configuration. Returns FALSE and an English
 * message in *err if the configuration cannot run.
 * on_finished is called in the GTK main thread when the run ends.
 */
gboolean seq_start(const Config *c, void (*on_finished)(void), char **err);
void seq_stop(void);
gboolean seq_running(void);
void seq_status(SeqStatus *out);

#endif
