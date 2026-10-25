/* SH4DOW-DC

   test/shade_test.c
   Copyright (C) 2026 Emerald May

   Host check of the plain C path: fixed points on one column, a grid and
   random frames against the reference formula (every column, no early
   outs), the ball and capsule lists against sh4dow_strength() with a
   shrunk-ball control that must fail, and the shade's direction.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4dow_dc.h"

#define TOL  0.0002f            /* strength: float rounding of two formulations */

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

static float ref_smooth(float x, float e0, float e1) {
    float t;

    if(e0 == e1)
        return x < e0 ? 0.0f : 1.0f;
    t = (x - e0) / (e1 - e0);
    if(t <= 0.0f)
        return 0.0f;
    if(t >= 1.0f)
        return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* The shade by the README's formula: every column, no reciprocals, no early outs. */
static float ref_strength(const sh4dow_frame_t *f, float vt, float vu, float vw, unsigned own) {
    float dark = 0.0f;
    int j;

    for(j = 0; j < f->count; j++) {
        const sh4dow_column_t *c = &f->column[j];
        const float t = vt - c->lt - c->from, du = vu - c->lu, dw = vw - c->lw;
        float perp2, fade, s;

        if(t <= 0.0f || t >= f->length || (c->bone != SH4DOW_BONE_NONE && own == c->bone))
            continue;
        perp2 = du * du + dw * dw;
        if(perp2 >= c->outer2)
            continue;
        fade = t * c->top;
        fade = fade < 0.0f ? 0.0f : fade > 1.0f ? 1.0f : fade;
        s = (1.0f - ref_smooth(sqrtf(perp2), c->inner, c->outer)) * fade *
            (1.0f - ref_smooth(t, f->hold * f->length, f->length));
        if(s > dark)
            dark = s;
    }

    return dark;
}

static float ref_at(const sh4dow_frame_t *f, const float p[3], unsigned own) {
    return ref_strength(f, p[0] * f->dir[0] + p[1] * f->dir[1] + p[2] * f->dir[2],
                           p[0] * f->u[0] + p[1] * f->u[1] + p[2] * f->u[2],
                           p[0] * f->w[0] + p[1] * f->w[1] + p[2] * f->w[2], own);
}

/* Colour of a white vertex after sh4dow_shade(). */
static float shaded(const sh4dow_frame_t *f, float x, float y, float z, uint8_t bone) {
    const float xyz[3] = { x, y, z };
    float rgb[3] = { 1.0f, 1.0f, 1.0f };

    sh4dow_shade(f, xyz, sizeof(xyz), &bone, 1, rgb, sizeof(rgb), 1);
    return rgb[0];
}

static unsigned lcg = 12345u;

static float rnd(void) {
    lcg = lcg * 1664525u + 1013904223u;
    return (float)(lcg >> 8) * (1.0f / 16777216.0f);
}

/* A uniform point in the unit ball (rejection). */
static void rnd_ball(float v[3]) {
    do {
        v[0] = rnd() * 2.0f - 1.0f;
        v[1] = rnd() * 2.0f - 1.0f;
        v[2] = rnd() * 2.0f - 1.0f;
    } while(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] > 1.0f);
}

static void rnd_limbs(sh4dow_limb_t *limb, int n) {
    int i;

    for(i = 0; i < n; i++) {
        limb[i].position[0] = (rnd() - 0.5f) * 0.6f;
        limb[i].position[1] = (rnd() - 0.5f) * 0.6f;
        limb[i].position[2] = rnd() * 0.7f;
        limb[i].radius = 0.02f + rnd() * 0.08f;
        limb[i].bone = (uint8_t)i;
        limb[i].flags = i == 0 ? SH4DOW_LIMB_HEAD : i == 1 ? SH4DOW_LIMB_NECK : 0;
    }
}

static void rnd_light(float dir[3]) {
    dir[0] = (rnd() - 0.5f) * 1.4f;
    dir[1] = (rnd() - 0.5f) * 1.4f;
    dir[2] = -0.4f - rnd();
}

