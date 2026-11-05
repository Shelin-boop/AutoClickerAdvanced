/*
 * AutoClicker Advanced - pattern reactions
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Lastenheft v1.1, F-25 to F-28:
 *  - per check at most one click, the strongest new hit
 *  - hits in an exclusion zone are ignored
 *  - patterns visible at the first check are "known" and never clicked;
 *    known patterns are followed within the lock radius (wobbling counters);
 *    exception: a hit at the place where the pattern was taken is never known
 *  - after a click the place is locked for the lock time
 *  - known patterns not seen for 2 s are forgotten
 */
#include "react.h"

#include <string.h>

#include "x11api.h"

#define FORGET_MS 2000
#define MAX_HITS 24

void react_init(React *r, const FrameCfg *f)
{
    memset(r, 0, sizeof *r);
    r->f = *f;
}

void react_free(React *r, Display *d)
{
    cap_cache_free(d, &r->cache);
}

static int dist2(int ax, int ay, int bx, int by)
{
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

static int near_point(const RPoint *pts, int n, int x, int y, int radius)
{
    int r2 = radius * radius;
    for (int i = 0; i < n; i++)
        if (dist2(pts[i].x, pts[i].y, x, y) <= r2)
            return i;
    return -1;
}

gboolean react_check(React *r, Display *d, const Pattern *p, const Config *c, int *tx, int *ty, float *score)
{
    gint64 t0 = xa_now_ms();
    Shot s;
    if (!cap_area(d, &r->cache, &r->f.r, &s)) {
        r->best = 0;
        r->ms = (int)(xa_now_ms() - t0);
        return FALSE;
    }
    Hit h[MAX_HITS];
    float best = 0;
    int n = pattern_find(p, s.rgb, s.w * 3, s.w, s.h, c->threshold_pct / 100.0f, h, MAX_HITS, &best);
    r->best = best;
    r->checks++;
    gint64 now = xa_now_ms();

    /* expire locks and forget known patterns not seen for a while */
    int k = 0;
    for (int i = 0; i < r->nl; i++)
        if (r->lock[i].t > now)
            r->lock[k++] = r->lock[i];
    r->nl = k;
    k = 0;
    for (int i = 0; i < r->nk; i++)
        if (now - r->known[i].t <= FORGET_MS)
            r->known[k++] = r->known[i];
    r->nk = k;

    int radius = c->lock_radius;
    int spot_r = MAX(radius, p->size / 4);
    gboolean found = FALSE;
    for (int i = 0; i < n; i++) {
        if (h[i].color > c->color_tol)
            continue;
        int hx = s.ox + h[i].x, hy = s.oy + h[i].y;
        gboolean in_zone = FALSE;
        for (int z = 0; z < r->f.nzones; z++)
            if (rect_contains(&r->f.zone[z], hx, hy))
                in_zone = TRUE;
        if (in_zone)
            continue;
        gboolean at_spot = c->pat_x >= 0 && dist2(hx, hy, c->pat_x, c->pat_y) <= spot_r * spot_r;
        if (!r->started && c->ignore_existing && !at_spot) {
            if (r->nk < REACT_KNOWN)
                r->known[r->nk++] = (RPoint){hx, hy, now};
            continue;
        }
        int kn = near_point(r->known, r->nk, hx, hy, radius);
        if (kn >= 0) { /* follow a known pattern that wobbles */
            r->known[kn] = (RPoint){hx, hy, now};
            continue;
        }
        if (near_point(r->lock, r->nl, hx, hy, radius) >= 0)
            continue;
        if (!found) { /* hits are sorted, the first new one is the strongest */
            found = TRUE;
            *tx = hx;
            *ty = hy;
            *score = h[i].score;
        }
    }
    r->started = TRUE;
    shot_free(&s);
    r->ms = (int)(xa_now_ms() - t0);
    return found;
}

void react_clicked(React *r, const Config *c, int x, int y)
{
    gint64 until = xa_now_ms() + c->lock_ms;
    if (r->nl < REACT_LOCKS)
        r->lock[r->nl++] = (RPoint){x, y, until};
    else
        r->lock[0] = (RPoint){x, y, until};
}
