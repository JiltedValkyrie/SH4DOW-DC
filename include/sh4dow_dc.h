/* SH4DOW-DC

   sh4dow_dc.h
   Copyright (C) 2026 Emerald May
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4dow_dc.h
    \brief   Soft self-shadow for skinned characters, one column per limb.

    Every limb casts a column away from the light. A vertex inside a column is
    darkened: fully within the core, fading out across a thin feather, fading
    in just past the column's start and out over its last stretch. The darkest
    column wins, and a column never shades the vertices of its own limb.

    Call sh4dow_prepare() once per character per frame, after posing, then
    evaluate each lit vertex with one of:

    - sh4dow_strength(): any vertex, from its position, against every column.
    - sh4dow_strength_list(): a vertex of one bone, against only the columns
      that bone's bounding ball can reach (sh4dow_ball_columns()), from its
      projection onto the frame (sh4dow_project(), or frame->rows in XMTRX).

    Both return the same strength. sh4dow_shade() runs the first over float
    colour triples. Multiply the lit colour by 1 - dark * strength.

    The dot products, square roots and reciprocals are SH4ZAM. Define
    SH4DOW_DC_NO_SH4ZAM to build the same steps in plain C.

    \author Emerald May
*/

#ifndef SH4DOW_DC_H
#define SH4DOW_DC_H

#include <stddef.h>
#include <stdint.h>

#ifndef SH4DOW_DC_NO_SH4ZAM
#include <sh4zam/shz_sh4zam.h>
#else
#include <math.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SH4DOW_COLUMN_MAX 32    /**< \brief Columns a frame holds. */
#define SH4DOW_BONE_NONE  0xffu /**< \brief Bone id that skips no vertex. */

/** \name Limb flags
    @{ */
#define SH4DOW_LIMB_HEAD  1u    /**< \brief Column starts at the neck, not the head. */
#define SH4DOW_LIMB_NECK  2u    /**< \brief The neck the head column starts from. */
/** @} */

/** \name Column list entries (sh4dow_ball_columns())
    @{ */
#define SH4DOW_LIST_INDEX 0x7fu /**< \brief Column index. */
#define SH4DOW_LIST_FULL  0x80u /**< \brief Whole ball is past the fade-in and before the fade-out. */
/** @} */

/** \name Shipped tuning (sh4dow_params_default())
    Radii are in limb radii; the length is in vertex units.
    @{ */
#define SH4DOW_INNER   1.25f    /**< \brief Solid core radius. */
#define SH4DOW_OUTER   1.35f    /**< \brief Radius where the shade reaches zero. */
#define SH4DOW_LENGTH  1.8f     /**< \brief Column length along the light. */
#define SH4DOW_HOLD    0.8f     /**< \brief Fraction of the length at full strength. */
#define SH4DOW_DARK    0.65f    /**< \brief Colour lost at full strength. */
#define SH4DOW_COLLAR  0.8f     /**< \brief Neck radii past the neck where the head column starts. */
#define SH4DOW_FADE_IN 0.1f     /**< \brief Distance past the start before full strength. */
/** @} */

/** \brief   One limb that casts a column. */
typedef struct sh4dow_limb {
    float position[3];          /**< \brief Joint centre, in the vertices' space. */
    float radius;               /**< \brief Sphere radius in that space; <= 0 casts nothing. */
    uint8_t bone;               /**< \brief First-bone id whose vertices the column skips. */
    uint8_t flags;              /**< \brief SH4DOW_LIMB_HEAD, SH4DOW_LIMB_NECK or 0. */
} sh4dow_limb_t;

/** \brief   Column shape and strength. */
typedef struct sh4dow_params {
    float inner_scale;          /**< \brief Core radius, in limb radii. */
    float outer_scale;          /**< \brief Feather's outer radius, above inner_scale. */
    float length;               /**< \brief Length along the light, > 0. */
    float hold;                 /**< \brief Fraction of the length at full strength, 0..1. */
    float dark;                 /**< \brief Colour lost at full strength, 0..1. */
    float collar;               /**< \brief Neck radii past the neck where a head column starts. */
    float fade_in;              /**< \brief Limb radii past the start before full strength. */
} sh4dow_params_t;

/** \brief   One prepared column, on the frame's axes (light, u, w). */
typedef struct sh4dow_column {
    float lt, lu, lw;           /**< \brief Joint on the light axis and the two side axes. */
    float from;                 /**< \brief Extra distance along the light before the start. */
    float t0;                   /**< \brief Start on the light axis: lt + from. */
    float inner, outer;         /**< \brief Core and feather radii. */
    float outer2, inner2;       /**< \brief Their squares. */
    float inv_feather;          /**< \brief 1 / (outer - inner). */
    float top;                  /**< \brief 1 / fade_w, or 1e8 with no fade-in. */
    float fade_w;               /**< \brief Distance past the start before full strength. */
    uint8_t bone;               /**< \brief First bone the column skips. */
} sh4dow_column_t;