/* One unit column at the origin, light straight down: core, feather, fade-out, lit side, own bone. */
static int single(sh4dow_frame_t *f) {
    const float down[3] = { 0.0f, 0.0f, -1.0f };
    sh4dow_limb_t limb = { { 0.0f, 0.0f, 0.0f }, 1.0f, 3, 0 };
    float y, z;

    if(sh4dow_prepare(NULL, NULL, &limb, 1, down) != 0)
        return fail("null frame");
    if(sh4dow_prepare(f, NULL, &limb, 1, down) != 1)
        return fail("prepare");
    if(!near(shaded(f, 0.0f, 0.0f, -1.0f, 1), 1.0f - SH4DOW_DARK, 0.0001f))
        return fail("core");
    if(!near(shaded(f, 1.30f, 0.0f, -1.0f, 1), 1.0f - SH4DOW_DARK * 0.5f, TOL))
        return fail("feather middle");
    if(!near(shaded(f, 0.0f, 0.0f, -1.62f, 1), 1.0f - SH4DOW_DARK * 0.5f, TOL))
        return fail("fade-out middle");
    if(!near(shaded(f, 0.0f, 0.0f, 1.0f, 1), 1.0f, 0.0001f))
        return fail("lit side");
    if(!near(shaded(f, 0.0f, 0.0f, -1.0f, 3), 1.0f, 0.0001f))
        return fail("own bone");

    for(z = -0.2f; z <= SH4DOW_LENGTH + 0.2f; z += 0.15f) {
        for(y = 0.0f; y <= SH4DOW_OUTER + 0.15f; y += 0.05f) {
            const float p[3] = { y, 0.0f, -z };

            if(!near(shaded(f, p[0], p[1], p[2], 1), 1.0f - f->dark * ref_at(f, p, 1), TOL)) {
                fprintf(stderr, "FAIL grid y %.3f z %.3f\n", y, z);
                return 1;
            }
        }
    }

    return 0;
}

/* A second column beside the first still shades the first's own bone; bad params store nothing. */
static int params(sh4dow_frame_t *f) {
    const float down[3] = { 0.0f, 0.0f, -1.0f };
    sh4dow_limb_t limb[2] = { { { 0.0f, 0.0f, 0.0f }, 1.0f, 3, 0 }, { { 0.2f, 0.0f, 0.0f }, 1.0f, 4, 0 } };
    sh4dow_params_t p;

    if(sh4dow_prepare(f, NULL, limb, 2, down) != 2)
        return fail("two columns");
    if(!near(shaded(f, 0.0f, 0.0f, -1.0f, 3), 1.0f - SH4DOW_DARK, 0.02f))
        return fail("other column");

    sh4dow_params_default(&p);
    p.length = 0.0f;
    if(sh4dow_prepare(f, &p, limb, 1, down) != 0)
        return fail("zero length");
    p.length = SH4DOW_LENGTH;
    p.outer_scale = p.inner_scale;
    if(sh4dow_prepare(f, &p, limb, 1, down) != 0)
        return fail("no feather");
    p.outer_scale = SH4DOW_OUTER;
    p.dark = 2.0f;
    if(sh4dow_prepare(f, &p, limb, 1, down) != 1 || f->dark != 1.0f)
        return fail("dark clamp");
    return 0;
}

/* Sorted columns and the early outs against the reference: 14 random limbs, a tilted light, random vertices and bones. */
static int sorted_random(void) {
    const float dir[3] = { 0.3f, -0.2f, -1.0f };
    sh4dow_frame_t f;
    sh4dow_limb_t limb[14];
    int trial, i;

    for(trial = 0; trial < 50; trial++) {
        rnd_limbs(limb, 14);
        if(sh4dow_prepare(&f, NULL, limb, 14, dir) != 14)
            return fail("random prepare");
        for(i = 1; i < f.count; i++)
            if(f.column[i - 1].t0 > f.column[i].t0)
                return fail("columns not sorted by start");
        for(i = 0; i < 200; i++) {
            const uint8_t bone = (uint8_t)(rnd() * 16.0f);
            float p[3];

            p[0] = (rnd() - 0.5f) * 0.7f;
            p[1] = (rnd() - 0.5f) * 0.7f;
            p[2] = rnd() * 0.8f;
            if(!near(shaded(&f, p[0], p[1], p[2], bone), 1.0f - f.dark * ref_at(&f, p, bone), TOL)) {
                fprintf(stderr, "FAIL random trial %d vertex %d\n", trial, i);
                return 1;
            }
        }
    }

    return 0;
}

/* The list's order: FULL entries first, the rest by start, never the own bone. */
static int list_order(const sh4dow_frame_t *f, const uint8_t *list, unsigned n, unsigned own) {
    unsigned k, full = 0;

    for(k = 0; k < n; k++) {
        const unsigned j = list[k] & SH4DOW_LIST_INDEX;

        if(own != SH4DOW_BONE_NONE && f->column[j].bone == own)
            return fail("own bone listed");
        if(list[k] & SH4DOW_LIST_FULL) {
            if(k != full++)
                return fail("FULL entries do not lead");
        }
        else if(k + 1 < n && !(list[k + 1] & SH4DOW_LIST_FULL) &&
                f->t0r[j] > f->t0r[list[k + 1] & SH4DOW_LIST_INDEX]) {
            return fail("general entries not in start order");
        }
    }

    return 0;
}

