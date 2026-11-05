/*
 * AutoClicker Advanced - main window
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: the window is built in code; it shows the lines
 * (mouse position, positions 1 to 3, reactions 1 to 3) with their own
 * intervals, the general options and a status line.
 */
#include "mainwin.h"

#include <stdlib.h>
#include <string.h>

#include "capture.h"
#include "config.h"
#include "editor.h"
#include "hotkeys.h"
#include "overlay.h"
#include "sequencer.h"
#include "settings.h"
#include "version.h"
#include "x11api.h"

typedef struct {
    GtkWidget *chk, *iv, *btn, *type, *x, *y, *get, *hold, *rnd, *finfo;
} Row;

static struct {
    GtkWidget *win;
    Row row[NSTEPS];
    GtkWidget *hotkey_mode, *chk_stop, *e_stop, *chk_random, *e_random;
    GtkWidget *status;
    GtkWidget *start, *stop, *settings;
    GtkWidget *pat_img, *pat_label, *shot_btn, *edit_btn;
    Shot last_shot;
    guint shot_timer;
    int shot_left;
    gboolean loading, capturing;
    guint status_timer, pick_timer;
    int pick_step, pick_old_x, pick_old_y;
    char note[200];
} W;

/* ------------------------------------------------------------------ */
/* small helpers                                                      */
/* ------------------------------------------------------------------ */

static void digits_only(GtkEditable *e, const gchar *text, gint len, gint *pos, gpointer u)
{
    (void)pos;
    (void)u;
    if (len < 0)
        len = (gint)strlen(text);
    for (gint i = 0; i < len; i++)
        if (!g_ascii_isdigit(text[i])) {
            g_signal_stop_emission_by_name(e, "insert-text");
            return;
        }
}

static GtkWidget *num_entry(int width_chars)
{
    GtkWidget *e = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e), width_chars);
    gtk_entry_set_max_length(GTK_ENTRY(e), 9);
    gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_DIGITS);
    g_signal_connect(e, "insert-text", G_CALLBACK(digits_only), NULL);
    return e;
}

static int entry_int(GtkWidget *e, int def)
{
    const char *t = gtk_entry_get_text(GTK_ENTRY(e));
    return (t && *t) ? atoi(t) : def;
}

static void entry_set_int(GtkWidget *e, int v, gboolean empty_if_not_positive)
{
    char buf[32] = "";
    if (!(empty_if_not_positive && v <= 0))
        snprintf(buf, sizeof buf, "%d", v);
    gtk_entry_set_text(GTK_ENTRY(e), buf);
}

static GtkWidget *framed(const char *title, GtkWidget *child)
{
    GtkWidget *f = gtk_frame_new(title);
    gtk_widget_set_margin_start(child, 10);
    gtk_widget_set_margin_end(child, 10);
    gtk_widget_set_margin_top(child, 6);
    gtk_widget_set_margin_bottom(child, 10);
    gtk_container_add(GTK_CONTAINER(f), child);
    return f;
}

static GtkWidget *dim_label(const char *text)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "dim-label");
    return l;
}

static void set_status(const char *text)
{
    /* avoid a relayout when nothing changed */
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(W.status)), text) != 0)
        gtk_label_set_text(GTK_LABEL(W.status), text);
}

/* ------------------------------------------------------------------ */
/* markers                                                            */
/* ------------------------------------------------------------------ */

static void markers_update(void)
{
    for (int i = STEP_P1; i <= STEP_P3; i++)
        ov_markers_set(i - STEP_P1, cfg.step[i].x, cfg.step[i].y, cfg.step[i].enabled);
    for (int i = 0; i < NFRAMES; i++)
        ov_frames_set(i, &cfg.frame[i], cfg.step[STEP_R1 + i].enabled);
}

