/*
 * AutoClicker Advanced - configuration
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: settings stored per line in a GKeyFile; every line has
 * its own interval. Files of version 0.1.x are migrated (their common step
 * duration becomes the interval of every line).
 * Modified 2026-11-05: program renamed; the settings folder of version 0.3.0
 * and earlier is taken over once.
 */
#include "config.h"

#include <glib/gstdio.h>
#include <string.h>

#include "version.h"

#define CONFIG_DIR "autoclicker-advanced"

/* Settings folder, newly allocated. Up to version 0.3.0 it was
 * ~/.config/xclicker-advanced; on the first call it is renamed to the new
 * folder if the new one does not exist yet. */
static char *config_dir(void)
{
    static gboolean checked;
    char *dir = g_build_filename(g_get_user_config_dir(), CONFIG_DIR, NULL);
    if (checked)
        return dir;
    checked = TRUE;
    char *old = g_build_filename(g_get_user_config_dir(), XCA_OLD_DIR, NULL);
    if (!g_file_test(dir, G_FILE_TEST_EXISTS) && g_file_test(old, G_FILE_TEST_IS_DIR)) {
        if (g_rename(old, dir) == 0) {
            char *oc = g_build_filename(dir, XCA_OLD_DIR ".conf", NULL);
            char *nc = g_build_filename(dir, CONFIG_DIR ".conf", NULL);
            if (g_file_test(oc, G_FILE_TEST_EXISTS) && !g_file_test(nc, G_FILE_TEST_EXISTS))
                g_rename(oc, nc);
            g_free(oc);
            g_free(nc);
        } else {
            g_printerr("autoclicker-advanced: cannot take over the settings from %s\n", old);
        }
    }
    g_free(old);
    return dir;
}

Config cfg;

static GKeyFile *kf;
static char *path;

static const char *step_group[NSTEPS] = {"Step.Mouse",     "Step.Position1", "Step.Position2", "Step.Position3",
                                         "Step.Reaction1", "Step.Reaction2", "Step.Reaction3"};
static const char *step_names[NSTEPS] = {"Mouse position", "Position 1", "Position 2", "Position 3",
                                         "Reaction 1",     "Reaction 2", "Reaction 3"};

const char *step_name(int i)
{
    return (i >= 0 && i < NSTEPS) ? step_names[i] : "?";
}

