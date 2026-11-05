/*
 * AutoClicker Advanced - line scheduler
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03:
 *  - every enabled line has its own interval (time between two of its
 *    clicks); all lines are due once right after the start
 *  - only one click runs at a time; of the due lines the one that has been due
 *    longest is clicked first, on equal times the upper line; between two
 *    clicks there is at least the event wait time
 *  - the mouse position line (and optionally the position lines) wait while
 *    the mouse moves and click as soon as it rests
 */
#include "sequencer.h"

#include <X11/Xlib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <string.h>

#include "match.h"
#include "react.h"
#include "x11api.h"

#define LAST_LINE STEP_R3

static Config run_cfg;
static Pattern run_pattern;
static gboolean have_run_pattern;
static gint running;
static GMutex st_lock;
static SeqStatus st;
static void (*finished_cb)(void);

gboolean seq_running(void)
{
    return g_atomic_int_get(&running);
}

void seq_stop(void)
{
    g_atomic_int_set(&running, 0);
}

void seq_status(SeqStatus *out)
{
    g_mutex_lock(&st_lock);
    *out = st;
    out->running = seq_running();
    g_mutex_unlock(&st_lock);
}

static int rand_between(int lo, int hi)
{
    if (hi <= lo)
        return lo;
    return lo + rand() % (hi - lo + 1);
}

typedef struct {
    Display *d;
    int ax, ay;         /* anchor of the motion detection */
    gint64 still_since; /* time the mouse came to rest */
    int px, py;         /* last pointer sample */
    gboolean have_ptr;
    gint64 manual_until; /* the user clicks: automatic clicks wait until then */
} Motion;

static void motion_sample(Motion *m)
{
    int x, y;
    gboolean buttons = FALSE;
    if (!xa_pointer_buttons(m->d, &x, &y, &buttons)) {
        m->have_ptr = FALSE;
        return;
    }
    if (buttons) /* held: wait until the release plus the pause after a click */
        m->manual_until = xa_now_ms() + run_cfg.manual_pause_ms;
    m->have_ptr = TRUE;
    m->px = x;
    m->py = y;
    int dx = x - m->ax, dy = y - m->ay, t = run_cfg.motion_threshold_px;
    if (dx * dx + dy * dy > t * t) {
        m->ax = x;
        m->ay = y;
        m->still_since = xa_now_ms();
    }
}

/* waits until 'until' while sampling the mouse; returns FALSE if stopped */
static gboolean wait_until(Motion *m, gint64 until)
{
    for (;;) {
        if (!g_atomic_int_get(&running))
            return FALSE;
        gint64 left = until - xa_now_ms();
        if (left <= 0)
            return TRUE;
        xa_sleep_ms(left > 10 ? 10 : (int)left);
        motion_sample(m);
    }
}

static int press_time(const StepCfg *s)
{
    if (s->hold_ms <= 0)
        return DEFAULT_PRESS_MS;
    int v = s->hold_random ? rand_between(MIN_PRESS_MS, s->hold_ms) : s->hold_ms;
    return v < MIN_PRESS_MS ? MIN_PRESS_MS : v;
}

/* the mouse position line needs no wait before pressing and no move back:
 * the game already has the pointer there */
static gboolean do_click(Motion *m, const StepCfg *s, int tx, int ty, gboolean position_line)
{
    int n = s->click_type == CLICK_DOUBLE ? 2 : 1;
    int settle = position_line ? run_cfg.settle_ms : 0;
    gboolean ok = TRUE;
    for (int k = 0; k < n && g_atomic_int_get(&running); k++) {
        if (k > 0 && !wait_until(m, xa_now_ms() + run_cfg.double_gap_ms))
            break;
        gboolean shielded = FALSE;
        ok = xa_click(m->d, tx, ty, s->button, press_time(s), settle, position_line && k == n - 1,
                      position_line && run_cfg.shield_pointer, &shielded) &&
             ok;
        if (position_line && run_cfg.shield_pointer && !shielded) {
            g_mutex_lock(&st_lock);
            st.unshielded++;
            g_mutex_unlock(&st_lock);
        }
    }
    return ok;
}

