/*
 * AutoClicker Advanced - on-screen overlays (click-through X11 windows)
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Override-redirect windows with a 32-bit visual and an empty input shape:
 * they are drawn above the fullscreen game and every click passes through.
 */
#include "overlay.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <cairo-xlib.h>
#include <glib-unix.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "x11api.h"

typedef struct {
    Window w;
    int x, y, wd, ht;
    gboolean shown;
    int kind; /* 0 banner, 1 marker, 2 pick, 3 frame */
    int idx;
} Ov;

static Display *od;
static Visual *vis;
static int depth, SW, SH;
static Colormap cmap;
static Ov banner, marker[3], pick, frame_ov[NFRAMES];
static FrameCfg fr_cfg[NFRAMES];
static gboolean fr_on[NFRAMES];
/* frame colors: R1 green, R2 yellow, R3 blue */
static const double fcol[3][3] = {{0.25, 0.90, 0.35}, {1.0, 0.85, 0.15}, {0.35, 0.60, 1.0}};

static void draw_frame(cairo_t *cr, const Ov *o)
{
    const FrameCfg *f = &fr_cfg[o->idx];
    const double *c = fcol[o->idx % 3];
    double lw = SH / 270.0 + 2; /* about 10 px on 4K */
    /* exclusion zones: red, hatched */
    for (int z = 0; z < f->nzones; z++) {
        const Rect *r = &f->zone[z];
        double zx = r->x - o->x, zy = r->y - o->y;
        cairo_rectangle(cr, zx, zy, r->w, r->h);
        cairo_set_source_rgba(cr, 0.9, 0.1, 0.1, 0.22);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.95, 0.15, 0.15, 0.9);
        cairo_set_line_width(cr, lw * 0.6);
        cairo_stroke(cr);
        cairo_save(cr);
        cairo_rectangle(cr, zx, zy, r->w, r->h);
        cairo_clip(cr);
        cairo_set_line_width(cr, lw * 0.3);
        for (double t = -r->h; t < r->w; t += lw * 4) {
            cairo_move_to(cr, zx + t, zy + r->h);
            cairo_line_to(cr, zx + t + r->h, zy);
        }
        cairo_stroke(cr);
        cairo_restore(cr);
    }
    /* border with a dark outline */
    cairo_rectangle(cr, lw / 2, lw / 2, o->wd - lw, o->ht - lw);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_set_line_width(cr, lw * 1.6);
    cairo_stroke(cr);
    cairo_rectangle(cr, lw / 2, lw / 2, o->wd - lw, o->ht - lw);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], 1);
    cairo_set_line_width(cr, lw);
    cairo_stroke(cr);
    char label[16];
    snprintf(label, sizeof label, "R%d", o->idx + 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, SH / 40.0);
    cairo_text_extents_t e;
    cairo_text_extents(cr, label, &e);
    cairo_move_to(cr, lw * 2.5, lw * 2.5 - e.y_bearing);
    cairo_text_path(cr, label);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.9);
    cairo_set_line_width(cr, lw * 0.5);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], 1);
    cairo_fill(cr);
}
static char b_title[200], b_text[300];
static int mk_x[3] = {-1, -1, -1}, mk_y[3] = {-1, -1, -1};
static gboolean mk_on[3], markers_wanted;

static const double col[3][3] = {{1.0, 0.55, 0.05}, {0.10, 0.80, 1.0}, {0.95, 0.30, 0.90}};

Display *ov_display(void)
{
    return od;
}

static void ov_create(Ov *o, int w, int h, int kind, int idx)
{
    XSetWindowAttributes a;
    memset(&a, 0, sizeof a);
    a.override_redirect = True;
    a.colormap = cmap;
    a.background_pixel = 0;
    a.border_pixel = 0;
    o->w = XCreateWindow(od, DefaultRootWindow(od), 0, 0, (unsigned)w, (unsigned)h, 0, depth, InputOutput, vis,
                         CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &a);
    XShapeCombineRectangles(od, o->w, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    XSelectInput(od, o->w, ExposureMask);
    XStoreName(od, o->w, "autoclicker-advanced overlay");
    long pid = getpid();
    XChangeProperty(od, o->w, XInternAtom(od, "_NET_WM_PID", False), XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&pid, 1);
    o->wd = w;
    o->ht = h;
    o->kind = kind;
    o->idx = idx;
    xa_register_own(o->w);
}

static void text_center(cairo_t *cr, const char *t, double size, int w, double y, int bold)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t e;
    cairo_text_extents(cr, t, &e);
    while (e.width > w * 0.94 && size > 8) {
        size *= 0.92;
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, t, &e);
    }
    cairo_move_to(cr, (w - e.width) / 2 - e.x_bearing, y);
    cairo_show_text(cr, t);
}

