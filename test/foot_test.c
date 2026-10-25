/* SH4DOW-DC

   foot_test.c
   Copyright (C) 2026 Emerald May
   See ../LICENSE.

   Foot cones: the shipped numbers, the cone's geometry on flat, sloped and
   rotated floors, the placement from a foot sphere, the distance fade, the
   picture, and the foot finder. Plain C; prints "ok" on success.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sh4dow_dc.h"

static int fails;

#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while(0)
#define NEAR(a, b, e, ...) do { const float a_ = (a), b_ = (b); if(!(fabsf(a_ - b_) <= (e))) { fails++; printf("FAIL line %d: %.7g vs %.7g: ", __LINE__, a_, b_); printf(__VA_ARGS__); printf("\n"); } } while(0)

static const float UP[3] = { 0.0f, 0.0f, 1.0f };
static const float FWD[3] = { 0.0f, 1.0f, 0.0f };
static const float FLOOR[3] = { 0.0f, 0.0f, 0.0f };

static int finite_quad(float q[4][3]) {
    int i, k;

    for(i = 0; i < 4; i++)
        for(k = 0; k < 3; k++)
            if(!(fabsf(q[i][k]) < 1.0e9f))
                return 0;
    return 1;
}

static sh4dow_foot_t foot_at(float x, float y, float z, float ax, float ay, float az, float radius) {
    sh4dow_foot_t f;

    f.heel[0] = x; f.heel[1] = y; f.heel[2] = z;
    f.away[0] = ax; f.away[1] = ay; f.away[2] = az;
    f.radius = radius;
    return f;
}

static void test_shipped_numbers(void) {
    sh4dow_foot_params_t p;

    /* The numbers the original game ships; the game's own tests pin its copies of them. */
    NEAR(SH4DOW_FOOT_MIN_LEN, 3.0f, 0, "min len");
    NEAR(SH4DOW_FOOT_GAIN, 3.0f, 0, "gain");
    NEAR(SH4DOW_FOOT_MAX_SLOPE, 2.0f, 0, "max slope");
    NEAR(SH4DOW_FOOT_MIN_FACING, 0.25f, 0, "min facing");
    NEAR(SH4DOW_FOOT_HEEL_BACK, 0.7f, 0, "heel back");
    NEAR(SH4DOW_FOOT_WIDTH, 0.8f, 0, "width");
    NEAR(SH4DOW_FOOT_WIDTH_MIN, 0.03f, 0, "width min");
    NEAR(SH4DOW_FOOT_WIDTH_MAX, 0.12f, 0, "width max");
    NEAR(SH4DOW_FOOT_FADE_HEIGHT, 0.35f, 0, "fade height");
    NEAR(SH4DOW_FOOT_LIFT, 0.012f, 0, "lift");
    NEAR(SH4DOW_FOOT_RANGE, 20.0f, 0, "range");
    NEAR(SH4DOW_FOOT_RANGE_FADE, 2.0f, 0, "range fade");
    NEAR(SH4DOW_FOOT_APEX, 0.25f, 0, "apex");
    NEAR(SH4DOW_FOOT_ZONE, 0.25f, 0, "zone");
    CHECK(SH4DOW_NO_FOOT == 255u, "no foot");

    sh4dow_foot_params_default(&p);
    NEAR(p.min_len, SH4DOW_FOOT_MIN_LEN, 0, "p.min_len");
    NEAR(p.gain, SH4DOW_FOOT_GAIN, 0, "p.gain");
    NEAR(p.max_slope, SH4DOW_FOOT_MAX_SLOPE, 0, "p.max_slope");
    NEAR(p.min_facing, SH4DOW_FOOT_MIN_FACING, 0, "p.min_facing");
    NEAR(p.heel_back, SH4DOW_FOOT_HEEL_BACK, 0, "p.heel_back");
    NEAR(p.width, SH4DOW_FOOT_WIDTH, 0, "p.width");
    NEAR(p.fade_height, SH4DOW_FOOT_FADE_HEIGHT, 0, "p.fade_height");
    NEAR(p.lift, SH4DOW_FOOT_LIFT, 0, "p.lift");
    NEAR(p.width_min, SH4DOW_FOOT_WIDTH_MIN, 0, "p.width_min");
    NEAR(p.width_max, SH4DOW_FOOT_WIDTH_MAX, 0, "p.width_max");
    NEAR(p.range, SH4DOW_FOOT_RANGE, 0, "p.range");
    NEAR(p.range_fade, SH4DOW_FOOT_RANGE_FADE, 0, "p.range_fade");

    /* Another unit: the six distances scale, the ratios do not. */
    sh4dow_foot_params_scale(&p, 3.048f);
    NEAR(p.fade_height, 0.35f * 3.048f, 1.0e-6f, "scaled fade height");
    NEAR(p.lift, 0.012f * 3.048f, 1.0e-6f, "scaled lift");
    NEAR(p.width_min, 0.03f * 3.048f, 1.0e-6f, "scaled width min");
    NEAR(p.width_max, 0.12f * 3.048f, 1.0e-6f, "scaled width max");
    NEAR(p.range, 20.0f * 3.048f, 1.0e-4f, "scaled range");
    NEAR(p.range_fade, 2.0f * 3.048f, 1.0e-5f, "scaled range fade");
    NEAR(p.min_len, 3.0f, 0, "unscaled min len");
    NEAR(p.gain, 3.0f, 0, "unscaled gain");
    NEAR(p.max_slope, 2.0f, 0, "unscaled max slope");
    NEAR(p.min_facing, 0.25f, 0, "unscaled min facing");
    NEAR(p.heel_back, 0.7f, 0, "unscaled heel back");
    NEAR(p.width, 0.8f, 0, "unscaled width");
}