/* Ball lists: random frames (some far from the origin), random balls and points inside them; the list strength must
   match sh4dow_strength() within TOL and 1/255 of the colour. shrink: list a half-size ball, which must be caught. */
static int ball_random(int shrink) {
    sh4dow_frame_t f;
    sh4dow_axes_t axes;
    sh4dow_limb_t limb[14];
    unsigned verts = 0, listed = 0, full = 0, above = 0;
    int trial, b, i, k, miss = 0;

    for(trial = 0; trial < 60; trial++) {
        float dir[3], off[3];

        rnd_limbs(limb, 14);
        rnd_light(dir);
        for(k = 0; k < 3; k++)
            off[k] = (rnd() - 0.5f) * 20.0f;
        for(i = 0; i < 14; i++)
            for(k = 0; k < 3; k++)
                limb[i].position[k] += off[k];
        if(sh4dow_prepare(&f, NULL, limb, 14, dir) != 14)
            return fail("ball prepare");
        axes = sh4dow_axes(&f);

        for(b = 0; b < 40; b++) {
            uint8_t list[SH4DOW_COLUMN_MAX];
            unsigned n, own = (unsigned)(rnd() * 16.0f);
            float c[3], r, pc[3];

            if(own >= 14)
                own = SH4DOW_BONE_NONE;
            c[0] = off[0] + (rnd() - 0.5f) * 0.8f;
            c[1] = off[1] + (rnd() - 0.5f) * 0.8f;
            c[2] = off[2] + rnd() * 0.9f - 0.1f;
            r = 0.01f + rnd() * 0.2f;
            sh4dow_project(&f, c[0], c[1], c[2], pc);
            n = sh4dow_ball_columns(&f, pc[0], pc[1], pc[2], shrink ? r * 0.5f : r, own, list);
            if(list_order(&f, list, n, own))
                return 1;
            listed += n;
            for(k = 0; k < (int)n; k++)
                full += (list[k] & SH4DOW_LIST_FULL) != 0;

            for(i = 0; i < 25; i++) {
                float v[3], q[3], a, d, vt;

                rnd_ball(v);
                for(k = 0; k < 3; k++)
                    v[k] = c[k] + v[k] * r;
                sh4dow_project(&f, v[0], v[1], v[2], q);
                a = sh4dow_strength(&f, &axes, v[0], v[1], v[2], own);
                d = sh4dow_strength_list(&f, q[0], q[1], q[2], list, n);
                if(!near(a, ref_at(&f, v, own), TOL))
                    return fail("ball: strength against the reference");
                if(!near(a, d, TOL))
                    miss++;
                if(!shrink && fabsf(floorf((1.0f - f.dark * a) * 255.0f + 0.5f) -
                                         floorf((1.0f - f.dark * d) * 255.0f + 0.5f)) > 1.0f)
                    return fail("ball: colour off by more than 1/255");
                vt = v[0] * f.dir[0] + v[1] * f.dir[1] + v[2] * f.dir[2];
                for(k = 0; k < f.count; k++)
                    above += vt - f.column[k].t0 > 0.0f;
                verts++;
            }
        }
    }

    if(shrink)
        return miss ? 0 : fail("a shrunk ball was not caught");
    if(miss) {
        fprintf(stderr, "FAIL ball: %d mismatches\n", miss);
        return 1;
    }
    printf("ball lists: %u vertices, columns above %.2f, listed %.2f a vertex (%.0f%% FULL)\n",
           verts, (double)above / verts, (double)listed * 25.0 / verts, 100.0 * full / (listed ? listed : 1));
    return 0;
}