/* pattern thumbnail and frame descriptions */
static void patterns_update(void)
{
    char *path = config_pattern_path();
    GdkPixbuf *pb = cfg.have_pattern ? gdk_pixbuf_new_from_file_at_scale(path, 40, 40, TRUE, NULL) : NULL;
    g_free(path);
    if (pb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(W.pat_img), pb);
        g_object_unref(pb);
    } else
        gtk_image_set_from_icon_name(GTK_IMAGE(W.pat_img), "image-missing", GTK_ICON_SIZE_DND);
    char t[200];
    if (cfg.have_pattern)
        snprintf(t, sizeof t, "Pattern %d px, taken at %d, %d. Threshold %.2f.", cfg.pat_size, cfg.pat_x, cfg.pat_y,
                 cfg.threshold_pct / 100.0);
    else
        snprintf(t, sizeof t, "No pattern yet. Take a screenshot, then select the pattern and draw the frames.");
    gtk_label_set_text(GTK_LABEL(W.pat_label), t);
    gtk_widget_set_sensitive(W.edit_btn, W.last_shot.rgb != NULL);
    for (int i = 0; i < NFRAMES; i++) {
        const FrameCfg *f = &cfg.frame[i];
        if (f->r.w > 0 && f->nzones > 0)
            snprintf(t, sizeof t, "frame %d x %d at %d, %d, %d exclusion zone%s", f->r.w, f->r.h, f->r.x, f->r.y,
                     f->nzones, f->nzones == 1 ? "" : "s");
        else if (f->r.w > 0)
            snprintf(t, sizeof t, "frame %d x %d at %d, %d", f->r.w, f->r.h, f->r.x, f->r.y);
        else
            snprintf(t, sizeof t, "no frame yet");
        gtk_label_set_text(GTK_LABEL(W.row[STEP_R1 + i].finfo), t);
    }
    markers_update();
}

static void markers_visibility(void)
{
    gboolean active = W.win && gtk_window_is_active(GTK_WINDOW(W.win));
    ov_markers_show(active || W.pick_step > 0);
}

static void on_active_changed(GObject *o, GParamSpec *p, gpointer u)
{
    (void)o;
    (void)p;
    (void)u;
    markers_visibility();
}

/* ------------------------------------------------------------------ */
/* reading widgets into cfg                                           */
/* ------------------------------------------------------------------ */

static void read_all(void)
{
    if (W.loading)
        return;
    for (int i = 0; i < NSTEPS; i++) {
        Row *r = &W.row[i];
        StepCfg *s = &cfg.step[i];
        s->enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(r->chk));
        s->interval_ms = CLAMP(entry_int(r->iv, s->interval_ms), MIN_INTERVAL_MS, MAX_INTERVAL_MS);
        s->button = atoi(gtk_combo_box_get_active_id(GTK_COMBO_BOX(r->btn)) ?: "1");
        s->click_type = gtk_combo_box_get_active(GTK_COMBO_BOX(r->type)) == 1 ? CLICK_DOUBLE : CLICK_SINGLE;
        if (r->x) {
            s->x = entry_int(r->x, -1);
            s->y = entry_int(r->y, -1);
        }
        s->hold_ms = CLAMP(entry_int(r->hold, 0), 0, 10000);
        s->hold_random = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(r->rnd));
        gtk_widget_set_sensitive(r->rnd, s->hold_ms > 0);
    }
    cfg.hotkey_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(W.hotkey_mode)) == 1 ? HOTKEY_HOLD : HOTKEY_NORMAL;
    cfg.use_stop_after = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W.chk_stop));
    cfg.stop_after_clicks = MAX(1, entry_int(W.e_stop, 1));
    cfg.use_random = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W.chk_random));
    cfg.random_ms = CLAMP(entry_int(W.e_random, 0), 0, 60000);
    gtk_widget_set_sensitive(W.e_stop, cfg.use_stop_after);
    gtk_widget_set_sensitive(W.e_random, cfg.use_random);
    config_save();
    markers_update();
}

static void on_changed(GtkWidget *w, gpointer u)
{
    (void)w;
    (void)u;
    read_all();
}