static void test_cone_flat_floor(void) {
    const float r = 0.04f, lift = SH4DOW_FOOT_LIFT;
    float q[4][3], fade;
    sh4dow_foot_t f;

    /* A light at 45 degrees, travelling +x and down: slope 1, length (3 + 3) r, along +x; the left side is -y. */
    f = foot_at(1.0f, 2.0f, 0.0f, 1.0f, 0.0f, -1.0f, r);
    fade = sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(fade, 1.0f, 1.0e-6f, "heel on the floor");
    NEAR(q[0][0], 1.0f, 1.0e-6f, "TL x"); NEAR(q[0][1], 2.0f - r, 1.0e-6f, "TL y"); NEAR(q[0][2], lift, 1.0e-6f, "TL z");
    NEAR(q[1][0], 1.0f, 1.0e-6f, "TR x"); NEAR(q[1][1], 2.0f + r, 1.0e-6f, "TR y"); NEAR(q[1][2], lift, 1.0e-6f, "TR z");
    NEAR(q[2][0], 1.0f + 6.0f * r, 1.0e-5f, "BL x"); NEAR(q[2][1], 2.0f - r, 1.0e-6f, "BL y");
    NEAR(q[3][0], 1.0f + 6.0f * r, 1.0e-5f, "BR x"); NEAR(q[3][1], 2.0f + r, 1.0e-6f, "BR y");

    /* The light overhead: the shortest cone (3 r) along the facing. */
    f = foot_at(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, r);
    fade = sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(fade, 1.0f, 1.0e-6f, "overhead fade");
    NEAR(q[2][1], 3.0f * r, 1.0e-6f, "overhead length along the facing");
    NEAR(fabsf(q[1][0] - q[0][0]), 2.0f * r, 1.0e-6f, "overhead width");
    CHECK(finite_quad(q), "overhead quad finite");

    /* A light on the horizon, one that travels up, and none: the longest cone (3 + 3 * 2 = 9 r), the same way. */
    f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.001f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 9.0f * r, 1.0e-5f, "capped at 9 r");
    f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 9.0f * r, 1.0e-5f, "horizontal light");
    f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 9.0f * r, 1.0e-5f, "a light under the floor");
    f = foot_at(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r);
    fade = sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    CHECK(fade > 0.0f && finite_quad(q), "no light at all stays finite");

    /* Just under and just over the cap: slope 1.9 -> 8.7 r, slope 2.5 -> 9 r. */
    f = foot_at(0.0f, 0.0f, 0.0f, 1.9f, 0.0f, -1.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 8.7f * r, 1.0e-5f, "slope 1.9");
    f = foot_at(0.0f, 0.0f, 0.0f, 2.5f, 0.0f, -1.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 9.0f * r, 1.0e-5f, "slope 2.5 is capped at 2");
    /* Slope 0.5 -> 4.5 r. Any length of away. */
    f = foot_at(0.0f, 0.0f, 0.0f, 0.5f, 0.0f, -1.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 4.5f * r, 1.0e-5f, "slope 0.5");
    f = foot_at(0.0f, 0.0f, 0.0f, 50.0f, 0.0f, -100.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(q[2][0], 4.5f * r, 1.0e-4f, "away of any length");

    /* The tip never reaches back toward the light: every corner is at or past the heel along the light's way. */
    f = foot_at(0.0f, 0.0f, 0.0f, -0.3f, 0.4f, -1.0f, r);
    sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    {
        const float len = sqrtf(0.09f + 0.16f);
        const float ux = -0.3f / len, uy = 0.4f / len;
        int i;

        for(i = 0; i < 4; i++)
            CHECK(q[i][0] * ux + q[i][1] * uy >= -1.0e-6f, "corner %d behind the heel", i);
        CHECK(q[2][0] * ux + q[2][1] * uy > 3.0f * r, "far end past the heel");
    }
}