/* Capsule lists: points on segments between a point of each of two balls (a two-bone blend); shrink as above. */
static int capsule_random(int shrink) {
    sh4dow_frame_t f;
    sh4dow_axes_t axes;
    sh4dow_limb_t limb[14];
    unsigned verts = 0, listed = 0;
    int trial, b, i, k, miss = 0;

    for(trial = 0; trial < 40; trial++) {
        float dir[3];

        rnd_limbs(limb, 14);
        rnd_light(dir);
        if(sh4dow_prepare(&f, NULL, limb, 14, dir) != 14)
            return fail("capsule prepare");
        axes = sh4dow_axes(&f);

        for(b = 0; b < 40; b++) {
            uint8_t list[SH4DOW_COLUMN_MAX];
            const unsigned own = (unsigned)(rnd() * 14.0f);
            float c0[3], c1[3], r0, r1, p0[3], p1[3];
            unsigned n;

            for(k = 0; k < 3; k++) {
                c0[k] = (rnd() - 0.5f) * 0.8f;
                c1[k] = c0[k] + (rnd() - 0.5f) * 0.3f;
            }
            c0[2] += 0.3f;
            c1[2] += 0.3f;
            r0 = 0.01f + rnd() * 0.15f;
            r1 = 0.01f + rnd() * 0.15f;
            sh4dow_project(&f, c0[0], c0[1], c0[2], p0);
            sh4dow_project(&f, c1[0], c1[1], c1[2], p1);
            n = sh4dow_capsule_columns(&f, p0, shrink ? r0 * 0.3f : r0, p1, shrink ? r1 * 0.3f : r1, own, list);
            if(list_order(&f, list, n, own))
                return 1;
            listed += n;

            for(i = 0; i < 25; i++) {
                const float w = rnd();
                float u0[3], u1[3], v[3], q[3];

                rnd_ball(u0);
                rnd_ball(u1);
                for(k = 0; k < 3; k++)
                    v[k] = w * (c0[k] + u0[k] * r0) + (1.0f - w) * (c1[k] + u1[k] * r1);
                sh4dow_project(&f, v[0], v[1], v[2], q);
                if(!near(sh4dow_strength(&f, &axes, v[0], v[1], v[2], own),
                         sh4dow_strength_list(&f, q[0], q[1], q[2], list, n), TOL))
                    miss++;
                verts++;
            }
        }
    }

    if(shrink)
        return miss ? 0 : fail("a shrunk capsule was not caught");
    if(miss) {
        fprintf(stderr, "FAIL capsule: %d mismatches\n", miss);
        return 1;
    }
    printf("capsule lists: %u blended vertices, listed %.2f a vertex\n", verts, (double)listed * 25.0 / verts);
    return 0;
}

/* A hand-made Z-up figure: the shade falls away from the light, straight down and leaning; the chin stays lit; a
   mirrored figure (a negative placement scale) mirrors the result. */
static int direction(void) {
    const float down[3] = { 0.0f, 0.0f, -1.0f }, lean[3] = { 0.6f, 0.0f, -0.8f };
    sh4dow_frame_t f;
    sh4dow_axes_t a;
    int mirror;

    for(mirror = 0; mirror < 2; mirror++) {
        const float sx = mirror ? -1.0f : 1.0f;
        const sh4dow_limb_t limb[4] = {
            { { 0.0f,       0.0f, 1.70f }, 0.10f, 12, SH4DOW_LIMB_HEAD },
            { { 0.0f,       0.0f, 1.55f }, 0.05f,  9, SH4DOW_LIMB_NECK },
            { { 0.0f,       0.0f, 1.20f }, 0.18f,  6, 0 },
            { { sx * 0.30f, 0.0f, 1.30f }, 0.05f, 13, 0 },
        };

        if(sh4dow_prepare(&f, NULL, limb, 4, down) != 4)
            return fail("direction prepare (down)");
        a = sh4dow_axes(&f);
        /* The head column starts at the neck centre + 0.8 neck radii, z 1.51. */
        if(!(sh4dow_strength(&f, &a, 0.0f, 0.0f, 1.00f, 6) > 0.99f))
            return fail("down: the chest under the head is lit");
        if(sh4dow_strength(&f, &a, 0.0f, 0.0f, 1.90f, 0) != 0.0f)
            return fail("down: above the head is shaded");
        if(sh4dow_strength(&f, &a, 0.0f, 0.0f, 1.60f, 6) != 0.0f)
            return fail("down: the chin is shaded");
        if(!(sh4dow_strength(&f, &a, sx * 0.30f, 0.0f, 1.00f, 6) > 0.99f))
            return fail("down: under the arm is lit");
        if(sh4dow_strength(&f, &a, sx * 0.30f, 0.0f, 1.50f, 6) != 0.0f)
            return fail("down: above the arm is shaded");

        if(sh4dow_prepare(&f, NULL, limb, 4, lean) != 4)
            return fail("direction prepare (lean)");
        a = sh4dow_axes(&f);
        /* 0.5 along (0.6, 0, -0.8) from the arm is (+0.3, 0, -0.4). */
        if(!(sh4dow_strength(&f, &a, sx * 0.30f + 0.30f, 0.0f, 0.90f, 6) > 0.99f))
            return fail("lean: along the light from the arm is lit");
        if(sh4dow_strength(&f, &a, sx * 0.30f - 0.30f, 0.0f, 1.70f, 6) != 0.0f)
            return fail("lean: toward the light from the arm is shaded");
        if(sh4dow_strength(&f, &a, sx * 0.30f, 0.0f, 0.90f, 6) != 0.0f)
            return fail("lean: straight under the arm is shaded");
    }

    return 0;
}

int main(void) {
    sh4dow_frame_t f;

    if(single(&f) || params(&f) || sorted_random() ||
       ball_random(0) || ball_random(1) || capsule_random(0) || capsule_random(1) || direction())
        return 1;
    printf("ok\n");
    return 0;
}