void mw_reload(void)
{
    W.loading = TRUE;
    for (int i = 0; i < NSTEPS; i++) {
        Row *r = &W.row[i];
        StepCfg *s = &cfg.step[i];
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->chk), s->enabled);
        entry_set_int(r->iv, s->interval_ms, FALSE);
        char id[4];
        snprintf(id, sizeof id, "%d", s->button);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(r->btn), id);
        gtk_combo_box_set_active(GTK_COMBO_BOX(r->type), s->click_type);
        if (r->x) {
            entry_set_int(r->x, s->x, FALSE);
            entry_set_int(r->y, s->y, FALSE);
            if (s->x < 0)
                gtk_entry_set_text(GTK_ENTRY(r->x), "");
            if (s->y < 0)
                gtk_entry_set_text(GTK_ENTRY(r->y), "");
        }
        entry_set_int(r->hold, s->hold_ms, TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->rnd), s->hold_random);
        gtk_widget_set_sensitive(r->rnd, s->hold_ms > 0);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(W.hotkey_mode), cfg.hotkey_mode);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W.chk_stop), cfg.use_stop_after);
    entry_set_int(W.e_stop, cfg.stop_after_clicks, FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W.chk_random), cfg.use_random);
    entry_set_int(W.e_random, cfg.random_ms, TRUE);
    gtk_widget_set_sensitive(W.e_stop, cfg.use_stop_after);
    gtk_widget_set_sensitive(W.e_random, cfg.use_random);
    W.loading = FALSE;
    patterns_update();
}

/* ------------------------------------------------------------------ */
/* start / stop                                                       */
/* ------------------------------------------------------------------ */

static void buttons_update(void)
{
    gboolean run = seq_running();
    gtk_widget_set_sensitive(W.start, !run && W.pick_step == 0);
    gtk_widget_set_sensitive(W.stop, run);
}

static gboolean status_tick(gpointer u)
{
    (void)u;
    SeqStatus s;
    seq_status(&s);
    if (!s.running) {
        W.status_timer = 0;
        return G_SOURCE_REMOVE;
    }
    char react[300] = "";
    gboolean any_r = FALSE;
    for (int i = 0; i < NFRAMES; i++)
        if (cfg.step[STEP_R1 + i].enabled && seq_running())
            any_r = TRUE;
    if (any_r) {
        size_t n = 0;
        n += (size_t)snprintf(react + n, sizeof react - n, "\nSimilarity now:");
        for (int i = 0; i < NFRAMES && n < sizeof react; i++)
            if (s.r_checks[i])
                n += (size_t)snprintf(react + n, sizeof react - n, "  R%d %.2f (%d ms)", i + 1, s.r_best[i], s.r_ms[i]);
        if (s.lr_step >= 0 && n < sizeof react)
            snprintf(react + n, sizeof react - n, "\nLast reaction %s: %s at %d, %d (%.2f)", s.lr_time,
                     step_name(s.lr_step), s.lr_x, s.lr_y, s.lr_score);
    }
    char buf[800];
    snprintf(buf, sizeof buf, "Running: %" G_GINT64_FORMAT " clicks (M %" G_GINT64_FORMAT ", P1 %" G_GINT64_FORMAT
             ", P2 %" G_GINT64_FORMAT ", P3 %" G_GINT64_FORMAT ", R %" G_GINT64_FORMAT "), last: %s%s%s%s%s",
             s.clicks, s.line_clicks[STEP_M], s.line_clicks[STEP_P1], s.line_clicks[STEP_P2], s.line_clicks[STEP_P3],
             s.line_clicks[STEP_R1] + s.line_clicks[STEP_R2] + s.line_clicks[STEP_R3],
             s.step >= 0 ? step_name(s.step) : "-", react,
             s.manual ? "\nWaiting: you are clicking." : (s.m_paused || s.p_paused) ? "\nWaiting while the mouse moves." : "",
             s.missed ? "\nSome clicks found no window at their position." :
             s.unshielded ? "\nSome position clicks could not hold the mouse off the game." : "",
             W.note);
    set_status(buf);
    return G_SOURCE_CONTINUE;
}