static void test_cone_rejects(void) {
    const float r = 0.04f;
    float q[4][3], n[3], fade;
    sh4dow_foot_t f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, r);
    sh4dow_foot_params_t p;

    /* A wall, a ceiling, a floor just under the 0.25 facing limit, a floor just over it. */
    n[0] = 1.0f; n[1] = 0.0f; n[2] = 0.0f;
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, n, UP, FWD, q) == 0.0f, "wall");
    n[0] = 0.0f; n[2] = -1.0f;
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, n, UP, FWD, q) == 0.0f, "ceiling");
    n[0] = sqrtf(1.0f - 0.0576f); n[2] = 0.24f;
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, n, UP, FWD, q) == 0.0f, "grazing floor");
    n[0] = sqrtf(1.0f - 0.0676f); n[2] = 0.26f;
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, n, UP, FWD, q) > 0.0f, "steep but a floor");
    n[0] = 0.0f; n[1] = 0.0f; n[2] = 9.0f;
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, n, UP, FWD, q) > 0.0f, "a normal of any length");

    /* The heel off the floor: linear fade to nothing at 0.35. */
    f = foot_at(0.0f, 0.0f, 0.175f, 1.0f, 0.0f, -1.0f, r);
    fade = sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q);
    NEAR(fade, 0.5f, 1.0e-5f, "half way up");
    NEAR(q[0][2], SH4DOW_FOOT_LIFT, 1.0e-6f, "the quad stays on the floor");
    f = foot_at(0.0f, 0.0f, 0.35f, 1.0f, 0.0f, -1.0f, r);
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q) == 0.0f, "at the fade height");
    f = foot_at(0.0f, 0.0f, 2.0f, 1.0f, 0.0f, -1.0f, r);
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q) == 0.0f, "high above");

    /* A plane above the heel: a little (the heel sinks into it, full fade), a storey (none). */
    {
        const float above[3] = { 0.0f, 0.0f, 0.1f }, storey[3] = { 0.0f, 0.0f, 1.0f };

        f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, r);
        fade = sh4dow_foot_cone(&f, NULL, above, UP, UP, FWD, q);
        NEAR(fade, 1.0f, 1.0e-6f, "a floor a hair above the heel");
        NEAR(q[0][2], 0.1f + SH4DOW_FOOT_LIFT, 1.0e-6f, "the quad is on that plane");
        CHECK(sh4dow_foot_cone(&f, NULL, storey, UP, UP, FWD, q) == 0.0f, "the plane is a storey up");
    }

    /* No radius, bad params and NULLs. */
    f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f);
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, q) == 0.0f, "no radius");
    f = foot_at(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, r);
    sh4dow_foot_params_default(&p);
    p.fade_height = 0.0f;
    CHECK(sh4dow_foot_cone(&f, &p, FLOOR, UP, UP, FWD, q) == 0.0f, "no fade height");
    CHECK(sh4dow_foot_cone(NULL, NULL, FLOOR, UP, UP, FWD, q) == 0.0f, "NULL foot");
    CHECK(sh4dow_foot_cone(&f, NULL, NULL, UP, UP, FWD, q) == 0.0f, "NULL point");
    CHECK(sh4dow_foot_cone(&f, NULL, FLOOR, UP, UP, FWD, NULL) == 0.0f, "NULL quad");
}

