/*
 * AutoClicker Advanced - pattern matching
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 */
#include "match.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* color as chromaticity scaled so that gray is (1, 1, 1): independent of brightness */
static void chroma(const double *rgb, float *out)
{
    double sum = rgb[0] + rgb[1] + rgb[2];
    for (int i = 0; i < 3; i++)
        out[i] = (float)(sum > 1 ? 3.0 * rgb[i] / sum : 1.0);
}

/* grayscale image reduced by f (box average) */
static float *to_gray(const unsigned char *rgb, int stride, int w, int h, int f, int *gw, int *gh)
{
    int W = w / f, H = h / f;
    *gw = W;
    *gh = H;
    if (W <= 0 || H <= 0)
        return NULL;
    float *g = malloc(sizeof(float) * (size_t)W * H);
    if (!g)
        return NULL;
    float inv = 1.0f / (f * f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float s = 0;
            for (int dy = 0; dy < f; dy++) {
                const unsigned char *p = rgb + (size_t)(y * f + dy) * stride + (size_t)x * f * 3;
                for (int dx = 0; dx < f; dx++, p += 3)
                    s += 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
            }
            g[(size_t)y * W + x] = s * inv;
        }
    return g;
}

/* reduce a float image by k */
static float *reduce(const float *g, int w, int h, int k, int *rw, int *rh)
{
    int W = w / k, H = h / k;
    *rw = W;
    *rh = H;
    if (W <= 0 || H <= 0)
        return NULL;
    float *r = malloc(sizeof(float) * (size_t)W * H);
    if (!r)
        return NULL;
    float inv = 1.0f / (k * k);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float s = 0;
            for (int dy = 0; dy < k; dy++)
                for (int dx = 0; dx < k; dx++)
                    s += g[(size_t)(y * k + dy) * w + x * k + dx];
            r[(size_t)y * W + x] = s * inv;
        }
    return r;
}

/* circle mask of a square of side s: offsets with the image stride, values */
static int build(const float *img, int s, float core, int **off_out, float **t_out, float *tss_out)
{
    double c = (s - 1) / 2.0, r = s / 2.0 * core;
    int n = 0;
    int *off = malloc(sizeof(int) * (size_t)s * s);
    float *t = malloc(sizeof(float) * (size_t)s * s);
    double mean = 0;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++)
            if ((x - c) * (x - c) + (y - c) * (y - c) <= r * r) {
                off[n] = y * s + x;
                t[n] = img[y * s + x];
                mean += t[n];
                n++;
            }
    mean /= n > 0 ? n : 1;
    double ss = 0;
    for (int i = 0; i < n; i++) {
        t[i] -= (float)mean;
        ss += (double)t[i] * t[i];
    }
    *off_out = off;
    *t_out = t;
    *tss_out = (float)ss;
    return n;
}

int pattern_init(Pattern *p, const unsigned char *rgb, int stride, int size, int f, float core)
{
    memset(p, 0, sizeof *p);
    if (core < 0.5f)
        core = 0.5f;
    if (core > 1.0f)
        core = 1.0f;
    p->core = core;
    if (f != 1 && f != 2 && f != 4)
        f = 2;
    if (size < 8)
        return 0;
    p->size = size;
    p->f = f;
    int gw, gh;
    float *g = to_gray(rgb, stride, size, size, f, &gw, &gh);
    if (!g || gw < 4)
        return 0;
    p->ts = gw;
    p->n = build(g, gw, core, &p->off, &p->t, &p->tss);

    p->k = p->ts / 12;
    if (p->k < 1)
        p->k = 1;
    int cw, ch;
    float *cg = reduce(g, gw, gh, p->k, &cw, &ch);
    p->cts = cw;
    p->cn = build(cg, cw, core, &p->coff, &p->ct, &p->ctss);
    free(cg);
    free(g);
    int fw1, fh1;
    float *g1 = to_gray(rgb, stride, size, size, 1, &fw1, &fh1);
    p->n1 = build(g1, size, core, &p->off1, &p->t1, &p->tss1);
    free(g1);

    double c = (size - 1) / 2.0, r = size / 2.0 * core, s[3] = {0, 0, 0};
    int cnt = 0;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if ((x - c) * (x - c) + (y - c) * (y - c) <= r * r) {
                const unsigned char *q = rgb + (size_t)y * stride + (size_t)x * 3;
                s[0] += q[0];
                s[1] += q[1];
                s[2] += q[2];
                cnt++;
            }
    for (int i = 0; i < 3; i++)
        s[i] /= cnt ? cnt : 1;
    chroma(s, p->rgb);
    return p->n > 0 && p->tss > 0;
}

