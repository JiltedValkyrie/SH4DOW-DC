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

/* strength: float rounding of two formulations. The list loop expands the disc test's square, so on random columns far thinner than their distance from the
   frame's origin it can differ by a little more (the README's note): -DSH4DOW_TEST_TOL=... widens it for a build whose rounding differs. */
#ifndef SH4DOW_TEST_TOL
#define SH4DOW_TEST_TOL 0.0002f
#endif
#define TOL  SH4DOW_TEST_TOL

/* The packed list against sh4dow_strength(): both test the plain distance, so only the projection's rounding
   (about 1e-5 on the plain C path); a build with approximate roots needs TOL. */
#ifndef SH4DOW_TEST_PACKED_TOL
#define SH4DOW_TEST_PACKED_TOL TOL
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

static unsigned packed_solid, packed_empty, packed_balls;
static float packed_worst;

/* sh4dow_strength_packed() against sh4dow_strength() (every column, plain distance); a solid pack must read 1,
   an empty one 0. */
static int packed_check(const sh4dow_packed_t *p, float every, const float q[3]) {
    const float s = sh4dow_strength_packed(p, q[0], q[1], q[2]);

    if(fabsf(s - every) > packed_worst)
        packed_worst = fabsf(s - every);
    if(!near(s, every, SH4DOW_TEST_PACKED_TOL))
        return fail("packed: strength against every column");
    if(p->solid && !near(every, 1.0f, SH4DOW_TEST_PACKED_TOL))
        return fail("packed: a solid ball with a vertex below full strength");
    if(!p->count && every != 0.0f)
        return fail("packed: an empty list with a shaded vertex");
    return 0;
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
            sh4dow_packed_t pk;
            float batch_xyz[25][3], batch_one[25], batch_out[25];
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
            if(sh4dow_ball_pack(&f, pc[0], pc[1], pc[2], shrink ? r * 0.5f : r, own, &pk) != n)
                return fail("ball pack: count");
            if(!shrink) {
                packed_balls++;
                packed_solid += pk.solid;
                packed_empty += !pk.count;
            }
            listed += n;
            for(k = 0; k < (int)n; k++)
                full += (list[k] & SH4DOW_LIST_FULL) != 0;

            for(i = 0; i < 25; i++) {
                float v[3], q[3], a, d, vt;

                rnd_ball(v);
                for(k = 0; k < 3; k++)
                    batch_xyz[i][k] = v[k] = c[k] + v[k] * r;
                sh4dow_project(&f, v[0], v[1], v[2], q);
                a = sh4dow_strength(&f, &axes, v[0], v[1], v[2], own);
                d = sh4dow_strength_list(&f, q[0], q[1], q[2], list, n);
                if(!shrink && packed_check(&pk, a, q))
                    return 1;
                batch_one[i] = sh4dow_strength_packed(&pk, q[0], q[1], q[2]);
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
            if(!shrink) {
                sh4dow_strength_packed_n(&f, &pk, batch_xyz[0], sizeof batch_xyz[0], batch_out, 25);
                for(i = 0; i < 25; i++)
                    if(!near(batch_out[i], pk.solid ? 1.0f : batch_one[i], TOL))
                        return fail("packed_n: against sh4dow_strength_packed()");
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
            sh4dow_packed_t pk;
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
            if(sh4dow_capsule_pack(&f, p0, shrink ? r0 * 0.3f : r0, p1, shrink ? r1 * 0.3f : r1, own, &pk) != n)
                return fail("capsule pack: count");
            listed += n;

            for(i = 0; i < 25; i++) {
                const float w = rnd();
                float u0[3], u1[3], v[3], q[3], a;

                rnd_ball(u0);
                rnd_ball(u1);
                for(k = 0; k < 3; k++)
                    v[k] = w * (c0[k] + u0[k] * r0) + (1.0f - w) * (c1[k] + u1[k] * r1);
                sh4dow_project(&f, v[0], v[1], v[2], q);
                a = sh4dow_strength(&f, &axes, v[0], v[1], v[2], own);
                if(!near(a, sh4dow_strength_list(&f, q[0], q[1], q[2], list, n), TOL))
                    miss++;
                if(!shrink && packed_check(&pk, a, q))
                    return 1;
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

/* One fat column straight down: a ball deep in its core packs solid and reads 1 at every point; one that
   pokes into the feather, or lies past the hold, does not; one off to the side packs empty. */
static int packed_solid_case(void) {
    const float down[3] = { 0.0f, 0.0f, -1.0f };
    sh4dow_limb_t limb = { { 0.0f, 0.0f, 0.0f }, 1.0f, 3, 0 };
    sh4dow_frame_t f;
    sh4dow_packed_t pk;
    float c[3];
    int i, k;

    if(sh4dow_prepare(&f, NULL, &limb, 1, down) != 1)
        return fail("solid prepare");
    sh4dow_project(&f, 0.3f, 0.0f, -0.8f, c);
    sh4dow_ball_pack(&f, c[0], c[1], c[2], 0.1f, SH4DOW_BONE_NONE, &pk);
    if(pk.count != 1 || !pk.solid)
        return fail("solid: a ball deep in the core");
    for(i = 0; i < 200; i++) {
        float v[3], q[3];

        rnd_ball(v);
        sh4dow_project(&f, 0.3f + v[0] * 0.1f, v[1] * 0.1f, -0.8f + v[2] * 0.1f, q);
        if(sh4dow_strength_packed(&pk, q[0], q[1], q[2]) != 1.0f)
            return fail("solid: a vertex below 1");
    }
    sh4dow_project(&f, 1.2f, 0.0f, -0.8f, c);
    sh4dow_ball_pack(&f, c[0], c[1], c[2], 0.1f, SH4DOW_BONE_NONE, &pk);
    if(pk.count != 1 || pk.solid)
        return fail("solid: a ball across the feather");
    sh4dow_project(&f, 0.0f, 0.0f, -1.6f, c);
    sh4dow_ball_pack(&f, c[0], c[1], c[2], 0.1f, SH4DOW_BONE_NONE, &pk);
    if(pk.count != 1 || pk.solid || pk.full)
        return fail("solid: a ball in the fade-out");
    sh4dow_project(&f, 3.0f, 0.0f, -0.8f, c);
    sh4dow_ball_pack(&f, c[0], c[1], c[2], 0.1f, SH4DOW_BONE_NONE, &pk);
    if(pk.count || pk.solid)
        return fail("solid: a ball beside the column");
    sh4dow_project(&f, 0.0f, 0.0f, -0.8f, c);
    sh4dow_ball_pack(&f, c[0], c[1], c[2], 0.1f, 3, &pk);
    if(pk.count || pk.solid)
        return fail("solid: the column's own bone");
    for(k = 0; k < 3; k++)
        c[k] = 0.0f;
    if(sh4dow_pack(&f, NULL, 0, &pk) || pk.count || sh4dow_ball_pack(NULL, 0.0f, 0.0f, 0.0f, 1.0f, 0, &pk))
        return fail("pack: NULL");
    return 0;
}

/* A pack of all 32 columns: the Dreamcast walk reads its second entry past the last, inside the pack. Against the
   plain list over the same 32 entries, at points all over the figure. */
static int pack_full_32(void) {
    sh4dow_limb_t limb32[SH4DOW_COLUMN_MAX];
    sh4dow_frame_t f;
    sh4dow_packed_t pk;
    uint8_t list[SH4DOW_COLUMN_MAX];
    float dir[3];
    int i, k, trial;

    for(trial = 0; trial < 20; trial++) {
        rnd_limbs(limb32, 14);
        rnd_limbs(limb32 + 14, 14);
        rnd_limbs(limb32 + 28, 4);
        for(i = 0; i < SH4DOW_COLUMN_MAX; i++) {
            limb32[i].bone = (uint8_t)i;
            limb32[i].flags = 0;
        }
        rnd_light(dir);
        if(sh4dow_prepare(&f, NULL, limb32, SH4DOW_COLUMN_MAX, dir) != SH4DOW_COLUMN_MAX)
            return fail("32 prepare");
        for(i = 0; i < SH4DOW_COLUMN_MAX; i++)
            list[i] = (uint8_t)i;
        if(sh4dow_pack(&f, list, SH4DOW_COLUMN_MAX, &pk) != SH4DOW_COLUMN_MAX || pk.full)
            return fail("32 pack");
        for(i = 0; i < 400; i++) {
            float v[3], q[3];

            for(k = 0; k < 3; k++)
                v[k] = (rnd() - 0.5f) * 0.8f;
            v[2] += 0.35f;
            sh4dow_project(&f, v[0], v[1], v[2], q);
            if(!near(sh4dow_strength_packed(&pk, q[0], q[1], q[2]),
                     sh4dow_strength_list(&f, q[0], q[1], q[2], list, SH4DOW_COLUMN_MAX), TOL))
                return fail("32 pack: against the list");
        }
    }
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

/* sh4dow_project() is the point on the frame's three axes less the origin; on the SH4ZAM path sh4dow_load_xmtrx() and one transform give the same numbers. */
static int projection(void) {
    const sh4dow_limb_t limb[3] = {
        { { 0.3f, -0.2f, 1.7f }, 0.10f, 12, SH4DOW_LIMB_HEAD },
        { { 0.2f, -0.1f, 1.5f }, 0.06f, 9, SH4DOW_LIMB_NECK },
        { { 0.1f, 0.3f, 1.3f }, 0.18f, 6, 0 },
    };
    const float dir[3] = { 0.3f, -0.4f, -0.9f };
    sh4dow_frame_t f;
    unsigned i;

    if(sh4dow_prepare(&f, NULL, limb, 3, dir) != 3)
        return fail("projection prepare");
#ifndef SH4DOW_DC_NO_SH4ZAM
    sh4dow_load_xmtrx(&f);
#endif
    for(i = 0; i < 200; i++) {
        const float p[3] = { -2.0f + 0.021f * (float)i, 1.0f - 0.017f * (float)i, 0.5f + 0.011f * (float)i };
        float q[3], want[3];
        int k;

        sh4dow_project(&f, p[0], p[1], p[2], q);
        want[0] = p[0] * f.dir[0] + p[1] * f.dir[1] + p[2] * f.dir[2] - f.ot;
        want[1] = p[0] * f.u[0] + p[1] * f.u[1] + p[2] * f.u[2] - f.ou;
        want[2] = p[0] * f.w[0] + p[1] * f.w[1] + p[2] * f.w[2] - f.ow;
        for(k = 0; k < 3; k++)
            if(!near(q[k], want[k], 1.0e-5f))
                return fail("sh4dow_project against the axes");
#ifndef SH4DOW_DC_NO_SH4ZAM
        {
            const shz_vec3_t x = shz_xmtrx_transform_point3(shz_vec3_init(p[0], p[1], p[2]));

            if(!near(x.x, q[0], 1.0e-5f) || !near(x.y, q[1], 1.0e-5f) || !near(x.z, q[2], 1.0e-5f))
                return fail("XMTRX against sh4dow_project");
        }
#endif
    }
    /* The side axes for lights along the axes, on the diagonals (ties between the world axes) and near one: cross(light, the world axis least aligned with it),
       then the light crossed with that. */
    {
        static const float ds[][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 }, { 1, 1, 1 }, { -1, -1, 1 }, { 1, 1, 0 }, { 0, 1, 1 }, { 1, 0, 1 },
                                       { 0.5f, 0.5f, 0.7071f }, { 3, -3, 3 }, { 0.001f, 0.002f, -1 }, { 0.4f, 0.1f, -0.2f } };
        unsigned d;

        for(d = 0; d < sizeof(ds) / sizeof(ds[0]); d++) {
            float x = ds[d][0], y = ds[d][1], z = ds[d][2], len = sqrtf(x * x + y * y + z * z), h[3] = { 0, 0, 0 }, c[3], cl, wx, wy, wz;

            x /= len; y /= len; z /= len;
            if(fabsf(x) <= fabsf(y) && fabsf(x) <= fabsf(z))
                h[0] = 1.0f;
            else if(fabsf(y) <= fabsf(z))
                h[1] = 1.0f;
            else
                h[2] = 1.0f;
            c[0] = y * h[2] - z * h[1]; c[1] = z * h[0] - x * h[2]; c[2] = x * h[1] - y * h[0];
            cl = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
            c[0] /= cl; c[1] /= cl; c[2] /= cl;
            wx = y * c[2] - z * c[1]; wy = z * c[0] - x * c[2]; wz = x * c[1] - y * c[0];
            if(sh4dow_prepare(&f, NULL, limb, 3, ds[d]) != 3)
                return fail("axes prepare");
            if(!near(f.dir[0], x, 1.0e-5f) || !near(f.dir[1], y, 1.0e-5f) || !near(f.dir[2], z, 1.0e-5f) ||
               !near(f.u[0], c[0], 1.0e-5f) || !near(f.u[1], c[1], 1.0e-5f) || !near(f.u[2], c[2], 1.0e-5f) ||
               !near(f.w[0], wx, 1.0e-5f) || !near(f.w[1], wy, 1.0e-5f) || !near(f.w[2], wz, 1.0e-5f))
                return fail("the side axes for this light");
        }
    }
    /* The axes are unit and orthogonal for any light, and the rows are the axes. */
    if(!near(f.dir[0] * f.dir[0] + f.dir[1] * f.dir[1] + f.dir[2] * f.dir[2], 1.0f, 1.0e-6f) ||
       !near(f.u[0] * f.dir[0] + f.u[1] * f.dir[1] + f.u[2] * f.dir[2], 0.0f, 1.0e-6f) ||
       !near(f.w[0] * f.u[0] + f.w[1] * f.u[1] + f.w[2] * f.u[2], 0.0f, 1.0e-6f))
        return fail("the frame's axes");
    return 0;
}

int main(void) {
    sh4dow_frame_t f;

    if(single(&f) || params(&f) || sorted_random() ||
       ball_random(0) || ball_random(1) || capsule_random(0) || capsule_random(1) || direction() || projection() ||
       packed_solid_case() || pack_full_32())
        return 1;
    printf("packed lists: within %.1e of every column; %u of %u random balls solid, %u empty\n",
           (double)packed_worst, packed_solid, packed_balls, packed_empty);
    printf("ok\n");
    return 0;
}