static gint64 next_interval(const Config *c, int i)
{
    gint64 iv = config_line_interval(c, i);
    if (c->use_random && c->random_ms > 0) {
        iv += rand_between(-c->random_ms, c->random_ms);
        if (iv < config_min_interval_ms(c))
            iv = config_min_interval_ms(c);
    }
    return iv;
}

static gboolean notify_finished(gpointer u)
{
    (void)u;
    if (finished_cb)
        finished_cb();
    return G_SOURCE_REMOVE;
}

static gpointer seq_thread(gpointer u)
{
    (void)u;
    Display *d = XOpenDisplay(NULL);
    if (!d) {
        g_atomic_int_set(&running, 0);
        g_idle_add(notify_finished, NULL);
        return NULL;
    }
    const Config *c = &run_cfg;
    Motion m = {d, 0, 0, 0, 0, 0, FALSE, 0};
    motion_sample(&m);
    m.ax = m.px;
    m.ay = m.py;
    m.still_since = xa_now_ms();

    gint64 due[NSTEPS];
    gint64 start = xa_now_ms();
    for (int i = 0; i < NSTEPS; i++)
        due[i] = start;
    gint64 clicks = 0, last_event = 0;
    React react[NFRAMES];
    for (int i = 0; i < NFRAMES; i++)
        react_init(&react[i], &c->frame[i]);

    while (g_atomic_int_get(&running)) {
        motion_sample(&m);
        gint64 now = xa_now_ms();
        gboolean moving = !m.have_ptr || now - m.still_since < c->motion_rest_ms;
        gboolean manual = c->manual_priority && now < m.manual_until;
        gboolean m_wait = FALSE, p_wait = FALSE;
        int pick = -1;
        gint64 earliest = G_MAXINT64;
        for (int i = STEP_M; i <= LAST_LINE; i++) {
            if (!c->step[i].enabled)
                continue;
            gboolean paused = manual || ((i == STEP_M) ? moving : (c->pause_positions && moving));
            if (paused) {
                if (due[i] <= now) {
                    if (i == STEP_M)
                        m_wait = TRUE;
                    else
                        p_wait = TRUE;
                }
                continue;
            }
            if (due[i] < earliest)
                earliest = due[i];
            if (due[i] <= now && (pick < 0 || due[i] < due[pick]))
                pick = i;
        }
        g_mutex_lock(&st_lock);
        st.m_paused = m_wait;
        st.p_paused = p_wait;
        st.manual = manual;
        g_mutex_unlock(&st_lock);

        if (pick < 0) {
            gint64 until = now + 10;
            if (earliest != G_MAXINT64 && earliest < until)
                until = earliest;
            if (!wait_until(&m, until))
                break;
            continue;
        }

        /* the game must see the end of the previous click in an earlier frame
         * than the start of this one */
        if (last_event > 0 && !wait_until(&m, last_event + c->settle_ms))
            break;
        const StepCfg *s = &c->step[pick];
        gint64 t0 = xa_now_ms();
        /* keep the rate: the next click is due one interval after the planned
         * time; if the line fell behind by a whole interval, count from now */
        gint64 iv = next_interval(c, pick), nd = due[pick] + iv;
        due[pick] = nd > t0 ? nd : t0 + iv;
        int tx = s->x, ty = s->y;
        if (pick >= STEP_R1) {
            int ri = pick - STEP_R1;
            float sc = 0;
            gboolean hit = react_check(&react[ri], d, &run_pattern, c, &tx, &ty, &sc);
            g_mutex_lock(&st_lock);
            st.r_best[ri] = react[ri].best;
            st.r_ms[ri] = react[ri].ms;
            st.r_checks[ri] = react[ri].checks;
            g_mutex_unlock(&st_lock);
            if (!hit)
                continue; /* only the analysis time was used */
            if (last_event > 0 && !wait_until(&m, last_event + c->settle_ms))
                break;
            react_clicked(&react[ri], c, tx, ty);
            GDateTime *dt = g_date_time_new_now_local();
            char *ts = g_date_time_format(dt, "%H:%M:%S");
            g_mutex_lock(&st_lock);
            st.lr_step = pick;
            st.lr_x = tx;
            st.lr_y = ty;
            st.lr_score = sc;
            g_strlcpy(st.lr_time, ts, sizeof st.lr_time);
            g_mutex_unlock(&st_lock);
            g_free(ts);
            g_date_time_unref(dt);
        }
        gboolean ok = pick == STEP_M ? do_click(&m, s, m.px, m.py, FALSE) : do_click(&m, s, tx, ty, TRUE);
        last_event = xa_now_ms();
        clicks++;
        g_mutex_lock(&st_lock);
        st.step = pick;
        st.clicks = clicks;
        st.line_clicks[pick]++;
        if (!ok)
            st.missed++;
        g_mutex_unlock(&st_lock);
        if (c->use_stop_after && clicks >= c->stop_after_clicks)
            g_atomic_int_set(&running, 0);
    }
    for (int i = 0; i < NFRAMES; i++)
        react_free(&react[i], d);
    XCloseDisplay(d);
    if (have_run_pattern) {
        pattern_free(&run_pattern);
        have_run_pattern = FALSE;
    }
    g_idle_add(notify_finished, NULL);
    return NULL;
}