const char *config_file_path(void)
{
    return path ? path : "";
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int get_int(const char *g, const char *k, int def)
{
    GError *e = NULL;
    int v = g_key_file_get_integer(kf, g, k, &e);
    if (e) {
        g_error_free(e);
        return def;
    }
    return v;
}

static gboolean get_bool(const char *g, const char *k, gboolean def)
{
    GError *e = NULL;
    gboolean v = g_key_file_get_boolean(kf, g, k, &e);
    if (e) {
        g_error_free(e);
        return def;
    }
    return v;
}

char *config_pattern_path(void)
{
    char *dir = config_dir();
    char *p = g_build_filename(dir, "pattern.png", NULL);
    g_free(dir);
    return p;
}

gboolean rect_overlap(const Rect *a, const Rect *b)
{
    if (a->w <= 0 || b->w <= 0)
        return FALSE;
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

gboolean rect_contains(const Rect *r, int x, int y)
{
    return r->w > 0 && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

static void defaults_presets(Config *c)
{
    c->hotkey_mode = HOTKEY_NORMAL;
    c->use_stop_after = FALSE;
    c->stop_after_clicks = 100;
    c->use_random = FALSE;
    c->random_ms = 0;
    for (int i = 0; i < NSTEPS; i++) {
        StepCfg *s = &c->step[i];
        s->enabled = (i == STEP_M);
        s->interval_ms = (i == STEP_M) ? 100 : (i >= STEP_R1) ? 200 : 1000;
        s->button = 1;
        s->click_type = CLICK_SINGLE;
        s->x = s->y = -1;
        s->hold_ms = 0;
        s->hold_random = FALSE;
    }
}

static void defaults_settings(Config *c)
{
    memset(c->frame, 0, sizeof c->frame);
    c->have_pattern = FALSE;
    c->pat_size = 0;
    c->pat_x = c->pat_y = -1;
    c->threshold_pct = 85;
    c->downscale = 2;
    c->core_pct = 80;
    c->color_tol = 25;
    c->lock_radius = 30;
    c->lock_ms = 1000;
    c->ignore_existing = TRUE;
    c->shot_key = 0;
    c->shot_countdown_s = 5;
    c->safe_mode = TRUE;
    c->key_mod = -1;
    c->key_main = 0;
    c->double_gap_ms = 150;
    c->motion_threshold_px = 30;
    c->motion_rest_ms = 300;
    c->pause_positions = TRUE;
    c->settle_ms = 40;
    c->shield_pointer = TRUE;
    c->manual_priority = TRUE;
    c->manual_pause_ms = 500;
    c->start_menu_entry = TRUE;
}

void config_load(void)
{
    if (!path) {
        char *dir = config_dir();
        g_mkdir_with_parents(dir, 0700);
        path = g_build_filename(dir, CONFIG_DIR ".conf", NULL);
        g_free(dir);
    }
    if (kf)
        g_key_file_free(kf);
    kf = g_key_file_new();
    if (g_file_test(path, G_FILE_TEST_EXISTS) && !g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL))
        g_printerr("autoclicker-advanced: the config file seems to be corrupted, using defaults\n");

    Config *c = &cfg;
    defaults_presets(c);
    defaults_settings(c);

    c->safe_mode = get_bool("Options", "SafeMode", c->safe_mode);
    c->key_mod = get_int("Options", "HotkeyModifier", c->key_mod);
    c->key_main = get_int("Options", "HotkeyKey", c->key_main);
    c->double_gap_ms = clampi(get_int("Options", "DoubleClickGapMs", c->double_gap_ms), 20, 500);
    c->motion_threshold_px = clampi(get_int("Options", "MotionThresholdPx", c->motion_threshold_px), 1, 100);
    c->motion_rest_ms = clampi(get_int("Options", "MotionRestMs", c->motion_rest_ms), 50, 5000);
    c->pause_positions = get_bool("Options", "PausePositionsWhileMoving", c->pause_positions);
    c->settle_ms = clampi(get_int("Options", "EventWaitMs", c->settle_ms), 0, 200);
    /* config version 1 (0.1.x) used 30 ms as default wait */
    if (get_int("Options", "ConfigVersion", 1) < 2 && c->settle_ms == 30)
        c->settle_ms = 40;
    c->shield_pointer = get_bool("Options", "ShieldPointer", c->shield_pointer);
    c->manual_priority = get_bool("Options", "WaitWhileUserClicks", c->manual_priority);
    c->manual_pause_ms = clampi(get_int("Options", "WaitAfterUserClickMs", c->manual_pause_ms), 0, 5000);
    c->start_menu_entry = get_bool("Options", "StartMenuEntry", c->start_menu_entry);

    /* version 0.1.x: one step duration for all lines */
    gint64 old_ms = -1;
    if (g_key_file_has_group(kf, "Interval"))
        old_ms = (gint64)get_int("Interval", "Hours", 0) * 3600000 + (gint64)get_int("Interval", "Minutes", 0) * 60000 +
                 (gint64)get_int("Interval", "Seconds", 0) * 1000 + get_int("Interval", "Milliseconds", 0);

    c->hotkey_mode = clampi(get_int("General", "HotkeyMode", c->hotkey_mode), 0, 1);
    c->use_stop_after = get_bool("General", "UseStopAfter", c->use_stop_after);
    c->stop_after_clicks = clampi(get_int("General", "StopAfterClicks", c->stop_after_clicks), 1, 100000000);
    c->use_random = get_bool("General", "UseRandomInterval", c->use_random);
    c->random_ms = clampi(get_int("General", "RandomIntervalMs", c->random_ms), 0, 60000);

    for (int i = 0; i < NSTEPS; i++) {
        StepCfg *s = &c->step[i];
        const char *g = step_group[i];
        if (old_ms > 0)
            s->interval_ms = (int)CLAMP(old_ms, MIN_INTERVAL_MS, MAX_INTERVAL_MS);
        s->enabled = get_bool(g, "Enabled", s->enabled);
        s->interval_ms = clampi(get_int(g, "IntervalMs", s->interval_ms), MIN_INTERVAL_MS, MAX_INTERVAL_MS);
        s->button = clampi(get_int(g, "Button", s->button), 1, 3);
        s->click_type = clampi(get_int(g, "ClickType", s->click_type), 0, 1);
        s->x = get_int(g, "X", s->x);
        s->y = get_int(g, "Y", s->y);
        s->hold_ms = clampi(get_int(g, "HoldMs", s->hold_ms), 0, 10000);
        s->hold_random = get_bool(g, "HoldRandom", s->hold_random);
    }
    c->threshold_pct = clampi(get_int("Reactions", "ThresholdPct", c->threshold_pct), 50, 99);
    c->downscale = get_int("Reactions", "Downscale", c->downscale);
    if (c->downscale != 1 && c->downscale != 2 && c->downscale != 4)
        c->downscale = 2;
    c->core_pct = clampi(get_int("Reactions", "CorePct", c->core_pct), 50, 100);
    c->color_tol = clampi(get_int("Reactions", "ColorTolerance", c->color_tol), 0, 100);
    c->lock_radius = clampi(get_int("Reactions", "LockRadiusPx", c->lock_radius), 0, 200);
    c->lock_ms = clampi(get_int("Reactions", "LockMs", c->lock_ms), 0, 10000);
    c->ignore_existing = get_bool("Reactions", "IgnoreExisting", c->ignore_existing);
    c->shot_key = get_int("Reactions", "ScreenshotKey", c->shot_key);
    c->shot_countdown_s = clampi(get_int("Reactions", "ScreenshotCountdownS", c->shot_countdown_s), 0, 30);

    c->pat_size = clampi(get_int("Pattern", "Size", 0), 0, MAX_PATTERN);
    c->pat_x = get_int("Pattern", "X", -1);
    c->pat_y = get_int("Pattern", "Y", -1);
    char *pp = config_pattern_path();
    c->have_pattern = c->pat_size >= MIN_PATTERN && g_file_test(pp, G_FILE_TEST_EXISTS);
    g_free(pp);

    for (int i = 0; i < NFRAMES; i++) {
        char g[16];
        snprintf(g, sizeof g, "Frame%d", i + 1);
        FrameCfg *f = &c->frame[i];
        f->r.x = get_int(g, "X", 0);
        f->r.y = get_int(g, "Y", 0);
        f->r.w = clampi(get_int(g, "W", 0), 0, 100000);
        f->r.h = clampi(get_int(g, "H", 0), 0, 100000);
        if (f->r.h <= 0)
            f->r.w = 0;
        f->nzones = 0;
        for (int z = 0; z < MAX_ZONES; z++) {
            char k[16];
            snprintf(k, sizeof k, "Zone%d", z + 1);
            gsize n = 0;
            gint *v = g_key_file_get_integer_list(kf, g, k, &n, NULL);
            if (v && n == 4 && v[2] > 0 && v[3] > 0)
                f->zone[f->nzones++] = (Rect){v[0], v[1], v[2], v[3]};
            g_free(v);
        }
    }
}

void config_save(void)
{
    if (!kf || !path)
        return;
    Config *c = &cfg;
    g_key_file_set_boolean(kf, "Options", "SafeMode", c->safe_mode);
    g_key_file_set_integer(kf, "Options", "HotkeyModifier", c->key_mod);
    g_key_file_set_integer(kf, "Options", "HotkeyKey", c->key_main);
    g_key_file_set_integer(kf, "Options", "DoubleClickGapMs", c->double_gap_ms);
    g_key_file_set_integer(kf, "Options", "MotionThresholdPx", c->motion_threshold_px);
    g_key_file_set_integer(kf, "Options", "MotionRestMs", c->motion_rest_ms);
    g_key_file_set_boolean(kf, "Options", "PausePositionsWhileMoving", c->pause_positions);
    g_key_file_set_integer(kf, "Options", "EventWaitMs", c->settle_ms);
    g_key_file_set_integer(kf, "Options", "ConfigVersion", 2);
    g_key_file_set_boolean(kf, "Options", "ShieldPointer", c->shield_pointer);
    g_key_file_set_boolean(kf, "Options", "WaitWhileUserClicks", c->manual_priority);
    g_key_file_set_integer(kf, "Options", "WaitAfterUserClickMs", c->manual_pause_ms);
    g_key_file_set_boolean(kf, "Options", "StartMenuEntry", c->start_menu_entry);

    /* keys of version 0.1.x */
    g_key_file_remove_group(kf, "Interval", NULL);
    g_key_file_remove_key(kf, "General", "UseRepeat", NULL);
    g_key_file_remove_key(kf, "General", "RepeatCycles", NULL);

    g_key_file_set_integer(kf, "General", "HotkeyMode", c->hotkey_mode);
    g_key_file_set_boolean(kf, "General", "UseStopAfter", c->use_stop_after);
    g_key_file_set_integer(kf, "General", "StopAfterClicks", c->stop_after_clicks);
    g_key_file_set_boolean(kf, "General", "UseRandomInterval", c->use_random);
    g_key_file_set_integer(kf, "General", "RandomIntervalMs", c->random_ms);

    for (int i = 0; i < NSTEPS; i++) {
        StepCfg *s = &c->step[i];
        const char *g = step_group[i];
        g_key_file_set_boolean(kf, g, "Enabled", s->enabled);
        g_key_file_set_integer(kf, g, "IntervalMs", s->interval_ms);
        g_key_file_set_integer(kf, g, "Button", s->button);
        g_key_file_set_integer(kf, g, "ClickType", s->click_type);
        g_key_file_set_integer(kf, g, "X", s->x);
        g_key_file_set_integer(kf, g, "Y", s->y);
        g_key_file_set_integer(kf, g, "HoldMs", s->hold_ms);
        g_key_file_set_boolean(kf, g, "HoldRandom", s->hold_random);
    }
    g_key_file_set_integer(kf, "Reactions", "ThresholdPct", c->threshold_pct);
    g_key_file_set_integer(kf, "Reactions", "Downscale", c->downscale);
    g_key_file_set_integer(kf, "Reactions", "CorePct", c->core_pct);
    g_key_file_set_integer(kf, "Reactions", "ColorTolerance", c->color_tol);
    g_key_file_set_integer(kf, "Reactions", "LockRadiusPx", c->lock_radius);
    g_key_file_set_integer(kf, "Reactions", "LockMs", c->lock_ms);
    g_key_file_set_boolean(kf, "Reactions", "IgnoreExisting", c->ignore_existing);
    g_key_file_set_integer(kf, "Reactions", "ScreenshotKey", c->shot_key);
    g_key_file_set_integer(kf, "Reactions", "ScreenshotCountdownS", c->shot_countdown_s);
    g_key_file_set_integer(kf, "Pattern", "Size", c->have_pattern ? c->pat_size : 0);
    g_key_file_set_integer(kf, "Pattern", "X", c->pat_x);
    g_key_file_set_integer(kf, "Pattern", "Y", c->pat_y);
    for (int i = 0; i < NFRAMES; i++) {
        char g[16];
        snprintf(g, sizeof g, "Frame%d", i + 1);
        g_key_file_remove_group(kf, g, NULL);
        const FrameCfg *f = &c->frame[i];
        if (f->r.w <= 0)
            continue;
        g_key_file_set_integer(kf, g, "X", f->r.x);
        g_key_file_set_integer(kf, g, "Y", f->r.y);
        g_key_file_set_integer(kf, g, "W", f->r.w);
        g_key_file_set_integer(kf, g, "H", f->r.h);
        for (int z = 0; z < f->nzones; z++) {
            char k[16];
            snprintf(k, sizeof k, "Zone%d", z + 1);
            gint v[4] = {f->zone[z].x, f->zone[z].y, f->zone[z].w, f->zone[z].h};
            g_key_file_set_integer_list(kf, g, k, v, 4);
        }
    }
    GError *e = NULL;
    if (!g_key_file_save_to_file(kf, path, &e)) {
        g_printerr("autoclicker-advanced: cannot save %s: %s\n", path, e ? e->message : "?");
        if (e)
            g_error_free(e);
    }
}

void config_reset_presets(void)
{
    defaults_presets(&cfg);
    config_save();
}

int config_min_interval_ms(const Config *c)
{
    return c->safe_mode ? MIN_INTERVAL_SAFE_MS : MIN_INTERVAL_MS;
}

int config_line_interval(const Config *c, int i)
{
    return clampi(c->step[i].interval_ms, config_min_interval_ms(c), MAX_INTERVAL_MS);
}