static void on_seq_finished(void)
{
    SeqStatus s;
    seq_status(&s);
    char buf[300];
    snprintf(buf, sizeof buf, "Stopped after %" G_GINT64_FORMAT " clicks.", s.clicks);
    set_status(buf);
    buttons_update();
}

static void do_start(void)
{
    if (seq_running() || W.pick_step)
        return;
    read_all();
    char *err = NULL;
    if (!seq_start(&cfg, on_seq_finished, &err)) {
        set_status(err ? err : "Cannot start.");
        g_free(err);
        return;
    }
    W.note[0] = 0;
    int min_ms = config_min_interval_ms(&cfg);
    for (int i = STEP_M; i <= STEP_P3; i++)
        if (cfg.step[i].enabled && cfg.step[i].interval_ms < min_ms) {
            snprintf(W.note, sizeof W.note, "\nIntervals below %d ms run with %d ms (safe mode).", min_ms, min_ms);
            break;
        }
    buttons_update();
    status_tick(NULL);
    if (!W.status_timer)
        W.status_timer = g_timeout_add(250, status_tick, NULL);
}

static void do_stop(void)
{
    seq_stop();
}

static void on_start(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    do_start();
}

static void on_stop(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    do_stop();
}

static void on_hotkey(gboolean pressed)
{
    if (W.capturing)
        return;
    if (cfg.hotkey_mode == HOTKEY_HOLD) {
        if (pressed)
            do_start();
        else
            do_stop();
    } else if (pressed) {
        if (seq_running())
            do_stop();
        else
            do_start();
    }
}

void mw_set_capturing(gboolean on)
{
    W.capturing = on;
}

void mw_update_hotkey_labels(void)
{
    Display *d = ov_display();
    char keys[96];
    const char *main = d ? xa_key_name(d, cfg.key_main) : "F8";
    if (cfg.key_mod >= 0 && d)
        snprintf(keys, sizeof keys, "%s+%s", xa_key_name(d, cfg.key_mod), main);
    else
        snprintf(keys, sizeof keys, "%s", main);
    char s[128];
    snprintf(s, sizeof s, "Start (%s)", keys);
    gtk_button_set_label(GTK_BUTTON(W.start), s);
    snprintf(s, sizeof s, "Stop (%s)", keys);
    gtk_button_set_label(GTK_BUTTON(W.stop), s);
}

/* ------------------------------------------------------------------ */
/* position picking                                                   */
/* ------------------------------------------------------------------ */

static gboolean pick_tick(gpointer u)
{
    (void)u;
    Display *d = ov_display();
    int x, y;
    if (!W.pick_step || !d) {
        W.pick_timer = 0;
        return G_SOURCE_REMOVE;
    }
    if (xa_pointer(d, &x, &y)) {
        Row *r = &W.row[W.pick_step];
        W.loading = TRUE;
        entry_set_int(r->x, x, FALSE);
        entry_set_int(r->y, y, FALSE);
        W.loading = FALSE;
        ov_pick(W.pick_step - STEP_P1, x, y);
    }
    return G_SOURCE_CONTINUE;
}

static void pick_end(void)
{
    hk_set_picking(0);
    if (W.pick_timer)
        g_source_remove(W.pick_timer);
    W.pick_timer = 0;
    for (int i = STEP_P1; i <= STEP_P3; i++) {
        gtk_button_set_label(GTK_BUTTON(W.row[i].get), "Get");
        gtk_widget_set_sensitive(W.row[i].get, TRUE);
    }
    W.pick_step = 0;
    ov_pick(-1, 0, 0);
    ov_banner(NULL, NULL);
    markers_visibility();
    buttons_update();
}

