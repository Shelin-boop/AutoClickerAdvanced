/*
 * AutoClicker Advanced - configuration
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: settings stored per line (step) in a GKeyFile; every
 * line has its own interval.
 */
#ifndef XCA_CONFIG_H
#define XCA_CONFIG_H

#include <glib.h>

enum { STEP_M = 0, STEP_P1, STEP_P2, STEP_P3, STEP_R1, STEP_R2, STEP_R3, NSTEPS };
enum { CLICK_SINGLE = 0, CLICK_DOUBLE = 1 };
enum { HOTKEY_NORMAL = 0, HOTKEY_HOLD = 1 };

#define MIN_INTERVAL_SAFE_MS 100
#define MIN_INTERVAL_MS 50
#define MAX_INTERVAL_MS 60000000
#define DEFAULT_PRESS_MS 50
#define MIN_PRESS_MS 20

#define NFRAMES 3
#define MAX_ZONES 8
#define MIN_PATTERN 8
#define MAX_PATTERN 200

typedef struct {
    int x, y, w, h; /* screen pixels; w == 0: not set */
} Rect;

typedef struct {
    Rect r;
    int nzones;
    Rect zone[MAX_ZONES]; /* exclusion zones inside the frame */
} FrameCfg;

typedef struct {
    gboolean enabled;
    int interval_ms; /* own interval of this line */
    int button;      /* 1 left, 2 middle, 3 right */
    int click_type;  /* CLICK_SINGLE / CLICK_DOUBLE */
    int x, y;        /* screen position for P lines, -1 = not set */
    int hold_ms;     /* 0 = default press time */
    gboolean hold_random;
} StepCfg;

typedef struct {
    /* options */
    int hotkey_mode;
    gboolean use_stop_after;
    int stop_after_clicks;
    gboolean use_random;
    int random_ms;
    StepCfg step[NSTEPS];
    /* pattern reactions */
    FrameCfg frame[NFRAMES];
    gboolean have_pattern;
    int pat_size;         /* side of the square in screen pixels */
    int pat_x, pat_y;     /* center where the pattern was taken (screen) */
    int threshold_pct;    /* similarity threshold 50..99 */
    int downscale;        /* 1, 2, 4 */
    int core_pct;         /* compared part of the radius 50..100 */
    int color_tol;        /* color tolerance 0..100 */
    int lock_radius;      /* px */
    int lock_ms;          /* lock time after a reaction click */
    gboolean ignore_existing;
    int shot_key;         /* keycode, 0 = F9 */
    int shot_countdown_s; /* countdown of the screenshot button */
    /* settings dialog */
    gboolean safe_mode;
    int key_mod;  /* keycode of an optional modifier, -1 = none */
    int key_main; /* keycode of the start/stop key, 0 = default F8 */
    int double_gap_ms;
    int motion_threshold_px;
    int motion_rest_ms;
    gboolean pause_positions; /* also pause P1..P3 while the mouse moves */
    int settle_ms;            /* wait between move, press/release and move back */
    gboolean shield_pointer;  /* keep real mouse events from the game during a position click */
    gboolean manual_priority; /* all lines wait while the user clicks */
    int manual_pause_ms;      /* ... and this long after the release */
    gboolean start_menu_entry;
} Config;

extern Config cfg;

void config_load(void);
void config_save(void);
/* resets the lines and options; keeps the settings dialog values */
void config_reset_presets(void);

/* smallest allowed interval (safe mode or not) */
int config_min_interval_ms(const Config *c);
/* interval of line i limited to the allowed range */
int config_line_interval(const Config *c, int i);

const char *step_name(int i);
const char *config_file_path(void);
/* path of the pattern image (PNG) in the configuration folder */
char *config_pattern_path(void);
gboolean rect_overlap(const Rect *a, const Rect *b);
gboolean rect_contains(const Rect *r, int x, int y);

#endif