/* A sloped floor: all four corners lie on the plane lifted along its normal, and the cone still runs away from the light. */
static void test_cone_sloped_floor(void) {
    const float n[3] = { 0.6f, 0.0f, 0.8f };
    const float p[3] = { 0.0f, 0.0f, 0.0f };
    float q[4][3];
    sh4dow_foot_t f = foot_at(0.0f, 0.0f, 0.0f, -1.0f, 0.3f, -1.0f, 0.05f);
    int i;

    NEAR(sh4dow_foot_cone(&f, NULL, p, n, UP, FWD, q), 1.0f, 1.0e-6f, "heel on the plane");
    for(i = 0; i < 4; i++) {
        const float d = (q[i][0] - p[0]) * n[0] + (q[i][1] - p[1]) * n[1] + (q[i][2] - p[2]) * n[2];

        NEAR(d, SH4DOW_FOOT_LIFT, 1.0e-6f, "corner %d on the lifted plane", i);
    }
    CHECK(q[2][1] > q[0][1], "the cone runs toward +y, with the light's travel");
}

/* The same cone in a Y up world is the Z up cone, rotated: the library has no fixed axis. */
static void test_cone_other_axes(void) {
    const float up_y[3] = { 0.0f, 1.0f, 0.0f };
    float a[4][3], b[4][3];
    sh4dow_foot_t fz = foot_at(1.0f, 2.0f, 0.1f, 0.4f, -0.2f, -1.0f, 0.05f), fy;
    int i;

    /* (x, y, z) -> (x, z, -y): Z up becomes Y up, the facing +y becomes +z (the third axis). */
    fy = foot_at(1.0f, 0.1f, -2.0f, 0.4f, -1.0f, 0.2f, 0.05f);
    {
        const float fwd_z[3] = { 0.0f, 0.0f, -1.0f };
        const float plane[3] = { 0.0f, 0.0f, 0.0f };
        const float fz_fwd[3] = { 0.0f, 1.0f, 0.0f };

        NEAR(sh4dow_foot_cone(&fz, NULL, plane, UP, UP, fz_fwd, a), sh4dow_foot_cone(&fy, NULL, plane, up_y, up_y, fwd_z, b), 1.0e-6f, "same fade");
        for(i = 0; i < 4; i++) {
            NEAR(b[i][0], a[i][0], 1.0e-5f, "corner %d x", i);
            NEAR(b[i][1], a[i][2], 1.0e-5f, "corner %d up", i);
            NEAR(b[i][2], -a[i][1], 1.0e-5f, "corner %d depth", i);
        }
    }
}

/* Everything in metres: scaled params, scaled inputs, a scaled quad and the same fade. */
static void test_cone_units(void) {
    const float k = 3.048f;
    float a[4][3], b[4][3], fa, fb;
    sh4dow_foot_t f1 = foot_at(0.5f, 0.25f, 0.07f, 0.6f, 0.1f, -1.0f, 0.05f), f2 = f1;
    sh4dow_foot_params_t p;
    const float plane[3] = { 0.0f, 0.0f, 0.0f };   /* the origin stays the origin in any unit */
    int i, j;

    f2.heel[0] *= k; f2.heel[1] *= k; f2.heel[2] *= k;
    f2.radius *= k;
    sh4dow_foot_params_default(&p);
    sh4dow_foot_params_scale(&p, k);
    fa = sh4dow_foot_cone(&f1, NULL, plane, UP, UP, FWD, a);
    fb = sh4dow_foot_cone(&f2, &p, plane, UP, UP, FWD, b);
    NEAR(fa, fb, 1.0e-5f, "fade is a ratio");
    for(i = 0; i < 4; i++)
        for(j = 0; j < 3; j++)
            NEAR(b[i][j], a[i][j] * k, 1.0e-4f, "corner %d.%d", i, j);
}