static void on_pick_done(int step, int x, int y, gboolean ok)
{
    if (step != W.pick_step || step < STEP_P1 || step > STEP_P3)
        return;
    Row *r = &W.row[step];
    W.loading = TRUE;
    entry_set_int(r->x, ok ? x : W.pick_old_x, FALSE);
    entry_set_int(r->y, ok ? y : W.pick_old_y, FALSE);
    if (!ok && W.pick_old_x < 0)
        gtk_entry_set_text(GTK_ENTRY(r->x), "");
    if (!ok && W.pick_old_y < 0)
        gtk_entry_set_text(GTK_ENTRY(r->y), "");
    if (ok)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->chk), TRUE);
    W.loading = FALSE;
    pick_end();
    read_all();
    char buf[160];
    if (ok)
        snprintf(buf, sizeof buf, "%s set to %d, %d.", step_name(step), x, y);
    else
        snprintf(buf, sizeof buf, "%s: picking cancelled.", step_name(step));
    set_status(buf);
}

static void on_get(GtkButton *b, gpointer u)
{
    (void)b;
    int step = GPOINTER_TO_INT(u);
    if (seq_running()) {
        set_status("Stop the sequence before picking a position.");
        return;
    }
    if (W.pick_step)
        return;
    W.pick_step = step;
    W.pick_old_x = cfg.step[step].x;
    W.pick_old_y = cfg.step[step].y;
    for (int i = STEP_P1; i <= STEP_P3; i++)
        gtk_widget_set_sensitive(W.row[i].get, FALSE);
    gtk_button_set_label(GTK_BUTTON(W.row[step].get), "...");
    buttons_update();
    hk_set_picking(step);

    Display *d = ov_display();
    char title[96], text[200], keys[96];
    const char *main = d ? xa_key_name(d, cfg.key_main) : "F8";
    if (cfg.key_mod >= 0 && d)
        snprintf(keys, sizeof keys, "%s+%s", xa_key_name(d, cfg.key_mod), main);
    else
        snprintf(keys, sizeof keys, "%s", main);
    snprintf(title, sizeof title, "Picking %s", step_name(step));
    snprintf(text, sizeof text, "Point at the target and press %s.   Esc = cancel.", keys);
    ov_banner(title, text);
    markers_visibility();
    char st[200];
    snprintf(st, sizeof st, "Picking %s: point at the target and press %s (or click). Esc cancels.",
             step_name(step), keys);
    set_status(st);
    W.pick_timer = g_timeout_add(50, pick_tick, NULL);
}

/* ------------------------------------------------------------------ */
/* building the window                                                */
/* ------------------------------------------------------------------ */

static GtkWidget *combo(const char *const *items, const char *const *ids)
{
    GtkWidget *c = gtk_combo_box_text_new();
    for (int i = 0; items[i]; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c), ids ? ids[i] : NULL, items[i]);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), 0);
    return c;
}

/* normalize an interval entry when the user leaves it */
static gboolean on_iv_focus_out(GtkWidget *w, GdkEvent *e, gpointer u)
{
    (void)e;
    (void)u;
    int v = entry_int(w, -1);
    int lo = config_min_interval_ms(&cfg);
    int nv = v < 0 ? 1000 : CLAMP(v, lo, MAX_INTERVAL_MS);
    if (nv != v)
        entry_set_int(w, nv, FALSE);
    return FALSE;
}