void pattern_free(Pattern *p)
{
    free(p->off);
    free(p->t);
    free(p->coff);
    free(p->ct);
    free(p->off1);
    free(p->t1);
    memset(p, 0, sizeof *p);
}

/* precomputed image offsets for a given image width */
static int *img_offsets(const int *off, int n, int s, int w)
{
    int *o = malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++)
        o[i] = (off[i] / s) * w + (off[i] % s);
    return o;
}

static float ncc_fast(const float *img, int w, int x, int y, int n, const int *ioff, const float *t, float tss)
{
    const float *base = img + (size_t)y * w + x;
    double si = 0, sii = 0, sit = 0;
    for (int i = 0; i < n; i++) {
        float v = base[ioff[i]];
        si += v;
        sii += (double)v * v;
        sit += (double)v * t[i];
    }
    double var = sii - si * si / n;
    if (var < 1e-3 * n || tss <= 0)
        return 0;
    return (float)(sit / sqrt(var * tss));
}

typedef struct {
    int x, y;
    float s;
} Cand;

static int cmp_cand(const void *a, const void *b)
{
    float d = ((const Cand *)b)->s - ((const Cand *)a)->s;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

#define MAX_CAND 64

int pattern_find(const Pattern *p, const unsigned char *rgb, int stride, int w, int h, float min_score, Hit *out,
                 int max, float *best)
{
    if (best)
        *best = 0;
    if (!p || p->n <= 0 || w < p->size || h < p->size)
        return 0;
    int gw, gh;
    float *g = to_gray(rgb, stride, w, h, p->f, &gw, &gh);
    if (!g)
        return 0;
    int cw, ch;
    float *cg = p->k > 1 ? reduce(g, gw, gh, p->k, &cw, &ch) : NULL;
    const float *cimg = cg ? cg : g;
    if (!cg) {
        cw = gw;
        ch = gh;
    }
    int cts = p->k > 1 ? p->cts : p->ts;
    int cn = p->k > 1 ? p->cn : p->n;
    const int *coff = p->k > 1 ? p->coff : p->off;
    const float *ct = p->k > 1 ? p->ct : p->t;
    float ctss = p->k > 1 ? p->ctss : p->tss;

    /* coarse pass: local maxima above a lowered threshold */
    float cthr = min_score - 0.25f;
    if (cthr < 0.2f)
        cthr = 0.2f;
    int nx = cw - cts + 1, ny = ch - cts + 1;
    Cand cand[MAX_CAND];
    int nc = 0;
    float cbest = 0;
    if (nx > 0 && ny > 0) {
        float *map = malloc(sizeof(float) * (size_t)nx * ny);
        int *io = img_offsets(coff, cn, cts, cw);
        for (int y = 0; y < ny; y++)
            for (int x = 0; x < nx; x++) {
                float s = ncc_fast(cimg, cw, x, y, cn, io, ct, ctss);
                map[(size_t)y * nx + x] = s;
                if (s > cbest)
                    cbest = s;
            }
        free(io);
        int r = cts / 2 > 1 ? cts / 2 : 1;
        for (int y = 0; y < ny; y++)
            for (int x = 0; x < nx; x++) {
                float s = map[(size_t)y * nx + x];
                if (s < cthr)
                    continue;
                int is_max = 1;
                for (int dy = -r; dy <= r && is_max; dy++)
                    for (int dx = -r; dx <= r; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if ((dx || dy) && xx >= 0 && yy >= 0 && xx < nx && yy < ny) {
                            float o = map[(size_t)yy * nx + xx];
                            if (o > s || (o == s && (dy < 0 || (dy == 0 && dx < 0)))) {
                                is_max = 0;
                                break;
                            }
                        }
                    }
                if (!is_max)
                    continue;
                if (nc < MAX_CAND)
                    cand[nc++] = (Cand){x, y, s};
                else {
                    int wi = 0;
                    for (int i = 1; i < nc; i++)
                        if (cand[i].s < cand[wi].s)
                            wi = i;
                    if (s > cand[wi].s)
                        cand[wi] = (Cand){x, y, s};
                }
            }
        free(map);
    }
    qsort(cand, (size_t)nc, sizeof(Cand), cmp_cand);
    if (nc > 16)
        nc = 16;

    /* fine pass around every candidate at working resolution */
    int fx = gw - p->ts + 1, fy = gh - p->ts + 1;
    int *fo = img_offsets(p->off, p->n, p->ts, gw);
    Cand fine[MAX_CAND];
    int nf = 0;
    float fbest = 0;
    Cand top = {0, 0, -2};
    int K = p->k;
    for (int i = 0; i < nc; i++) {
        int x0 = cand[i].x * K, y0 = cand[i].y * K;
        Cand b = {x0, y0, -2};
        for (int y = y0 - K; y <= y0 + K; y++)
            for (int x = x0 - K; x <= x0 + K; x++) {
                if (x < 0 || y < 0 || x >= fx || y >= fy)
                    continue;
                float s = ncc_fast(g, gw, x, y, p->n, fo, p->t, p->tss);
                if (s > b.s)
                    b = (Cand){x, y, s};
            }
        if (b.s > fbest) {
            fbest = b.s;
            top = b;
        }
        if (b.s >= min_score)
            fine[nf++] = b;
    }
    free(fo);
    /* the best candidate is always refined, so that *best is exact */
    if (nc > 0 && top.s > -2 && top.s < min_score && nf < MAX_CAND)
        fine[nf++] = top;

    /* final refinement at full resolution: exact sub-sample position and score */
    if (p->f > 1) {
        int f = p->f, S = p->size, P = S + 2 * f;
        float *patch = malloc(sizeof(float) * (size_t)P * P);
        int *po = img_offsets(p->off1, p->n1, S, P);
        fbest = 0;
        for (int i = 0; i < nf; i++) {
            int bx = fine[i].x * f - f, by = fine[i].y * f - f;
            for (int y = 0; y < P; y++)
                for (int x = 0; x < P; x++) {
                    int ix = bx + x, iy = by + y;
                    if (ix < 0)
                        ix = 0;
                    if (iy < 0)
                        iy = 0;
                    if (ix >= w)
                        ix = w - 1;
                    if (iy >= h)
                        iy = h - 1;
                    const unsigned char *q = rgb + (size_t)iy * stride + (size_t)ix * 3;
                    patch[y * P + x] = 0.299f * q[0] + 0.587f * q[1] + 0.114f * q[2];
                }
            Cand b = {fine[i].x * f, fine[i].y * f, -2};
            for (int y = 0; y <= 2 * f; y++)
                for (int x = 0; x <= 2 * f; x++) {
                    int ax = bx + x, ay = by + y;
                    if (ax < 0 || ay < 0 || ax + S > w || ay + S > h)
                        continue;
                    float sc = ncc_fast(patch, P, x, y, p->n1, po, p->t1, p->tss1);
                    if (sc > b.s)
                        b = (Cand){ax, ay, sc};
                }
            fine[i] = b; /* now in full-resolution top-left coordinates */
            if (b.s > fbest)
                fbest = b.s;
        }
        free(po);
        free(patch);
        /* drop candidates that fell below the threshold after refinement */
        int k2 = 0;
        for (int i = 0; i < nf; i++)
            if (fine[i].s >= min_score)
                fine[k2++] = fine[i];
        nf = k2;
    }
    if (p->f == 1) { /* working resolution is full resolution: drop the extra best candidate */
        int k1 = 0;
        for (int i = 0; i < nf; i++)
            if (fine[i].s >= min_score)
                fine[k1++] = fine[i];
        nf = k1;
    }
    if (best)
        *best = nc > 0 ? (fbest > 0 ? fbest : 0) : (cbest < 0 ? 0 : cbest * 0.9f);
    qsort(fine, (size_t)nf, sizeof(Cand), cmp_cand);

    /* suppress duplicates, color check, output in screen pixels */
    int nout = 0;
    int rad = p->size / 2;
    for (int i = 0; i < nf && nout < max; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++)
            if (abs(fine[j].x - fine[i].x) <= rad && abs(fine[j].y - fine[i].y) <= rad && fine[j].s >= fine[i].s)
                dup = 1;
        if (dup)
            continue;
        int f = p->f;
        int px = fine[i].x, py = fine[i].y, size = p->size;
        double c = (size - 1) / 2.0, r = size / 2.0 * p->core, s3[3] = {0, 0, 0};
        int cnt = 0;
        for (int y = 0; y < size; y += f)
            for (int x = 0; x < size; x += f) {
                if ((x - c) * (x - c) + (y - c) * (y - c) > r * r)
                    continue;
                int ix = px + x, iy = py + y;
                if (ix >= w || iy >= h)
                    continue;
                const unsigned char *q = rgb + (size_t)iy * stride + (size_t)ix * 3;
                s3[0] += q[0];
                s3[1] += q[1];
                s3[2] += q[2];
                cnt++;
            }
        for (int k = 0; k < 3; k++)
            s3[k] /= cnt ? cnt : 1;
        float hc[3];
        chroma(s3, hc);
        double d = 0;
        for (int k = 0; k < 3; k++) {
            double m = (hc[k] - p->rgb[k]) * 100.0;
            d += m * m;
        }
        out[nout].x = px + size / 2;
        out[nout].y = py + size / 2;
        out[nout].score = fine[i].s;
        out[nout].color = (float)sqrt(d);
        nout++;
    }
    free(g);
    free(cg);
    return nout;
}
