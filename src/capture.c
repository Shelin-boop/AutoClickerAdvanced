/*
 * AutoClicker Advanced - screen capture from X11 windows
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 */
#include "capture.h"

#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "x11api.h"

static int mask_shift(unsigned long m)
{
    int s = 0;
    if (!m)
        return 0;
    while (!(m & 1)) {
        m >>= 1;
        s++;
    }
    return s;
}

static void to_rgb(XImage *img, int w, int h, unsigned char *out)
{
    unsigned long rm = img->red_mask, gm = img->green_mask, bm = img->blue_mask;
    int fast = img->bits_per_pixel == 32 && img->byte_order == LSBFirst && rm == 0xff0000 && gm == 0xff00 && bm == 0xff;
    int rs = mask_shift(rm), gs = mask_shift(gm), bs = mask_shift(bm);
    for (int y = 0; y < h; y++) {
        unsigned char *o = out + (size_t)y * w * 3;
        if (fast) {
            const unsigned char *s = (const unsigned char *)img->data + (size_t)y * img->bytes_per_line;
            for (int x = 0; x < w; x++) {
                o[x * 3 + 0] = s[x * 4 + 2];
                o[x * 3 + 1] = s[x * 4 + 1];
                o[x * 3 + 2] = s[x * 4 + 0];
            }
        } else {
            for (int x = 0; x < w; x++) {
                unsigned long px = XGetPixel(img, x, y);
                o[x * 3 + 0] = (unsigned char)((px & rm) >> rs);
                o[x * 3 + 1] = (unsigned char)((px & gm) >> gs);
                o[x * 3 + 2] = (unsigned char)((px & bm) >> bs);
            }
        }
    }
}

void shot_free(Shot *s)
{
    g_free(s->rgb);
    memset(s, 0, sizeof *s);
}

void cap_cache_free(Display *d, CapCache *cc)
{
    if (cc->img) {
        XShmDetach(d, &cc->si);
        XSync(d, False);
        cc->img->data = NULL;
        XDestroyImage(cc->img);
        shmdt(cc->si.shmaddr);
    }
    memset(cc, 0, sizeof *cc);
}

/* top-level foreign window at a screen point */
static Window toplevel_at(Display *d, int rx, int ry)
{
    int lx, ly;
    Window w = xa_target_at(d, rx, ry, &lx, &ly);
    if (w == None)
        return None;
    Window root = DefaultRootWindow(d), r, parent = None, *kids = NULL;
    unsigned nk;
    for (int i = 0; i < 32; i++) {
        if (!XQueryTree(d, w, &r, &parent, &kids, &nk))
            return None;
        if (kids)
            XFree(kids);
        if (parent == root || parent == None)
            return w;
        w = parent;
    }
    return w;
}

gboolean cap_area(Display *d, CapCache *cc, const Rect *r, Shot *out)
{
    memset(out, 0, sizeof *out);
    if (r->w <= 0 || r->h <= 0)
        return FALSE;
    Window win = toplevel_at(d, r->x + r->w / 2, r->y + r->h / 2);
    if (win == None)
        return FALSE;
    XWindowAttributes a;
    if (!XGetWindowAttributes(d, win, &a) || a.map_state != IsViewable)
        return FALSE;
    int wx, wy;
    Window child;
    if (!XTranslateCoordinates(d, DefaultRootWindow(d), win, r->x, r->y, &wx, &wy, &child))
        return FALSE;
    /* clip to the window */
    int x0 = wx < 0 ? 0 : wx, y0 = wy < 0 ? 0 : wy;
    int x1 = wx + r->w > a.width ? a.width : wx + r->w, y1 = wy + r->h > a.height ? a.height : wy + r->h;
    int w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0)
        return FALSE;

    XImage *img = NULL;
    gboolean from_shm = FALSE;
    if (!cc->shm_failed) {
        if (cc->img && (cc->win != win || cc->w != w || cc->h != h))
            cap_cache_free(d, cc);
        if (!cc->img) {
            cc->img = XShmCreateImage(d, a.visual, (unsigned)a.depth, ZPixmap, NULL, &cc->si, (unsigned)w, (unsigned)h);
            if (cc->img) {
                cc->si.shmid = shmget(IPC_PRIVATE, (size_t)cc->img->bytes_per_line * cc->img->height, IPC_CREAT | 0600);
                if (cc->si.shmid >= 0) {
                    cc->si.shmaddr = cc->img->data = shmat(cc->si.shmid, NULL, 0);
                    cc->si.readOnly = False;
                    int before = xa_error_count();
                    XShmAttach(d, &cc->si);
                    XSync(d, False);
                    shmctl(cc->si.shmid, IPC_RMID, NULL);
                    if (xa_error_count() != before) {
                        shmdt(cc->si.shmaddr);
                        cc->img->data = NULL;
                        XDestroyImage(cc->img);
                        cc->img = NULL;
                        cc->shm_failed = TRUE;
                    }
                } else {
                    XDestroyImage(cc->img);
                    cc->img = NULL;
                    cc->shm_failed = TRUE;
                }
            } else
                cc->shm_failed = TRUE;
            cc->win = win;
            cc->w = w;
            cc->h = h;
        }
        if (cc->img) {
            int before = xa_error_count();
            Bool ok = XShmGetImage(d, win, cc->img, x0, y0, AllPlanes);
            XSync(d, False);
            if (ok && xa_error_count() == before) {
                img = cc->img;
                from_shm = TRUE;
            }
        }
    }
    if (!img) {
        int before = xa_error_count();
        img = XGetImage(d, win, x0, y0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
        XSync(d, False);
        if (!img || xa_error_count() != before) {
            if (img)
                XDestroyImage(img);
            return FALSE;
        }
    }
    out->rgb = g_malloc((size_t)w * h * 3);
    to_rgb(img, w, h, out->rgb);
    if (!from_shm)
        XDestroyImage(img);
    out->w = w;
    out->h = h;
    out->ox = r->x + (x0 - wx);
    out->oy = r->y + (y0 - wy);
    return TRUE;
}

gboolean cap_window_at(Display *d, int rx, int ry, Shot *out)
{
    memset(out, 0, sizeof *out);
    Window win = toplevel_at(d, rx, ry);
    if (win == None)
        return FALSE;
    XWindowAttributes a;
    if (!XGetWindowAttributes(d, win, &a) || a.map_state != IsViewable)
        return FALSE;
    int ox, oy;
    Window child;
    XTranslateCoordinates(d, win, DefaultRootWindow(d), 0, 0, &ox, &oy, &child);
    Rect r = {ox, oy, a.width, a.height};
    CapCache cc;
    memset(&cc, 0, sizeof cc);
    cc.shm_failed = TRUE; /* one shot: plain XGetImage */
    return cap_area(d, &cc, &r, out);
}