static void test_place(void) {
    const float fwd[3] = { 1.0f, 0.0f, 0.0f };
    float centre[3] = { 2.0f, 3.0f, 0.05f };
    sh4dow_foot_t f;
    sh4dow_foot_params_t p;

    /* Seven tenths of a sphere radius back along the facing, the height kept; 0.8 of the radius wide. */
    sh4dow_foot_place(&f, NULL, centre, 0.05f, fwd, UP);
    NEAR(f.heel[0], 2.0f - 0.035f, 1.0e-6f, "heel x"); NEAR(f.heel[1], 3.0f, 1.0e-6f, "heel y"); NEAR(f.heel[2], 0.05f, 1.0e-6f, "heel z");
    NEAR(f.radius, 0.04f, 1.0e-6f, "width");

    /* The facing's vertical part and its length do not matter. */
    {
        const float tilted[3] = { 3.0f, 0.0f, 9.0f };

        sh4dow_foot_place(&f, NULL, centre, 0.05f, tilted, UP);
        NEAR(f.heel[0], 2.0f - 0.035f, 1.0e-6f, "tilted facing x");
        NEAR(f.heel[2], 0.05f, 1.0e-6f, "tilted facing z");
    }
    /* A facing straight up has no heel direction: the centre. */
    {
        const float straight[3] = { 0.0f, 0.0f, 1.0f };

        sh4dow_foot_place(&f, NULL, centre, 0.05f, straight, UP);
        NEAR(f.heel[0], 2.0f, 0, "no direction"); NEAR(f.heel[1], 3.0f, 0, "no direction y");
    }
    /* Clamped to 3 to 12 cm. */
    sh4dow_foot_place(&f, NULL, centre, 0.01f, fwd, UP);
    NEAR(f.radius, 0.03f, 1.0e-7f, "narrow clamp");
    sh4dow_foot_place(&f, NULL, centre, 0.5f, fwd, UP);
    NEAR(f.radius, 0.12f, 1.0e-7f, "wide clamp");
    sh4dow_foot_params_default(&p);
    sh4dow_foot_params_scale(&p, 3.048f);
    sh4dow_foot_place(&f, &p, centre, 0.5f, fwd, UP);
    NEAR(f.radius, 0.12f * 3.048f, 1.0e-6f, "scaled clamp");
    centre[0] = 0.0f;
    sh4dow_foot_place(NULL, NULL, centre, 0.05f, fwd, UP);   /* nothing to do, no crash */
}

static void test_range_fade(void) {
    sh4dow_foot_params_t p;
    float last = 1.0f, d;

    NEAR(sh4dow_foot_range_fade(NULL, 0.0f, 1.0f), 1.0f, 0, "at the camera");
    NEAR(sh4dow_foot_range_fade(NULL, 17.99f, 1.0f), 1.0f, 1.0e-6f, "before the fade");
    NEAR(sh4dow_foot_range_fade(NULL, 19.0f, 1.0f), 0.5f, 1.0e-5f, "half way");
    NEAR(sh4dow_foot_range_fade(NULL, 20.0f, 1.0f), 0.0f, 0, "at the range");
    NEAR(sh4dow_foot_range_fade(NULL, 99.0f, 1.0f), 0.0f, 0, "beyond");
    NEAR(sh4dow_foot_range_fade(NULL, NAN, 1.0f), 0.0f, 0, "NaN");
    /* The scope: range and fade both times the zoom, counted as 1 below 1. */
    NEAR(sh4dow_foot_range_fade(NULL, 35.9f, 2.0f), 1.0f, 1.0e-5f, "zoom 2 before the fade");
    NEAR(sh4dow_foot_range_fade(NULL, 38.0f, 2.0f), 0.5f, 1.0e-5f, "zoom 2 half way");
    NEAR(sh4dow_foot_range_fade(NULL, 40.0f, 2.0f), 0.0f, 0, "zoom 2 at the range");
    NEAR(sh4dow_foot_range_fade(NULL, 19.0f, 0.5f), 0.5f, 1.0e-5f, "zoom below 1 counts as 1");
    for(d = 18.0f; d <= 20.0f; d += 0.05f) {
        const float v = sh4dow_foot_range_fade(NULL, d, 1.0f);

        CHECK(v <= last + 1.0e-6f && v >= 0.0f && v <= 1.0f, "monotone at %.2f", d);
        last = v;
    }
    sh4dow_foot_params_default(&p);
    sh4dow_foot_params_scale(&p, 3.048f);
    NEAR(sh4dow_foot_range_fade(&p, 19.0f * 3.048f, 1.0f), 0.5f, 1.0e-4f, "scaled half way");
}

