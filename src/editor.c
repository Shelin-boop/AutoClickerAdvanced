/*
 * AutoClicker Advanced - screenshot editor for pattern, frames and exclusion zones
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Lastenheft v1.1, F-20 to F-22, F-26: the user selects on a frozen screenshot
 *  - the pattern: a square, the inscribed circle is compared (8 to 200 px)
 *  - frames 1 to 3: rectangles that must not overlap
 *  - exclusion zones: up to 8 rectangles inside a frame
 * "Test detection" runs the search on the screenshot and shows every hit with
 * its similarity, so the threshold can be checked before saving.
 */
#include "editor.h"

#include <math.h>
#include <string.h>

#include "config.h"
#include "match.h"

enum { TOOL_PATTERN, TOOL_F1, TOOL_F2, TOOL_F3, TOOL_ZONE, TOOL_DELETE, NTOOLS };

typedef struct {
    int x, y, frame;
    float score, color;
    gboolean ok;
} Res;

static struct {
    GtkWidget *win, *area, *scroll, *preview, *info, *tbtn[NTOOLS];
    Shot shot;
    cairo_surface_t *img;
    double scale;
    gboolean fit;
    int tool;
    gboolean have_pat;
    Rect pat;        /* screen pixels */
    guchar *pat_rgb; /* pattern pixels: the stored pattern or the new selection */
    gboolean pat_changed;
    cairo_surface_t *pat_img;
    FrameCfg frame[NFRAMES];
    gboolean dragging;
    double ax, ay, bx, by; /* drag in image pixels */
    Res res[96];
    int nres;
    gboolean tested;
    void (*on_saved)(void);
} E;

static const double fcol[3][3] = {{0.25, 0.90, 0.35}, {1.0, 0.85, 0.15}, {0.35, 0.60, 1.0}};

gboolean editor_is_open(void)
{
    return E.win != NULL;
}

static void info(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *t = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    gtk_label_set_text(GTK_LABEL(E.info), t);
    g_free(t);
}

static cairo_surface_t *surface_from_rgb(const guchar *rgb, int w, int h)
{
    cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    unsigned char *d = cairo_image_surface_get_data(img);
    int st = cairo_image_surface_get_stride(img);
    for (int y = 0; y < h; y++) {
        guint32 *row = (guint32 *)(d + (size_t)y * st);
        const guchar *q = rgb + (size_t)y * w * 3;
        for (int x = 0; x < w; x++, q += 3)
            row[x] = ((guint32)q[0] << 16) | ((guint32)q[1] << 8) | q[2];
    }
    cairo_surface_mark_dirty(img);
    return img;
}

static void set_pattern_pixels(guchar *rgb)
{
    g_free(E.pat_rgb);
    E.pat_rgb = rgb;
    if (E.pat_img)
        cairo_surface_destroy(E.pat_img);
    E.pat_img = rgb ? surface_from_rgb(rgb, E.pat.w, E.pat.h) : NULL;
}

static void redraw(void)
{
    gtk_widget_queue_draw(E.area);
    gtk_widget_queue_draw(E.preview);
}

static void untest(void)
{
    E.nres = 0;
    E.tested = FALSE;
}

/* ------------------------------------------------------------------ */
/* geometry                                                           */
/* ------------------------------------------------------------------ */

static Rect drag_rect(void)
{
    double x0 = MIN(E.ax, E.bx), y0 = MIN(E.ay, E.by), x1 = MAX(E.ax, E.bx), y1 = MAX(E.ay, E.by);
    x0 = CLAMP(x0, 0, E.shot.w);
    x1 = CLAMP(x1, 0, E.shot.w);
    y0 = CLAMP(y0, 0, E.shot.h);
    y1 = CLAMP(y1, 0, E.shot.h);
    return (Rect){(int)x0 + E.shot.ox, (int)y0 + E.shot.oy, (int)(x1 - x0), (int)(y1 - y0)};
}