static GtkWidget *build_steps(void)
{
    static const char *const buttons[] = {"Left", "Middle", "Right", NULL};
    static const char *const button_ids[] = {"1", "2", "3", NULL};
    static const char *const types[] = {"Single", "Double", NULL};
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g), 8);
    gtk_grid_set_row_spacing(GTK_GRID(g), 6);
    const char *heads[] = {"Line", "Interval (ms)", "Button", "Click", "X", "Y", "", "Hold (ms)", "Random"};
    for (int c = 0; c < 9; c++) {
        GtkWidget *l = dim_label(heads[c]);
        gtk_label_set_xalign(GTK_LABEL(l), c == 0 ? 0.0f : 0.5f);
        gtk_grid_attach(GTK_GRID(g), l, c, 0, 1, 1);
    }
    for (int i = 0; i < NSTEPS; i++) {
        Row *r = &W.row[i];
        int y = i + 1;
        r->chk = gtk_check_button_new_with_label(step_name(i));
        gtk_grid_attach(GTK_GRID(g), r->chk, 0, y, 1, 1);
        r->iv = num_entry(9);
        gtk_entry_set_max_length(GTK_ENTRY(r->iv), 8);
        gtk_widget_set_tooltip_text(r->iv, "How often this line clicks: 50 to 60000000 ms "
                                           "(safe mode: at least 100 ms).");
        g_signal_connect(r->iv, "focus-out-event", G_CALLBACK(on_iv_focus_out), NULL);
        g_signal_connect(r->iv, "changed", G_CALLBACK(on_changed), NULL);
        gtk_grid_attach(GTK_GRID(g), r->iv, 1, y, 1, 1);
        r->btn = combo(buttons, button_ids);
        r->type = combo(types, NULL);
        gtk_grid_attach(GTK_GRID(g), r->btn, 2, y, 1, 1);
        gtk_grid_attach(GTK_GRID(g), r->type, 3, y, 1, 1);
        if (i == STEP_M) {
            GtkWidget *l = dim_label("follows the mouse, waits while it moves");
            gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
            gtk_grid_attach(GTK_GRID(g), l, 4, y, 3, 1);
        } else if (i >= STEP_R1) {
            r->finfo = dim_label("");
            gtk_label_set_xalign(GTK_LABEL(r->finfo), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(r->finfo), PANGO_ELLIPSIZE_END);
            gtk_label_set_width_chars(GTK_LABEL(r->finfo), 30);
            gtk_widget_set_tooltip_text(r->chk, "Checks its frame in its interval and clicks the pattern when it "
                                                "newly appears.");
            gtk_grid_attach(GTK_GRID(g), r->finfo, 4, y, 3, 1);
        } else {
            r->x = num_entry(6);
            r->y = num_entry(6);
            r->get = gtk_button_new_with_label("Get");
            gtk_widget_set_tooltip_text(r->get, "Pick the position on the screen: point at it and press the start/stop hotkey.");
            gtk_grid_attach(GTK_GRID(g), r->x, 4, y, 1, 1);
            gtk_grid_attach(GTK_GRID(g), r->y, 5, y, 1, 1);
            gtk_grid_attach(GTK_GRID(g), r->get, 6, y, 1, 1);
            g_signal_connect(r->x, "changed", G_CALLBACK(on_changed), NULL);
            g_signal_connect(r->y, "changed", G_CALLBACK(on_changed), NULL);
            g_signal_connect(r->get, "clicked", G_CALLBACK(on_get), GINT_TO_POINTER(i));
        }
        r->hold = num_entry(6);
        gtk_entry_set_placeholder_text(GTK_ENTRY(r->hold), "auto");
        gtk_widget_set_tooltip_text(r->hold, "How long the button stays pressed. Empty = 50 ms.");
        r->rnd = gtk_check_button_new();
        gtk_widget_set_halign(r->rnd, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(r->rnd, "Random hold time between 20 ms and the value on the left.");
        gtk_grid_attach(GTK_GRID(g), r->hold, 7, y, 1, 1);
        gtk_grid_attach(GTK_GRID(g), r->rnd, 8, y, 1, 1);
        g_signal_connect(r->chk, "toggled", G_CALLBACK(on_changed), NULL);
        g_signal_connect(r->btn, "changed", G_CALLBACK(on_changed), NULL);
        g_signal_connect(r->type, "changed", G_CALLBACK(on_changed), NULL);
        g_signal_connect(r->hold, "changed", G_CALLBACK(on_changed), NULL);
        g_signal_connect(r->rnd, "toggled", G_CALLBACK(on_changed), NULL);
    }
    return framed("Lines (each clicks in its own interval, one click at a time)", g);
}

/* ------------------------------------------------------------------ */
/* screenshot for the pattern and the frames                          */
/* ------------------------------------------------------------------ */

static void on_editor_saved(void)
{
    patterns_update();
    set_status("Pattern, frames and exclusion zones saved.");
}