static void test_picture(void) {
    uint8_t b[32 * 32];
    uint16_t w[32 * 32];
    unsigned x, y, sum = 0;
    float v, last = 1.0f;

    /* The round heel is full; the corners and the far end are empty; the picture is mirror symmetric. */
    NEAR(sh4dow_foot_texel(0.5f, 0.25f), 1.0f, 1.0e-6f, "heel centre");
    NEAR(sh4dow_foot_texel(0.0f, 0.0f), 0.0f, 1.0e-6f, "corner");
    NEAR(sh4dow_foot_texel(0.5f, 1.0f), 0.0f, 1.0e-6f, "far end");
    NEAR(sh4dow_foot_texel(0.5f, 0.99f), 0.0f, 0.01f, "almost nothing at the far end");
    for(y = 0; y <= 40; y++)
        for(x = 0; x <= 40; x++) {
            const float u = (float)x / 40.0f, vv = (float)y / 40.0f;

            v = sh4dow_foot_texel(u, vv);
            CHECK(v >= 0.0f && v <= 1.0f, "range at %u %u: %g", x, y, v);
            NEAR(sh4dow_foot_texel(1.0f - u, vv), v, 1.0e-5f, "mirror at %u %u", x, y);
        }
    /* Down the middle, after the heel, it only fades. */
    for(y = 0; y <= 80; y++) {
        const float vv = 0.25f + 0.75f * (float)y / 80.0f;

        v = sh4dow_foot_texel(0.5f, vv);
        CHECK(v <= last + 1.0e-6f, "middle falls at %.3f", vv);
        last = v;
    }
    /* The cone is wider past the heel than at it: at v = 0.45, 0.5 +- 0.3 is inside, at the heel's centre row it is not. */
    CHECK(sh4dow_foot_texel(0.5f + 0.3f, 0.45f) > sh4dow_foot_texel(0.5f + 0.3f, 0.25f), "the cone widens past the round heel");

    sh4dow_foot_texture(b, 32);
    sh4dow_foot_texture565(w, 32);
    for(y = 0; y < 32; y++)
        for(x = 0; x < 32; x++) {
            const unsigned i = y * 32 + x, r5 = w[i] >> 11, g6 = (w[i] >> 5) & 63u, b5 = w[i] & 31u;
            const float t = sh4dow_foot_texel(((float)x + 0.5f) / 32.0f, ((float)y + 0.5f) / 32.0f);

            sum += b[i];
            CHECK(fabsf((float)b[i] - t * 255.0f) <= 0.51f, "byte texel %u %u", x, y);
            CHECK(r5 == b5, "red and blue agree at %u %u", x, y);
            CHECK(fabsf((float)g6 - t * 63.0f) <= 0.51f && fabsf((float)r5 - t * 31.0f) <= 0.51f, "565 texel %u %u", x, y);
        }
    CHECK(sum > 88000u && sum < 92000u, "the picture's weight %u", sum);   /* the shipped picture: 90056 on the plain C path */
    sh4dow_foot_texture(NULL, 32);
    sh4dow_foot_texture565(w, 0);
}