/** \brief   A column's disc test for sh4dow_strength_list(), one 32-byte cache line.

    With origin-relative side coordinates, c . (vu, vw, vu^2 + vw^2, 1) is
    perp^2 - outer^2: >= 0 outside, <= core in the solid core.
*/
typedef struct sh4dow_disc {
    float c[4];                 /**< \brief (-2 lu, -2 lw, 1, lu^2 + lw^2 - outer^2). */
    float core;                 /**< \brief inner^2 - outer^2. */
    float inner;                /**< \brief The column's, kept on this line. */
    float inv_feather;          /**< \brief The column's, kept on this line. */
    float outer2;               /**< \brief The column's, kept on this line. */
} sh4dow_disc_t;

/** \brief   A character's columns for one frame (sh4dow_prepare()). */
typedef struct sh4dow_frame {
    float dir[3], u[3], w[3];   /**< \brief Light axis and the two side axes, orthonormal. */
    float length, hold, dark;   /**< \brief From the params, clamped. */
    float hold_at;              /**< \brief hold * length: no fade-out before this. */
    float inv_fade;             /**< \brief 1 / (length - hold_at), or 0. */
    float first, last;          /**< \brief Earliest start and latest end on the light axis. */
    int count;                  /**< \brief Columns stored. */
    sh4dow_column_t column[SH4DOW_COLUMN_MAX]; /**< \brief Sorted by t0. */
    float ot, ou, ow;           /**< \brief The origin (mean joint) on the three axes. */
    float rows[3][4];           /**< \brief (x, y, z, 1) onto the axes, origin-relative: a 3x4 for XMTRX. */
    sh4dow_disc_t disc[SH4DOW_COLUMN_MAX];     /**< \brief Per column, origin-relative. */
    float t0r[SH4DOW_COLUMN_MAX];              /**< \brief Per column start, origin-relative. */
} sh4dow_frame_t;

/** \brief   The light and side axes, held in registers across a vertex loop. */
typedef struct sh4dow_axes {
#ifndef SH4DOW_DC_NO_SH4ZAM
    shz_vec3_t dir, u, w;
#else
    float dir[3], u[3], w[3];
#endif
} sh4dow_axes_t;

/** \brief   Fill params with the shipped tuning.
    \param  params          The parameters to fill.
*/
void sh4dow_params_default(sh4dow_params_t *params);

/** \brief   Build a character's columns for this frame.

    A head limb starts at its neck's centre plus collar neck radii along the
    light, or one head radius along it when there is no neck or that start
    would fall on the lit side. Every other limb starts at its joint.

    \param  frame           The frame to fill.
    \param  params          Column shape, or NULL for the shipped tuning.
    \param  limbs           The limbs, in the vertices' space.
    \param  limb_count      Number of limbs; past SH4DOW_COLUMN_MAX are ignored.
    \param  direction       The way the shadow falls (away from the light), any length.
    \return                 Columns stored; 0 for a NULL frame, a zero or
                            non-finite direction, or invalid params.
*/
int sh4dow_prepare(sh4dow_frame_t *frame, const sh4dow_params_t *params,
                   const sh4dow_limb_t *limbs, unsigned limb_count,
                   const float direction[3]);

/** \brief   List the columns that can touch a ball of vertices.

    The list is exact for every point inside the ball: sh4dow_strength_list()
    over it equals sh4dow_strength(). Entries flagged SH4DOW_LIST_FULL come
    first, nearest axis first; the rest follow in start order.

    \param  frame           A prepared frame.
    \param  ct, cu, cw      Ball centre, projected (sh4dow_project()).
    \param  r               Ball radius, covering every vertex tested with the list.
    \param  own             The first bone of every vertex in the ball, or SH4DOW_BONE_NONE.
    \param  out             The list, at most frame->count entries.
    \return                 Number of entries.
*/
unsigned sh4dow_ball_columns(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                             unsigned own, uint8_t out[SH4DOW_COLUMN_MAX]);

/** \brief   List the columns that can touch the hull of two balls.

    A vertex blended between two bones lies between its two single-bone
    images, so inside this hull when each image lies inside its bone's ball.

    \param  frame           A prepared frame.
    \param  a, ra           First ball, projected centre and radius.
    \param  b, rb           Second ball.
    \param  own             The vertices' first bone, or SH4DOW_BONE_NONE.
    \param  out             The list, as sh4dow_ball_columns().
    \return                 Number of entries.
*/
unsigned sh4dow_capsule_columns(const sh4dow_frame_t *frame, const float a[3], float ra,
                                const float b[3], float rb, unsigned own,
                                uint8_t out[SH4DOW_COLUMN_MAX]);