static void draw_banner(cairo_t *cr, int w, int h)
{
    double r = h * 0.12, lw = h * 0.035;
    cairo_new_path(cr);
    cairo_arc(cr, w - r - lw, r + lw, r, -M_PI / 2, 0);
    cairo_arc(cr, w - r - lw, h - r - lw, r, 0, M_PI / 2);
    cairo_arc(cr, r + lw, h - r - lw, r, M_PI / 2, M_PI);
    cairo_arc(cr, r + lw, r + lw, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 0.88);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1.0, 0.85, 0.1, 1.0);
    cairo_set_line_width(cr, lw);
    cairo_stroke(cr);
    text_center(cr, b_title, h * 0.36, w, h * 0.45, 1);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    text_center(cr, b_text, h * 0.26, w, h * 0.80, 0);
}

static void draw_ring(cairo_t *cr, int w, int h, const double *c, const char *label, double scale)
{
    double cx = w / 2.0, cy = h / 2.0, r = w / 3.0 * scale, lw = r * 0.16;
    /* dark outline for contrast on any background */
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65);
    cairo_set_line_width(cr, lw * 1.8);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], 1);
    cairo_set_line_width(cr, lw);
    cairo_stroke(cr);
    /* small center dot = exact click point */
    cairo_arc(cr, cx, cy, lw * 0.75, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy, lw * 0.50, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], 1);
    cairo_fill(cr);
    /* label in the upper right corner */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, w * 0.26);
    cairo_text_extents_t e;
    cairo_text_extents(cr, label, &e);
    double tx = w - e.width - e.x_bearing - w * 0.03, ty = -e.y_bearing + h * 0.03;
    cairo_move_to(cr, tx, ty);
    cairo_text_path(cr, label);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.9);
    cairo_set_line_width(cr, w * 0.04);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(cr, c[0], c[1], c[2], 1);
    cairo_fill(cr);
}

static void paint(Ov *o)
{
    if (!o->shown)
        return;
    cairo_surface_t *s = cairo_xlib_surface_create(od, o->w, vis, o->wd, o->ht);
    cairo_t *cr = cairo_create(s);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    if (o->kind == 0)
        draw_banner(cr, o->wd, o->ht);
    else if (o->kind == 3)
        draw_frame(cr, o);
    else {
        char label[16];
        snprintf(label, sizeof label, "%d", o->idx + 1);
        draw_ring(cr, o->wd, o->ht, col[o->idx % 3], label, o->kind == 2 ? 1.0 : 0.9);
    }
    cairo_destroy(cr);
    cairo_surface_flush(s);
    cairo_surface_destroy(s);
    XFlush(od);
}

static void show_at(Ov *o, int x, int y)
{
    if (o->shown && o->x == x && o->y == y) {
        XRaiseWindow(od, o->w);
        XFlush(od);
        return;
    }
    o->x = x;
    o->y = y;
    XMoveWindow(od, o->w, x, y);
    if (!o->shown) {
        XMapRaised(od, o->w);
        o->shown = TRUE;
        XSync(od, False);
        paint(o);
    } else {
        XRaiseWindow(od, o->w);
        XFlush(od);
    }
}

static void hide(Ov *o)
{
    if (o->shown) {
        XUnmapWindow(od, o->w);
        o->shown = FALSE;
        XFlush(od);
    }
}

static gboolean on_x_events(gint fd, GIOCondition c, gpointer u)
{
    (void)fd;
    (void)c;
    (void)u;
    while (XPending(od)) {
        XEvent e;
        XNextEvent(od, &e);
        if (e.type != Expose || e.xexpose.count != 0)
            continue;
        Ov *all[8] = {&banner, &marker[0], &marker[1], &marker[2], &pick, &frame_ov[0], &frame_ov[1], &frame_ov[2]};
        for (int i = 0; i < 8; i++)
            if (all[i]->w == e.xany.window)
                paint(all[i]);
    }
    return G_SOURCE_CONTINUE;
}