static Rect drag_square(void)
{
    double dx = E.bx - E.ax, dy = E.by - E.ay;
    int side = (int)MAX(fabs(dx), fabs(dy));
    side = CLAMP(side, MIN_PATTERN, MAX_PATTERN);
    int x = (int)(dx >= 0 ? E.ax : E.ax - side), y = (int)(dy >= 0 ? E.ay : E.ay - side);
    x = CLAMP(x, 0, E.shot.w - side);
    y = CLAMP(y, 0, E.shot.h - side);
    return (Rect){x + E.shot.ox, y + E.shot.oy, side, side};
}

static int frame_at(int sx, int sy)
{
    for (int i = 0; i < NFRAMES; i++)
        if (rect_contains(&E.frame[i].r, sx, sy))
            return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* drawing                                                            */
/* ------------------------------------------------------------------ */

static void rect_path(cairo_t *cr, const Rect *r)
{
    cairo_rectangle(cr, r->x - E.shot.ox, r->y - E.shot.oy, r->w, r->h);
}

static void label_at(cairo_t *cr, double x, double y, const char *t, const double *c, double size)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_text_path(cr, t);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.9);
    cairo_set_line_width(cr, size / 6);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgb(cr, c[0], c[1], c[2]);
    cairo_fill(cr);
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer u)
{
    (void)w;
    (void)u;
    if (!E.img)
        return FALSE;
    double s = E.scale, lw = 2.5 / s;
    cairo_save(cr);
    cairo_scale(cr, s, s);
    cairo_set_source_surface(cr, E.img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), s >= 1 ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_GOOD);
    cairo_paint(cr);

    for (int i = 0; i < NFRAMES; i++) {
        const FrameCfg *f = &E.frame[i];
        if (f->r.w <= 0)
            continue;
        for (int z = 0; z < f->nzones; z++) {
            rect_path(cr, &f->zone[z]);
            cairo_set_source_rgba(cr, 0.9, 0.1, 0.1, 0.30);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.95, 0.15, 0.15, 1);
            cairo_set_line_width(cr, lw);
            cairo_stroke(cr);
        }
        rect_path(cr, &f->r);
        cairo_set_source_rgb(cr, fcol[i][0], fcol[i][1], fcol[i][2]);
        cairo_set_line_width(cr, lw * 1.5);
        cairo_stroke(cr);
        char t[8];
        snprintf(t, sizeof t, "R%d", i + 1);
        label_at(cr, f->r.x - E.shot.ox + 8 / s, f->r.y - E.shot.oy + 30 / s, t, fcol[i], 22 / s);
    }
    if (E.have_pat) {
        const Rect *p = &E.pat;
        double cx = p->x - E.shot.ox + p->w / 2.0, cy = p->y - E.shot.oy + p->h / 2.0;
        rect_path(cr, p);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, lw);
        double dash[2] = {6 / s, 4 / s};
        cairo_set_dash(cr, dash, 2, 0);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);
        cairo_arc(cr, cx, cy, p->w / 2.0, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_arc(cr, cx, cy, p->w / 2.0 * cfg.core_pct / 100.0, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 1, 0.3, 1);
        cairo_stroke(cr);
    }
    for (int i = 0; i < E.nres; i++) {
        const Res *r = &E.res[i];
        double cx = r->x - E.shot.ox, cy = r->y - E.shot.oy, rad = E.pat.w / 2.0;
        static const double green[3] = {0.2, 1.0, 0.3}, red[3] = {1.0, 0.3, 0.3};
        const double *c = r->ok ? green : red;
        cairo_arc(cr, cx, cy, rad, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, c[0], c[1], c[2]);
        cairo_set_line_width(cr, lw * 1.5);
        cairo_stroke(cr);
        char t[16];
        snprintf(t, sizeof t, "%.2f", r->score);
        label_at(cr, cx - rad, cy - rad - 6 / s, t, c, 20 / s);
    }
    if (E.dragging) {
        Rect r = E.tool == TOOL_PATTERN ? drag_square() : drag_rect();
        rect_path(cr, &r);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, lw);
        cairo_stroke(cr);
        if (E.tool == TOOL_PATTERN) {
            cairo_arc(cr, r.x - E.shot.ox + r.w / 2.0, r.y - E.shot.oy + r.h / 2.0, r.w / 2.0, 0, 2 * M_PI);
            cairo_stroke(cr);
        }
    }
    cairo_restore(cr);
    return FALSE;
}