/** \brief   Darken float colours by the shade.
    \param  frame           A prepared frame.
    \param  xyz             Vertex positions, xyz_stride bytes apart.
    \param  bone0           First bones, bone_stride bytes apart, or NULL for no skip.
    \param  rgb             Colours to scale, rgb_stride bytes apart.
    \param  count           Number of vertices.
*/
void sh4dow_shade(const sh4dow_frame_t *frame,
                  const float *xyz, size_t xyz_stride,
                  const uint8_t *bone0, size_t bone_stride,
                  float *rgb, size_t rgb_stride,
                  unsigned count);

/* Private: the arithmetic both builds share. */
#ifndef SH4DOW_DC_NO_SH4ZAM
static inline float sh4dow_sqrt_(float x) { return shz_sqrtf(x); }
static inline float sh4dow_sat_(float x) { return shz_saturatef(x); }

static inline float sh4dow_smooth01(float t) {
    t = shz_saturatef(t);
    return t * t * shz_fmaf(t, -2.0f, 3.0f);
}

/* perp^2 - outer^2 by one FIPR. */
static inline float sh4dow_disc_(const sh4dow_disc_t *d, float vu, float vw, float vv) {
    return shz_dot8f(d->c[0], d->c[1], d->c[2], d->c[3], vu, vw, vv, 1.0f);
}
#else
static inline float sh4dow_sqrt_(float x) { return sqrtf(x); }
static inline float sh4dow_sat_(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

static inline float sh4dow_smooth01(float t) {
    if(t <= 0.0f)
        return 0.0f;
    if(t >= 1.0f)
        return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static inline float sh4dow_disc_(const sh4dow_disc_t *d, float vu, float vw, float vv) {
    return d->c[0] * vu + d->c[1] * vw + d->c[2] * vv + d->c[3];
}
#endif

/* Strength across the feather, from the squared distance to the axis. */
static inline float sh4dow_feather_(float perp2, float inner, float inv_feather) {
    return 1.0f - sh4dow_smooth01((sh4dow_sqrt_(perp2) - inner) * inv_feather);
}

/* core scaled by the fade-in and the fade-out, t past the column's start. */
static inline float sh4dow_span_(const sh4dow_frame_t *f, float core, float t, float top) {
    const float out = t > f->hold_at ? 1.0f - sh4dow_smooth01((t - f->hold_at) * f->inv_fade) : 1.0f;
    return core * sh4dow_sat_(t * top) * out;
}

/** \brief   Load a frame's axes for a vertex loop.
    \param  frame           A prepared frame.
    \return                 Its axes.
*/
static inline sh4dow_axes_t sh4dow_axes(const sh4dow_frame_t *frame) {
    sh4dow_axes_t a;
#ifndef SH4DOW_DC_NO_SH4ZAM
    a.dir = shz_vec3_init(frame->dir[0], frame->dir[1], frame->dir[2]);
    a.u = shz_vec3_init(frame->u[0], frame->u[1], frame->u[2]);
    a.w = shz_vec3_init(frame->w[0], frame->w[1], frame->w[2]);
#else
    int k;

    for(k = 0; k < 3; k++) {
        a.dir[k] = frame->dir[k];
        a.u[k] = frame->u[k];
        a.w[k] = frame->w[k];
    }
#endif
    return a;
}

/** \brief   Project a point onto a frame, origin-relative.

    This is what sh4dow_strength_list() and sh4dow_ball_columns() take. On the
    Dreamcast, frame->rows loaded into XMTRX does it in one FTRV.

    \param  frame           A prepared frame.
    \param  x, y, z         The point.
    \param  out             Light axis, u and w coordinates.
*/
static inline void sh4dow_project(const sh4dow_frame_t *frame, float x, float y, float z, float out[3]) {
    int k;

    for(k = 0; k < 3; k++)
        out[k] = frame->rows[k][0] * x + frame->rows[k][1] * y + frame->rows[k][2] * z + frame->rows[k][3];
}

/** \brief   Shade strength at a vertex, against every column.

    A vertex outside the columns' span along the light costs one dot product.
    Otherwise the columns run in start order and stop at the first that starts
    at or past the vertex.

    \param  frame           A frame with at least one column.
    \param  axes            sh4dow_axes() of it.
    \param  x, y, z         The vertex.
    \param  own             Its first bone, or SH4DOW_BONE_NONE.
    \return                 0..1.
*/
static inline float sh4dow_strength(const sh4dow_frame_t *frame, const sh4dow_axes_t *axes,
                                    float x, float y, float z, unsigned own) {
    float vt, vu, vw, dark = 0.0f;
    int j;
#ifndef SH4DOW_DC_NO_SH4ZAM
    const shz_vec3_t v = shz_vec3_init(x, y, z);
    shz_vec2_t side;

    vt = shz_vec3_dot(v, axes->dir);
    if(vt <= frame->first || vt >= frame->last)
        return 0.0f;
    side = shz_vec3_dot2(v, axes->u, axes->w);
    vu = side.x;
    vw = side.y;
#else
    vt = x * axes->dir[0] + y * axes->dir[1] + z * axes->dir[2];
    if(vt <= frame->first || vt >= frame->last)
        return 0.0f;
    vu = x * axes->u[0] + y * axes->u[1] + z * axes->u[2];
    vw = x * axes->w[0] + y * axes->w[1] + z * axes->w[2];
#endif

    for(j = 0; j < frame->count; j++) {
        const sh4dow_column_t *col = &frame->column[j];
        const float t = vt - col->t0;
        float du, dw, perp2, s;

        if(t <= 0.0f)
            break;
        if(t >= frame->length)
            continue;
        if(own == col->bone && own != SH4DOW_BONE_NONE)
            continue;
        du = vu - col->lu;
        dw = vw - col->lw;
        perp2 = du * du + dw * dw;
        if(perp2 >= col->outer2)
            continue;
        s = perp2 <= col->inner2 ? 1.0f : sh4dow_feather_(perp2, col->inner, col->inv_feather);
        s = sh4dow_span_(frame, s, t, col->top);
        if(s > dark)
            dark = s;
        if(dark >= 1.0f)
            break;
    }

    return dark;
}

/** \brief   Shade strength at a projected vertex, against every column.

    For a vertex no list covers.

    \param  frame           A frame with at least one column.
    \param  vt, vu, vw      The vertex, from sh4dow_project().
    \param  own             Its first bone, or SH4DOW_BONE_NONE.
    \return                 0..1, as sh4dow_strength().
*/
static inline float sh4dow_strength_rel(const sh4dow_frame_t *frame, float vt, float vu, float vw,
                                        unsigned own) {
    const float vv = vu * vu + vw * vw;
    float dark = 0.0f;
    int j;

    for(j = 0; j < frame->count; j++) {
        const sh4dow_column_t *col = &frame->column[j];
        const sh4dow_disc_t *d = &frame->disc[j];
        const float t = vt - frame->t0r[j];
        float r, s;

        if(t <= 0.0f)
            break;
        if(t >= frame->length)
            continue;
        if(own == col->bone && own != SH4DOW_BONE_NONE)
            continue;
        r = sh4dow_disc_(d, vu, vw, vv);
        if(r >= 0.0f)
            continue;
        s = r <= d->core ? 1.0f : sh4dow_feather_(r + d->outer2, d->inner, d->inv_feather);
        s = sh4dow_span_(frame, s, t, col->top);
        if(s > dark)
            dark = s;
        if(dark >= 1.0f)
            break;
    }

    return dark;
}

/** \brief   Shade strength at a projected vertex, against a column list.

    One FIPR and one compare per listed column. A FULL entry skips the fades,
    and one that lands in the solid core ends the search.

    \note
    The disc test expands the square, so it loses precision for a column much
    thinner than its distance from the frame's origin (the mean joint): about
    one colour step at a ratio of 1:100, and a wrong answer near 1:3000.

    \param  frame           A prepared frame.
    \param  vt, vu, vw      The vertex, from sh4dow_project(), inside the list's ball.
    \param  list            From sh4dow_ball_columns() or sh4dow_capsule_columns().
    \param  n               Its length.
    \return                 0..1, as sh4dow_strength().
*/
static inline float sh4dow_strength_list(const sh4dow_frame_t *frame, float vt, float vu, float vw,
                                         const uint8_t *list, unsigned n) {
    const float vv = vu * vu + vw * vw;
    float dark = 0.0f;
    unsigned k;

    for(k = 0; k < n; k++) {
        const unsigned j = list[k] & SH4DOW_LIST_INDEX, full = list[k] & SH4DOW_LIST_FULL;
        const sh4dow_disc_t *d = &frame->disc[j];
        const float r = sh4dow_disc_(d, vu, vw, vv);
        float t = 0.0f, s;

        if(r >= 0.0f)
            continue;
        if(!full) {
            t = vt - frame->t0r[j];
            if(t <= 0.0f)
                break;          /* Later general entries start further along. */
            if(t >= frame->length)
                continue;
        }
        s = r <= d->core ? 1.0f : sh4dow_feather_(r + d->outer2, d->inner, d->inv_feather);
        s = full ? s : sh4dow_span_(frame, s, t, frame->column[j].top);
        if(s > dark)
            dark = s;
        if(dark >= 1.0f)
            break;
    }

    return dark;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DOW_DC_H */