static void test_pick_feet(void) {
    /* A figure in model space, Z up, +Y left: node, centre, radius. Height 0 .. 1; the zone is below 0.25. */
    static const float c[7][3] = {
        { 0.00f, 0.00f, 0.50f },    /* 0 pelvis */
        { 0.00f, 0.10f, 0.03f },    /* 1 left foot */
        { 0.00f, -0.10f, 0.02f },   /* 2 right foot */
        { 0.00f, 0.10f, 0.25f },    /* 3 left calf: on the zone's edge, not below it */
        { 0.00f, -0.10f, 0.24f },   /* 4 right calf: inside the zone, higher than the foot */
        { 0.00f, 0.00f, 0.90f },    /* 5 head */
        { 0.00f, 0.12f, 0.01f },    /* 6 a zero radius node under the left foot */
    };
    static const float r[7] = { 0.1f, 0.05f, 0.05f, 0.06f, 0.06f, 0.1f, 0.0f };
    uint8_t out[2];
    float c2[7][3];
    unsigned n, i;

    n = sh4dow_pick_feet(&c[0][0], r, 7, 2, 1, 0.0f, 1.0f, out);
    CHECK(n == 2 && out[0] == 2 && out[1] == 1, "feet: %u %u %u", n, out[0], out[1]);   /* the lowest (right), then the lowest on the other side (left) */

    /* The same figure on a Y up skeleton, +X left: axes 1 and 0. */
    for(i = 0; i < 7; i++) { c2[i][0] = c[i][1]; c2[i][1] = c[i][2]; c2[i][2] = c[i][0]; }
    n = sh4dow_pick_feet(&c2[0][0], r, 7, 1, 0, 0.0f, 1.0f, out);
    CHECK(n == 2 && out[0] == 2 && out[1] == 1, "y up feet: %u %u %u", n, out[0], out[1]);

    /* One foot on the ground only. */
    {
        float up[7][3];

        memcpy(up, c, sizeof(up));
        up[1][2] = 0.6f;
        n = sh4dow_pick_feet(&up[0][0], r, 7, 2, 1, 0.0f, 1.0f, out);
        CHECK(n == 1 && out[0] == 2 && out[1] == SH4DOW_NO_FOOT, "one foot: %u %u %u", n, out[0], out[1]);
    }
    /* Both on one side of the middle: one foot. A hovering or legless figure: none. */
    {
        float side[7][3];

        memcpy(side, c, sizeof(side));
        side[2][1] = 0.2f;
        side[4][1] = 0.1f;   /* the right calf crosses the middle too */
        n = sh4dow_pick_feet(&side[0][0], r, 7, 2, 1, 0.0f, 1.0f, out);
        CHECK(n == 1 && out[1] == SH4DOW_NO_FOOT, "same side: %u", n);
    }
    n = sh4dow_pick_feet(&c[0][0], r, 7, 2, 1, 0.0f, 10.0f, out);
    CHECK(n == 2 && out[0] == 2, "a ten times taller figure: the zone grows with it");
    n = sh4dow_pick_feet(&c[0][0], r, 7, 2, 1, -5.0f, 1.0f, out);
    CHECK(n == 0 && out[0] == SH4DOW_NO_FOOT && out[1] == SH4DOW_NO_FOOT, "hovering: %u", n);
    n = sh4dow_pick_feet(&c[0][0], r, 0, 2, 1, 0.0f, 1.0f, out);
    CHECK(n == 0 && out[0] == SH4DOW_NO_FOOT, "no nodes");
    n = sh4dow_pick_feet(&c[0][0], r, 7, 3, 1, 0.0f, 1.0f, out);
    CHECK(n == 0 && out[0] == SH4DOW_NO_FOOT, "a bad axis");
    CHECK(sh4dow_pick_feet(NULL, r, 7, 2, 1, 0.0f, 1.0f, out) == 0 && out[0] == SH4DOW_NO_FOOT, "NULL centres");
    CHECK(sh4dow_pick_feet(&c[0][0], r, 7, 2, 1, 0.0f, 1.0f, NULL) == 0, "NULL out");
}

int main(void) {
    test_shipped_numbers();
    test_cone_flat_floor();
    test_cone_rejects();
    test_cone_sloped_floor();
    test_cone_other_axes();
    test_cone_units();
    test_place();
    test_range_fade();
    test_picture();
    test_pick_feet();
    if(fails) {
        printf("foot_test: %d FAILED\n", fails);
        return 1;
    }
    printf("foot_test: ok\n");
    return 0;
}