static gboolean banner_off(gpointer u)
{
    (void)u;
    if (!W.shot_timer && W.pick_step == 0)
        ov_banner(NULL, NULL);
    return G_SOURCE_REMOVE;
}

static void take_shot(gboolean announce)
{
    Display *d = ov_display();
    int x, y;
    if (!d || !xa_pointer(d, &x, &y)) {
        set_status("Screenshot failed: the mouse position is unknown.");
        return;
    }
    ov_banner(NULL, NULL);
    ov_markers_show(FALSE);
    XSync(d, False);
    xa_sleep_ms(80);
    Shot s;
    if (!cap_window_at(d, x, y, &s)) {
        set_status("Screenshot failed: no X11 window under the mouse. Point at the game.");
        return;
    }
    shot_free(&W.last_shot);
    W.last_shot = s;
    char t[160];
    snprintf(t, sizeof t, "Screenshot taken: %d x %d px.", s.w, s.h);
    set_status(t);
    if (announce) {
        ov_banner("Screenshot taken", "Switch to AutoClicker Advanced to select the pattern and the frames.");
        g_timeout_add(3000, banner_off, NULL);
    }
    patterns_update();
    editor_open(GTK_WINDOW(W.win), &W.last_shot, on_editor_saved);
}

static gboolean shot_tick(gpointer u)
{
    (void)u;
    if (W.shot_left <= 0) {
        W.shot_timer = 0;
        take_shot(FALSE);
        return G_SOURCE_REMOVE;
    }
    char t[120];
    snprintf(t, sizeof t, "Screenshot in %d s", W.shot_left);
    ov_banner(t, "Switch to the game now and point at it with the mouse.");
    W.shot_left--;
    return G_SOURCE_CONTINUE;
}

static void on_shot_button(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    if (W.shot_timer || seq_running())
        return;
    W.shot_left = cfg.shot_countdown_s;
    if (W.shot_left <= 0) {
        take_shot(FALSE);
        return;
    }
    shot_tick(NULL);
    W.shot_timer = g_timeout_add(1000, shot_tick, NULL);
}

static void on_shot_hotkey(void)
{
    if (W.capturing || W.shot_timer || W.pick_step || editor_is_open())
        return;
    take_shot(TRUE);
}

static void on_edit_button(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    if (W.last_shot.rgb)
        editor_open(GTK_WINDOW(W.win), &W.last_shot, on_editor_saved);
}

static GtkWidget *build_patterns(void)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    W.pat_img = gtk_image_new();
    gtk_widget_set_size_request(W.pat_img, 44, 44);
    W.pat_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(W.pat_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(W.pat_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(W.pat_label), 50);
    W.shot_btn = gtk_button_new_with_label("Screenshot");
    W.edit_btn = gtk_button_new_with_label("Edit last screenshot");
    g_signal_connect(W.shot_btn, "clicked", G_CALLBACK(on_shot_button), NULL);
    g_signal_connect(W.edit_btn, "clicked", G_CALLBACK(on_edit_button), NULL);
    gtk_box_pack_start(GTK_BOX(b), W.pat_img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), W.pat_label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(b), W.edit_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(b), W.shot_btn, FALSE, FALSE, 0);
    return framed("Pattern reactions", b);
}

void mw_update_shot_label(void)
{
    if (!W.shot_btn)
        return;
    Display *d = ov_display();
    char t[80];
    snprintf(t, sizeof t, "Screenshot (%s or %d s)", d ? xa_key_name(d, cfg.shot_key) : "F9", cfg.shot_countdown_s);
    gtk_button_set_label(GTK_BUTTON(W.shot_btn), t);
    gtk_widget_set_tooltip_text(W.shot_btn, "Press the key while the game is in front, or click here and switch "
                                            "to the game within the countdown.");
    patterns_update();
}

