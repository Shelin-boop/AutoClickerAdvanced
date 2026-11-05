/*
 * AutoClicker Advanced - pattern matching
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Normalized cross-correlation of a circular pattern (the circle inscribed
 * in a square), on a grayscale image reduced by a factor of 1, 2 or 4, with a
 * coarse-to-fine search and a color check of every hit.
 */
#ifndef XCA_MATCH_H
#define XCA_MATCH_H

typedef struct {
    int size;  /* side of the square pattern in screen pixels */
    int f;     /* reduction factor 1, 2, 4 */
    int ts;    /* side at working resolution */
    int n;     /* pixels inside the circle at working resolution */
    int *off;  /* offsets (y * ts + x) of the circle pixels */
    float *t;  /* zero-mean pattern values on the circle */
    float tss; /* sum of t^2 */
    /* coarse level (additional reduction k) */
    int k, cts, cn;
    int *coff;
    float *ct;
    float ctss;
    /* full resolution (final refinement) */
    int n1;
    int *off1;
    float *t1;
    float tss1;
    float core;   /* compared part of the radius */
    float rgb[3]; /* mean color on the circle */
} Pattern;

typedef struct {
    int x, y;      /* center in image pixels */
    float score;   /* similarity -1..1 */
    float color;   /* color distance to the pattern, brightness independent, 0..~245 */
} Hit;

/*
 * rgb: 3 bytes per pixel, rows of 'stride' bytes. core: part of the radius of
 * the inscribed circle that is compared (0.5..1.0), so that a background
 * around the pattern does not count. Returns 0 on error.
 */
int pattern_init(Pattern *p, const unsigned char *rgb, int stride, int size, int f, float core);
void pattern_free(Pattern *p);

/*
 * Searches the image and returns up to max hits with score >= min_score,
 * strongest first, at least one pattern radius apart. *best receives the best
 * score found anywhere (also below min_score), for display.
 */
int pattern_find(const Pattern *p, const unsigned char *rgb, int stride, int w, int h, float min_score, Hit *out,
                 int max, float *best);

#endif