gboolean ov_init(void)
{
    od = XOpenDisplay(NULL);
    if (!od)
        return FALSE;
    int scr = DefaultScreen(od);
    SW = DisplayWidth(od, scr);
    SH = DisplayHeight(od, scr);
    XVisualInfo vi;
    if (XMatchVisualInfo(od, scr, 32, TrueColor, &vi)) {
        vis = vi.visual;
        depth = 32;
    } else {
        vis = DefaultVisual(od, scr);
        depth = DefaultDepth(od, scr);
    }
    cmap = XCreateColormap(od, DefaultRootWindow(od), vis, AllocNone);
    ov_create(&banner, SW * 60 / 100, SH * 9 / 100, 0, 0);
    int ms = SH / 15, ps = SH / 9;
    for (int i = 0; i < 3; i++)
        ov_create(&marker[i], ms, ms, 1, i);
    ov_create(&pick, ps, ps, 2, 0);
    for (int i = 0; i < NFRAMES; i++)
        ov_create(&frame_ov[i], 16, 16, 3, i);
    g_unix_fd_add(ConnectionNumber(od), G_IO_IN, on_x_events, NULL);
    XFlush(od);
    return TRUE;
}

static void markers_apply(void)
{
    if (!od)
        return;
    for (int i = 0; i < NFRAMES; i++) {
        Ov *o = &frame_ov[i];
        const Rect *r = &fr_cfg[i].r;
        if (markers_wanted && fr_on[i] && r->w > 0 && r->h > 0) {
            if (o->wd != r->w || o->ht != r->h) {
                o->wd = r->w;
                o->ht = r->h;
                XResizeWindow(od, o->w, (unsigned)r->w, (unsigned)r->h);
                if (o->shown) {
                    o->x = r->x;
                    o->y = r->y;
                    XMoveWindow(od, o->w, r->x, r->y);
                    paint(o);
                }
            }
            show_at(o, r->x, r->y);
        } else
            hide(o);
    }
    for (int i = 0; i < 3; i++) {
        if (markers_wanted && mk_on[i] && mk_x[i] >= 0 && mk_y[i] >= 0)
            show_at(&marker[i], mk_x[i] - marker[i].wd / 2, mk_y[i] - marker[i].ht / 2);
        else
            hide(&marker[i]);
    }
    if (pick.shown)
        XRaiseWindow(od, pick.w);
    if (banner.shown)
        XRaiseWindow(od, banner.w);
    XFlush(od);
}

void ov_markers_set(int idx, int x, int y, gboolean enabled)
{
    if (idx < 0 || idx > 2)
        return;
    mk_x[idx] = x;
    mk_y[idx] = y;
    mk_on[idx] = enabled;
    markers_apply();
}

void ov_frames_set(int idx, const FrameCfg *f, gboolean enabled)
{
    if (idx < 0 || idx >= NFRAMES)
        return;
    gboolean zones_changed = memcmp(&fr_cfg[idx], f, sizeof *f) != 0;
    fr_cfg[idx] = *f;
    fr_on[idx] = enabled;
    if (zones_changed && frame_ov[idx].shown)
        paint(&frame_ov[idx]);
    markers_apply();
}

void ov_markers_show(gboolean show)
{
    markers_wanted = show;
    markers_apply();
}

void ov_banner(const char *title, const char *text)
{
    if (!od)
        return;
    if (!title) {
        hide(&banner);
        return;
    }
    snprintf(b_title, sizeof b_title, "%s", title);
    snprintf(b_text, sizeof b_text, "%s", text ? text : "");
    if (banner.shown) {
        XRaiseWindow(od, banner.w);
        paint(&banner);
    } else
        show_at(&banner, (SW - banner.wd) / 2, SH / 50);
}

void ov_pick(int idx, int x, int y)
{
    if (!od)
        return;
    if (idx < 0) {
        hide(&pick);
        return;
    }
    if (pick.idx != idx) {
        pick.idx = idx;
        if (pick.shown)
            paint(&pick);
    }
    show_at(&pick, x - pick.wd / 2, y - pick.ht / 2);
    if (banner.shown)
        XRaiseWindow(od, banner.w);
}