static GtkWidget *build_options(void)
{
    static const char *const modes[] = {"Normal", "Hold", NULL};
    GtkWidget *g = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(g), 10);
    gtk_grid_set_row_spacing(GTK_GRID(g), 6);
    GtkWidget *l = gtk_label_new("Hotkey");
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    W.hotkey_mode = combo(modes, NULL);
    gtk_widget_set_tooltip_text(W.hotkey_mode, "Normal: press to start and again to stop. Hold: runs while the key is held.");
    W.chk_stop = gtk_check_button_new_with_label("Stop after");
    W.e_stop = num_entry(8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W.e_stop), "# of clicks");
    gtk_widget_set_tooltip_text(W.e_stop, "Stops after this number of clicks of all lines together.");
    W.chk_random = gtk_check_button_new_with_label("Random interval");
    W.e_random = num_entry(8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W.e_random), "+/- ms");
    gtk_widget_set_tooltip_text(W.e_random, "Each interval varies randomly by up to this value.");
    gtk_grid_attach(GTK_GRID(g), l, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), W.hotkey_mode, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), W.chk_stop, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), W.e_stop, 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), W.chk_random, 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(g), W.e_random, 5, 0, 1, 1);
    g_signal_connect(W.hotkey_mode, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(W.chk_stop, "toggled", G_CALLBACK(on_changed), NULL);
    g_signal_connect(W.e_stop, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(W.chk_random, "toggled", G_CALLBACK(on_changed), NULL);
    g_signal_connect(W.e_random, "changed", G_CALLBACK(on_changed), NULL);
    return framed("Options", g);
}

static void on_settings(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    settings_run(GTK_WINDOW(W.win));
}

static void on_destroy(GtkWidget *w, gpointer u)
{
    (void)w;
    (void)u;
    seq_stop();
    ov_markers_show(FALSE);
    ov_banner(NULL, NULL);
    if (W.shot_timer)
        g_source_remove(W.shot_timer);
    W.shot_timer = 0;
    shot_free(&W.last_shot);
    W.win = NULL;
}

GtkWidget *mw_create(GtkApplication *app)
{
    memset(&W, 0, sizeof W);
    W.win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(W.win), XCA_NAME);
    gtk_window_set_resizable(GTK_WINDOW(W.win), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_box_pack_start(GTK_BOX(box), build_steps(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), build_patterns(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), build_options(), FALSE, FALSE, 0);

    W.status = gtk_label_new("Stopped.");
    gtk_label_set_xalign(GTK_LABEL(W.status), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(W.status), TRUE);
    gtk_label_set_lines(GTK_LABEL(W.status), 5);
    gtk_widget_set_size_request(W.status, -1, 90);
    gtk_box_pack_start(GTK_BOX(box), framed("Status", W.status), FALSE, FALSE, 0);

    GtkWidget *bb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(bb), TRUE);
    W.start = gtk_button_new_with_label("Start (F8)");
    W.stop = gtk_button_new_with_label("Stop (F8)");
    W.settings = gtk_button_new_with_label("Settings");
    gtk_widget_set_size_request(W.start, -1, 54);
    gtk_box_pack_start(GTK_BOX(bb), W.start, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bb), W.stop, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bb), W.settings, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), bb, FALSE, FALSE, 0);
    g_signal_connect(W.start, "clicked", G_CALLBACK(on_start), NULL);
    g_signal_connect(W.stop, "clicked", G_CALLBACK(on_stop), NULL);
    g_signal_connect(W.settings, "clicked", G_CALLBACK(on_settings), NULL);

    gtk_container_add(GTK_CONTAINER(W.win), box);
    g_signal_connect(W.win, "notify::is-active", G_CALLBACK(on_active_changed), NULL);
    g_signal_connect(W.win, "destroy", G_CALLBACK(on_destroy), NULL);

    mw_reload();
    mw_update_hotkey_labels();
    mw_update_shot_label();
    buttons_update();

    static const HotkeyCallbacks cbs = {on_hotkey, on_pick_done, settings_on_capture, on_shot_hotkey};
    hk_start(&cbs);
    gtk_widget_show_all(W.win);
    return W.win;
}