/* loads the pattern image of the configuration */
static gboolean load_pattern(const Config *c, char **err)
{
    char *path = config_pattern_path();
    GError *e = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &e);
    g_free(path);
    if (!pb) {
        *err = g_strdup_printf("Cannot load the pattern: %s", e ? e->message : "?");
        if (e)
            g_error_free(e);
        return FALSE;
    }
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb), nch = gdk_pixbuf_get_n_channels(pb);
    int rs = gdk_pixbuf_get_rowstride(pb);
    const guchar *px = gdk_pixbuf_read_pixels(pb);
    int size = MIN(w, h);
    guchar *rgb = g_malloc((size_t)size * size * 3);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            memcpy(rgb + ((size_t)y * size + x) * 3, px + (size_t)y * rs + (size_t)x * nch, 3);
    g_object_unref(pb);
    gboolean ok = pattern_init(&run_pattern, rgb, size * 3, size, c->downscale, c->core_pct / 100.0f);
    g_free(rgb);
    if (!ok) {
        *err = g_strdup("The pattern cannot be used (too small or without contrast).");
        return FALSE;
    }
    have_run_pattern = TRUE;
    return TRUE;
}

gboolean seq_start(const Config *c, void (*on_finished)(void), char **err)
{
    if (seq_running()) {
        *err = g_strdup("Already running.");
        return FALSE;
    }
    gboolean any = FALSE, reactions = FALSE;
    for (int i = STEP_M; i <= LAST_LINE; i++) {
        if (!c->step[i].enabled)
            continue;
        any = TRUE;
        if (i >= STEP_P1 && i <= STEP_P3 && (c->step[i].x < 0 || c->step[i].y < 0)) {
            *err = g_strdup_printf("%s has no coordinates. Use \"Get\" first.", step_name(i));
            return FALSE;
        }
        if (i >= STEP_R1) {
            const Rect *r = &c->frame[i - STEP_R1].r;
            if (!c->have_pattern) {
                *err = g_strdup("No pattern yet. Take a screenshot and select the pattern first.");
                return FALSE;
            }
            if (r->w <= 0) {
                *err = g_strdup_printf("%s has no frame. Take a screenshot and draw frame %d.", step_name(i),
                                       i - STEP_R1 + 1);
                return FALSE;
            }
            if (r->w < c->pat_size || r->h < c->pat_size) {
                *err = g_strdup_printf("Frame %d is smaller than the pattern.", i - STEP_R1 + 1);
                return FALSE;
            }
            for (int j = STEP_R1; j < i; j++)
                if (c->step[j].enabled && rect_overlap(&c->frame[j - STEP_R1].r, r)) {
                    *err = g_strdup_printf("Frames %d and %d overlap. %s cannot be enabled.", j - STEP_R1 + 1,
                                           i - STEP_R1 + 1, step_name(i));
                    return FALSE;
                }
            reactions = TRUE;
        }
    }
    if (!any) {
        *err = g_strdup("No line is enabled.");
        return FALSE;
    }
    if (reactions && !load_pattern(c, err))
        return FALSE;
    run_cfg = *c;
    finished_cb = on_finished;
    g_mutex_lock(&st_lock);
    memset(&st, 0, sizeof st);
    st.step = -1;
    st.lr_step = -1;
    g_mutex_unlock(&st_lock);
    g_atomic_int_set(&running, 1);
    g_thread_unref(g_thread_new("lines", seq_thread, NULL));
    return TRUE;
}