static gboolean on_preview_draw(GtkWidget *w, cairo_t *cr, gpointer u)
{
    (void)u;
    int aw = gtk_widget_get_allocated_width(w), ah = gtk_widget_get_allocated_height(w);
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_paint(cr);
    if (!E.have_pat || !E.pat_img)
        return FALSE;
    double s = MIN(aw, ah) / (double)E.pat.w;
    cairo_save(cr);
    cairo_translate(cr, (aw - E.pat.w * s) / 2, (ah - E.pat.w * s) / 2);
    cairo_scale(cr, s, s);
    cairo_rectangle(cr, 0, 0, E.pat.w, E.pat.h);
    cairo_clip(cr);
    cairo_set_source_surface(cr, E.pat_img, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_set_line_width(cr, 1.5 / s);
    cairo_arc(cr, E.pat.w / 2.0, E.pat.h / 2.0, E.pat.w / 2.0, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_stroke(cr);
    cairo_arc(cr, E.pat.w / 2.0, E.pat.h / 2.0, E.pat.w / 2.0 * cfg.core_pct / 100.0, 0, 2 * M_PI);
    cairo_set_source_rgb(cr, 1, 0.3, 1);
    cairo_stroke(cr);
    cairo_restore(cr);
    return FALSE;
}

static void apply_scale(void)
{
    if (E.fit) {
        int aw = gtk_widget_get_allocated_width(E.scroll) - 4, ah = gtk_widget_get_allocated_height(E.scroll) - 4;
        if (aw > 50 && ah > 50)
            E.scale = MIN(aw / (double)E.shot.w, ah / (double)E.shot.h);
    }
    gtk_widget_set_size_request(E.area, (int)(E.shot.w * E.scale), (int)(E.shot.h * E.scale));
    redraw();
}

static void on_scroll_alloc(GtkWidget *w, GdkRectangle *a, gpointer u)
{
    (void)w;
    (void)a;
    (void)u;
    if (E.fit) {
        double old = E.scale;
        apply_scale();
        if (fabs(old - E.scale) < 1e-6)
            return;
    }
}

/* ------------------------------------------------------------------ */
/* mouse                                                              */
/* ------------------------------------------------------------------ */

static guchar *crop_rgb(const Rect *r);

static void to_image(double wx, double wy, double *ix, double *iy)
{
    *ix = wx / E.scale;
    *iy = wy / E.scale;
}

static void delete_at(int sx, int sy)
{
    for (int i = 0; i < NFRAMES; i++) {
        FrameCfg *f = &E.frame[i];
        for (int z = 0; z < f->nzones; z++)
            if (rect_contains(&f->zone[z], sx, sy)) {
                memmove(&f->zone[z], &f->zone[z + 1], sizeof(Rect) * (size_t)(f->nzones - z - 1));
                f->nzones--;
                info("Exclusion zone deleted.");
                return;
            }
    }
    if (E.have_pat && rect_contains(&E.pat, sx, sy)) {
        E.have_pat = FALSE;
        E.pat_changed = TRUE;
        set_pattern_pixels(NULL);
        info("Pattern deleted.");
        return;
    }
    int fi = frame_at(sx, sy);
    if (fi >= 0) {
        memset(&E.frame[fi], 0, sizeof E.frame[fi]);
        info("Frame %d and its exclusion zones deleted.", fi + 1);
        return;
    }
    info("Nothing to delete here.");
}

static gboolean on_press(GtkWidget *w, GdkEventButton *ev, gpointer u)
{
    (void)w;
    (void)u;
    if (ev->button != 1)
        return FALSE;
    double ix, iy;
    to_image(ev->x, ev->y, &ix, &iy);
    if (E.tool == TOOL_DELETE) {
        untest();
        delete_at((int)ix + E.shot.ox, (int)iy + E.shot.oy);
        redraw();
        return TRUE;
    }
    E.dragging = TRUE;
    E.ax = E.bx = ix;
    E.ay = E.by = iy;
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer u)
{
    (void)w;
    (void)u;
    if (!E.dragging)
        return FALSE;
    to_image(ev->x, ev->y, &E.bx, &E.by);
    if (E.tool == TOOL_PATTERN) {
        Rect r = drag_square();
        info("Pattern %d x %d px", r.w, r.h);
    } else {
        Rect r = drag_rect();
        info("%d x %d px at %d, %d", r.w, r.h, r.x, r.y);
    }
    gtk_widget_queue_draw(E.area);
    return TRUE;
}

static gboolean on_release(GtkWidget *w, GdkEventButton *ev, gpointer u)
{
    (void)w;
    (void)u;
    if (!E.dragging || ev->button != 1)
        return FALSE;
    E.dragging = FALSE;
    to_image(ev->x, ev->y, &E.bx, &E.by);
    untest();
    if (E.tool == TOOL_PATTERN) {
        if (fabs(E.bx - E.ax) < 3 && fabs(E.by - E.ay) < 3) {
            info("Drag a square around the pattern.");
        } else {
            E.pat = drag_square();
            E.have_pat = TRUE;
            E.pat_changed = TRUE;
            set_pattern_pixels(crop_rgb(&E.pat));
            info("Pattern %d px selected. The inner circle (%d %%) is compared.", E.pat.w, cfg.core_pct);
        }
    } else if (E.tool >= TOOL_F1 && E.tool <= TOOL_F3) {
        int fi = E.tool - TOOL_F1;
        Rect r = drag_rect();
        if (r.w < 20 || r.h < 20) {
            info("The frame is too small.");
        } else {
            int other = -1;
            for (int i = 0; i < NFRAMES; i++)
                if (i != fi && rect_overlap(&E.frame[i].r, &r))
                    other = i;
            if (other >= 0)
                info("Frames must not overlap: this frame overlaps frame %d. Not accepted.", other + 1);
            else {
                E.frame[fi].r = r;
                E.frame[fi].nzones = 0;
                info("Frame %d: %d x %d px at %d, %d.", fi + 1, r.w, r.h, r.x, r.y);
            }
        }
    } else if (E.tool == TOOL_ZONE) {
        Rect r = drag_rect();
        int fi = frame_at((int)E.ax + E.shot.ox, (int)E.ay + E.shot.oy);
        if (fi < 0)
            info("Start an exclusion zone inside a frame.");
        else if (r.w < 4 || r.h < 4)
            info("The exclusion zone is too small.");
        else if (E.frame[fi].nzones >= MAX_ZONES)
            info("Frame %d already has %d exclusion zones.", fi + 1, MAX_ZONES);
        else {
            const Rect *f = &E.frame[fi].r;
            int x0 = MAX(r.x, f->x), y0 = MAX(r.y, f->y), x1 = MIN(r.x + r.w, f->x + f->w),
                y1 = MIN(r.y + r.h, f->y + f->h);
            E.frame[fi].zone[E.frame[fi].nzones++] = (Rect){x0, y0, x1 - x0, y1 - y0};
            info("Exclusion zone added to frame %d (%d of %d).", fi + 1, E.frame[fi].nzones, MAX_ZONES);
        }
    }
    redraw();
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* buttons                                                            */
/* ------------------------------------------------------------------ */

static guchar *crop_rgb(const Rect *r)
{
    guchar *out = g_malloc((size_t)r->w * r->h * 3);
    for (int y = 0; y < r->h; y++)
        memcpy(out + (size_t)y * r->w * 3,
               E.shot.rgb + ((size_t)(r->y - E.shot.oy + y) * E.shot.w + (r->x - E.shot.ox)) * 3, (size_t)r->w * 3);
    return out;
}

static void on_test(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    untest();
    if (!E.have_pat) {
        info("Select a pattern first.");
        redraw();
        return;
    }
    Pattern p;
    gboolean ok = E.pat_rgb && pattern_init(&p, E.pat_rgb, E.pat.w * 3, E.pat.w, cfg.downscale, cfg.core_pct / 100.0f);
    if (!ok) {
        info("This pattern cannot be used (too small or without contrast).");
        redraw();
        return;
    }
    float thr = cfg.threshold_pct / 100.0f;
    GString *t = g_string_new(NULL);
    gboolean any_frame = FALSE;
    for (int i = 0; i < NFRAMES + 1; i++) {
        Rect fr;
        const FrameCfg *f = i < NFRAMES ? &E.frame[i] : NULL;
        if (f) {
            if (f->r.w <= 0)
                continue;
            any_frame = TRUE;
            fr = f->r;
        } else {
            if (any_frame)
                break;
            fr = (Rect){E.shot.ox, E.shot.oy, E.shot.w, E.shot.h}; /* no frame yet: whole screenshot */
        }
        int x0 = MAX(fr.x, E.shot.ox), y0 = MAX(fr.y, E.shot.oy);
        int x1 = MIN(fr.x + fr.w, E.shot.ox + E.shot.w), y1 = MIN(fr.y + fr.h, E.shot.oy + E.shot.h);
        if (x1 - x0 < E.pat.w || y1 - y0 < E.pat.w)
            continue;
        Hit h[32];
        float best = 0;
        const guchar *base = E.shot.rgb + ((size_t)(y0 - E.shot.oy) * E.shot.w + (x0 - E.shot.ox)) * 3;
        int n = pattern_find(&p, base, E.shot.w * 3, x1 - x0, y1 - y0, 0.5f, h, 32, &best);
        int good = 0;
        for (int k = 0; k < n && E.nres < (int)G_N_ELEMENTS(E.res); k++) {
            Res *r = &E.res[E.nres++];
            r->x = x0 + h[k].x;
            r->y = y0 + h[k].y;
            r->score = h[k].score;
            r->color = h[k].color;
            r->frame = f ? i : -1;
            gboolean in_zone = FALSE;
            if (f)
                for (int z = 0; z < f->nzones; z++)
                    if (rect_contains(&f->zone[z], r->x, r->y))
                        in_zone = TRUE;
            r->ok = h[k].score >= thr && h[k].color <= cfg.color_tol && !in_zone;
            if (r->ok)
                good++;
        }
        if (f)
            g_string_append_printf(t, "R%d: %d hit%s at threshold %.2f, best %.2f.   ", i + 1, good, good == 1 ? "" : "s",
                                   thr, best);
        else
            g_string_append_printf(t, "Whole screenshot: %d hit%s at threshold %.2f, best %.2f.   ", good,
                                   good == 1 ? "" : "s", thr, best);
    }
    pattern_free(&p);
    g_string_append(t, "Green = would react, red = below threshold, other color or in an exclusion zone.");
    gtk_label_set_text(GTK_LABEL(E.info), t->str);
    g_string_free(t, TRUE);
    E.tested = TRUE;
    redraw();
}

static void close_editor(void)
{
    if (!E.win)
        return;
    GtkWidget *w = E.win;
    E.win = NULL;
    gtk_widget_destroy(w);
}

static void on_destroy(GtkWidget *w, gpointer u)
{
    (void)w;
    (void)u;
    E.win = NULL;
    if (E.img)
        cairo_surface_destroy(E.img);
    E.img = NULL;
    set_pattern_pixels(NULL);
    shot_free(&E.shot);
}

static void on_save(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    if (E.pat_changed && E.have_pat) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_data(E.pat_rgb, GDK_COLORSPACE_RGB, FALSE, 8, E.pat.w, E.pat.h,
                                                 E.pat.w * 3, NULL, NULL);
        char *path = config_pattern_path();
        GError *e = NULL;
        gboolean ok = gdk_pixbuf_save(pb, path, "png", &e, NULL);
        g_object_unref(pb);
        g_free(path);
        if (!ok) {
            info("Cannot save the pattern: %s", e ? e->message : "?");
            if (e)
                g_error_free(e);
            return;
        }
        cfg.have_pattern = TRUE;
        cfg.pat_size = E.pat.w;
        cfg.pat_x = E.pat.x + E.pat.w / 2;
        cfg.pat_y = E.pat.y + E.pat.h / 2;
    } else if (E.pat_changed) {
        cfg.have_pattern = FALSE;
        cfg.pat_size = 0;
        cfg.pat_x = cfg.pat_y = -1;
    }
    memcpy(cfg.frame, E.frame, sizeof cfg.frame);
    config_save();
    void (*cb)(void) = E.on_saved;
    close_editor();
    if (cb)
        cb();
}

static void on_cancel(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    close_editor();
}

static void on_tool(GtkToggleButton *t, gpointer u)
{
    if (!gtk_toggle_button_get_active(t))
        return;
    E.tool = GPOINTER_TO_INT(u);
    static const char *hints[NTOOLS] = {
        "Drag a square around the pattern. The circle inside the square is used.",
        "Drag frame 1 (green).",
        "Drag frame 2 (yellow).",
        "Drag frame 3 (blue).",
        "Drag an exclusion zone inside a frame. No reaction happens there.",
        "Click a pattern, frame or exclusion zone to delete it.",
    };
    info("%s", hints[E.tool]);
}

static void on_zoom(GtkButton *b, gpointer u)
{
    (void)b;
    E.fit = GPOINTER_TO_INT(u) == 0;
    if (!E.fit)
        E.scale = 1.0;
    apply_scale();
}

/* ------------------------------------------------------------------ */

void editor_open(GtkWindow *parent, const Shot *shot, void (*on_saved)(void))
{
    if (E.win) {
        gtk_window_present(GTK_WINDOW(E.win));
        return;
    }
    memset(&E, 0, sizeof E);
    E.on_saved = on_saved;
    E.shot = *shot;
    E.shot.rgb = g_memdup2(shot->rgb, (gsize)shot->w * shot->h * 3);
    E.fit = TRUE;
    E.scale = 0.5;
    memcpy(E.frame, cfg.frame, sizeof E.frame);
    if (cfg.have_pattern && cfg.pat_size >= MIN_PATTERN) {
        /* show the stored pattern and where it was taken */
        char *path = config_pattern_path();
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, NULL);
        g_free(path);
        if (pb && gdk_pixbuf_get_width(pb) >= cfg.pat_size && gdk_pixbuf_get_height(pb) >= cfg.pat_size) {
            E.pat = (Rect){cfg.pat_x - cfg.pat_size / 2, cfg.pat_y - cfg.pat_size / 2, cfg.pat_size, cfg.pat_size};
            int nch = gdk_pixbuf_get_n_channels(pb), rs = gdk_pixbuf_get_rowstride(pb);
            const guchar *px = gdk_pixbuf_read_pixels(pb);
            guchar *rgb = g_malloc((size_t)E.pat.w * E.pat.h * 3);
            for (int y = 0; y < E.pat.h; y++)
                for (int x = 0; x < E.pat.w; x++)
                    memcpy(rgb + ((size_t)y * E.pat.w + x) * 3, px + (size_t)y * rs + (size_t)x * nch, 3);
            set_pattern_pixels(rgb);
            E.have_pat = TRUE;
        }
        if (pb)
            g_object_unref(pb);
    }

    E.img = surface_from_rgb(E.shot.rgb, E.shot.w, E.shot.h);

    E.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(E.win), "AutoClicker Advanced - pattern, frames and exclusion zones");
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(E.win), parent);
    gtk_window_set_default_size(GTK_WINDOW(E.win), 1500, 950);
    g_signal_connect(E.win, "destroy", G_CALLBACK(on_destroy), NULL);

    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vb), 8);
    gtk_container_add(GTK_CONTAINER(E.win), vb);

    GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    static const char *names[NTOOLS] = {"Pattern", "Frame 1", "Frame 2", "Frame 3", "Exclusion zone", "Delete"};
    GSList *group = NULL;
    for (int i = 0; i < NTOOLS; i++) {
        E.tbtn[i] = gtk_radio_button_new_with_label(group, names[i]);
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(E.tbtn[i]), FALSE);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(E.tbtn[i]));
        g_signal_connect(E.tbtn[i], "toggled", G_CALLBACK(on_tool), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(tb), E.tbtn[i], FALSE, FALSE, 0);
    }
    GtkWidget *bfit = gtk_button_new_with_label("Fit");
    GtkWidget *b100 = gtk_button_new_with_label("100 %");
    GtkWidget *btest = gtk_button_new_with_label("Test detection");
    GtkWidget *bsave = gtk_button_new_with_label("Save");
    GtkWidget *bcancel = gtk_button_new_with_label("Cancel");
    gtk_style_context_add_class(gtk_widget_get_style_context(bsave), "suggested-action");
    g_signal_connect(bfit, "clicked", G_CALLBACK(on_zoom), GINT_TO_POINTER(0));
    g_signal_connect(b100, "clicked", G_CALLBACK(on_zoom), GINT_TO_POINTER(1));
    g_signal_connect(btest, "clicked", G_CALLBACK(on_test), NULL);
    g_signal_connect(bsave, "clicked", G_CALLBACK(on_save), NULL);
    g_signal_connect(bcancel, "clicked", G_CALLBACK(on_cancel), NULL);
    gtk_box_pack_end(GTK_BOX(tb), bcancel, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(tb), bsave, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(tb), btest, FALSE, FALSE, 12);
    gtk_box_pack_end(GTK_BOX(tb), b100, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(tb), bfit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vb), tb, FALSE, FALSE, 0);

    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vb), hb, TRUE, TRUE, 0);
    E.scroll = gtk_scrolled_window_new(NULL, NULL);
    E.area = gtk_drawing_area_new();
    gtk_widget_add_events(E.area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(E.area, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(E.area, "button-press-event", G_CALLBACK(on_press), NULL);
    g_signal_connect(E.area, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_signal_connect(E.area, "button-release-event", G_CALLBACK(on_release), NULL);
    g_signal_connect(E.scroll, "size-allocate", G_CALLBACK(on_scroll_alloc), NULL);
    gtk_container_add(GTK_CONTAINER(E.scroll), E.area);
    gtk_box_pack_start(GTK_BOX(hb), E.scroll, TRUE, TRUE, 0);

    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *pl = gtk_label_new("Pattern (enlarged)");
    E.preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(E.preview, 220, 220);
    g_signal_connect(E.preview, "draw", G_CALLBACK(on_preview_draw), NULL);
    GtkWidget *legend = gtk_label_new("White circle: pattern.\nPink circle: compared part.\n\n"
                                      "Coordinates are screen pixels.");
    gtk_label_set_xalign(GTK_LABEL(legend), 0.0f);
    gtk_box_pack_start(GTK_BOX(side), pl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), E.preview, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(side), legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hb), side, FALSE, FALSE, 0);

    E.info = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(E.info), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(E.info), TRUE);
    gtk_box_pack_start(GTK_BOX(vb), E.info, FALSE, FALSE, 0);

    gtk_widget_show_all(E.win);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(E.tbtn[cfg.have_pattern ? TOOL_F1 : TOOL_PATTERN]), TRUE);
    on_tool(GTK_TOGGLE_BUTTON(E.tbtn[cfg.have_pattern ? TOOL_F1 : TOOL_PATTERN]),
            GINT_TO_POINTER(cfg.have_pattern ? TOOL_F1 : TOOL_PATTERN));
    apply_scale();
}
