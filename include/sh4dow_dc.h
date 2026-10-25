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

    Foot cones (src/sh4dow_foot.c) are the floor half: two small shadows per
    character, one under each foot, each a cone that starts at the heel and
    stretches away from the brightest light. sh4dow_foot_place() sets a cone
    from a posed foot sphere, sh4dow_foot_cone() puts it on a floor plane as a
    quad, sh4dow_foot_texture() draws the picture it carries and
    sh4dow_foot_range_fade() fades it with distance. They need no column frame.

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

/** \name Foot cones: shipped tuning (sh4dow_foot_params_default())
    Lengths and widths are in foot radii (a pure ratio). The distances are in
    the original game's world units (1 unit = 3.048 m, a standing character is
    about 0.7 units tall): if yours differ, sh4dow_foot_params_scale() carries
    them over.
    @{ */
#define SH4DOW_FOOT_MIN_LEN     3.0f    /**< \brief Cone length with the light overhead, in radii. */
#define SH4DOW_FOOT_GAIN        3.0f    /**< \brief Extra radii of length per unit of light slope. */
#define SH4DOW_FOOT_MAX_SLOPE   2.0f    /**< \brief Slope (across / down) where the cone stops growing: 9 radii. */
#define SH4DOW_FOOT_MIN_FACING  0.25f   /**< \brief Floor normal . up at or below which no cone (a wall). */
#define SH4DOW_FOOT_HEEL_BACK   0.7f    /**< \brief Heel: foot sphere radii behind its centre, along the facing. */
#define SH4DOW_FOOT_WIDTH       0.8f    /**< \brief Cone half width at the heel, in foot sphere radii ... */
#define SH4DOW_FOOT_WIDTH_MIN   0.03f   /**< \brief ... clamped to at least this (distance) ... */
#define SH4DOW_FOOT_WIDTH_MAX   0.12f   /**< \brief ... and at most this (distance). */
#define SH4DOW_FOOT_FADE_HEIGHT 0.35f   /**< \brief Heel height above the floor where the cone is gone (distance). */
#define SH4DOW_FOOT_LIFT        0.012f  /**< \brief The quad's offset off the floor, against depth fighting (distance). */
#define SH4DOW_FOOT_RANGE       20.0f   /**< \brief Camera distance where the cone is gone (distance). */
#define SH4DOW_FOOT_RANGE_FADE  2.0f    /**< \brief The fade out spans this much before the range (distance). */
#define SH4DOW_FOOT_APEX        0.25f   /**< \brief The picture's round heel sits this fraction of the cone along. */
#define SH4DOW_FOOT_ZONE        0.25f   /**< \brief sh4dow_pick_feet(): feet lie in this lowest fraction of the height. */
/** @} */

#define SH4DOW_NO_FOOT 0xffu            /**< \brief sh4dow_pick_feet(): no foot found. */

/** \brief   One foot's cone. */
typedef struct sh4dow_foot {
    float heel[3];              /**< \brief Where the cone starts: its narrow end. */
    float away[3];              /**< \brief The light's travel at the foot, from the light toward the heel; any length. */
    float radius;               /**< \brief Half width at the heel, in distance units; <= 0 casts nothing. */
} sh4dow_foot_t;

/** \brief   Foot cone shape: the Foot cones macros, as fields. */
typedef struct sh4dow_foot_params {
    float min_len;              /**< \brief Length with the light overhead, in radii. */
    float gain;                 /**< \brief Extra radii per unit of slope. */
    float max_slope;            /**< \brief Slope where the length stops growing. */
    float min_facing;           /**< \brief Floor normal . up must be above this. */
    float heel_back;            /**< \brief sh4dow_foot_place(): heel, in sphere radii behind the centre. */
    float width;                /**< \brief sh4dow_foot_place(): half width, in sphere radii. */
    float fade_height;          /**< \brief Heel height where the cone is gone, distance, > 0. */
    float lift;                 /**< \brief Offset off the floor, distance. */
    float width_min;            /**< \brief sh4dow_foot_place(): smallest half width, distance. */
    float width_max;            /**< \brief sh4dow_foot_place(): largest half width, distance. */
    float range;                /**< \brief sh4dow_foot_range_fade(): where it reaches 0, distance. */
    float range_fade;           /**< \brief sh4dow_foot_range_fade(): width of the fade, distance. */
} sh4dow_foot_params_t;

/** \brief   Fill params with the shipped foot cone tuning.
    \param  params          The parameters to fill.
*/
void sh4dow_foot_params_default(sh4dow_foot_params_t *params);

/** \brief   Carry the distances over to another unit.

    Multiplies fade_height, lift, width_min, width_max, range and range_fade;
    the radius ratios and the slope numbers are unit free and stay.

    \param  params          The parameters to scale, normally sh4dow_foot_params_default().
    \param  units_per_unit  Your units per original game unit: 3.048 for metres,
                            1 for none.
*/
void sh4dow_foot_params_scale(sh4dow_foot_params_t *params, float units_per_unit);

/** \brief   Place a foot's cone from its posed foot sphere.

    The heel is heel_back sphere radii behind the centre, along forward
    flattened across up (the height is kept); the half width is width sphere
    radii, clamped to width_min .. width_max. away is left alone: set it from
    your light.

    \param  foot            The foot to fill: heel and radius.
    \param  params          The shape, or NULL for the shipped tuning.
    \param  centre          The foot sphere's centre, posed, world space.
    \param  sphere_radius   Its radius, world units, character scale applied.
    \param  forward         The character's facing, any length.
    \param  up              World up, unit length.
*/
void sh4dow_foot_place(sh4dow_foot_t *foot, const sh4dow_foot_params_t *params,
                       const float centre[3], float sphere_radius,
                       const float forward[3], const float up[3]);

/** \brief   One foot's cone on a floor plane, as a quad.

    The heel drops onto the plane along its normal, and the quad runs from
    there away from the light only, along away flattened onto the plane. Its
    length grows with the light's slope (across the floor over into it), from
    min_len to min_len + gain * max_slope radii. With the light straight
    overhead it points along forward and has the shortest length.

    The quad is TL, TR, BL, BR: TL and TR at the heel (texture v 0), BL and BR
    at the far end (v 1); TL and BL are texture u 0, TR and BR u 1. The picture
    is mirror symmetric, so which side is which does not show.

    \param  foot            The foot.
    \param  params          The shape, or NULL for the shipped tuning.
    \param  floor_point     A point of the floor plane under the foot.
    \param  floor_normal    The plane's normal, facing the foot; any length.
    \param  up              World up, unit length.
    \param  forward         The character's facing, for a light straight overhead.
    \param  quad            The four corners, out.
    \return                 The fade, 1 with the heel on the floor down to 0 at
                            fade_height above it; 0 when there is no quad (a
                            wall, no radius, the heel too high, or the plane
                            more than fade_height above the heel).
*/
float sh4dow_foot_cone(const sh4dow_foot_t *foot, const sh4dow_foot_params_t *params,
                       const float floor_point[3], const float floor_normal[3],
                       const float up[3], const float forward[3], float quad[4][3]);

/** \brief   Fade with distance from the camera.

    Full to range - range_fade, nothing from range on; both scaled by zoom (a
    scope's magnification), which counts as 1 below 1.

    \param  params          The shape, or NULL for the shipped tuning.
    \param  dist            Distance from the camera, finite.
    \param  zoom            Magnification, 1 for none.
    \return                 0..1.
*/
float sh4dow_foot_range_fade(const sh4dow_foot_params_t *params, float dist, float zoom);

/** \brief   The foot cone picture at one point.

    A round heel at v = 2 * SH4DOW_FOOT_APEX (in half widths), the cone
    widening past it to the full width and fading toward a third, then out at
    the far end.

    \param  u               Across, 0..1.
    \param  v               Along, 0 at the heel end of the quad, 1 at the far end.
    \return                 Shadow, 0..1.
*/
float sh4dow_foot_texel(float u, float v);

/** \brief   The foot cone picture, one byte a texel (texel centres).
    \param  out             side * side bytes, row major, row 0 at v 0; 255 is full shadow.
    \param  side            Texels a side; 32 is plenty.
*/
void sh4dow_foot_texture(uint8_t *out, unsigned side);

/** \brief   The foot cone picture as grey RGB565, for a texture the GPU multiplies in.
    \param  out             side * side texels, row major, row 0 at v 0; 0xffff is full shadow.
    \param  side            Texels a side; 32 is plenty.
*/
void sh4dow_foot_texture565(uint16_t *out, unsigned side);

/** \brief   Find a skeleton's two feet from its bind pose node spheres.

    The lowest node, then the lowest on the other side of the middle (the
    other sign along side_axis). A node counts only below SH4DOW_FOOT_ZONE of
    the height lo..hi, so a hovering or legless model has none.

    \param  centre          Node sphere centres, x y z per node, model space.
    \param  radius          Node sphere radii; <= 0 is never a foot.
    \param  count           Nodes; past 254 are ignored.
    \param  up_axis         The up axis: 0 x, 1 y, 2 z.
    \param  side_axis       The left-right axis.
    \param  lo, hi          The model's extent along up.
    \param  out             Node indices, SH4DOW_NO_FOOT for none.
    \return                 Feet found, 0..2.
*/
unsigned sh4dow_pick_feet(const float *centre, const float *radius, unsigned count,
                          unsigned up_axis, unsigned side_axis, float lo, float hi, uint8_t out[2]);

/* Private: the arithmetic both builds share. */
#ifndef SH4DOW_DC_NO_SH4ZAM
static inline float sh4dow_sqrt_(float x) { return shz_sqrtf(x); }
static inline float sh4dow_sat_(float x) { return shz_saturatef(x); }

static inline float sh4dow_smooth01(float t) {
    t = shz_saturatef(t);
    return t * t * shz_fmaf(t, -2.0f, 3.0f);
}

/* perp^2 - outer^2 by one four-term dot (FMACs; FIPR with SHZ_SH4_DOT_FIPR). */
static inline float sh4dow_disc_(const sh4dow_disc_t *d, float vu, float vw, float vv) {
    return shz_dot8f(d->c[0], d->c[1], d->c[2], d->c[3], vu, vw, vv, 1.0f);
}

static inline float sh4dow_len2_(float a, float b) { return shz_vec2_magnitude_sqr(shz_vec2_init(a, b)); }
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

static inline float sh4dow_len2_(float a, float b) { return a * a + b * b; }
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
    Dreamcast, sh4dow_load_xmtrx() then one FTRV per point does the same.

    \param  frame           A prepared frame.
    \param  x, y, z         The point.
    \param  out             Light axis, u and w coordinates.
*/
static inline void sh4dow_project(const sh4dow_frame_t *frame, float x, float y, float z, float out[3]) {
#ifndef SH4DOW_DC_NO_SH4ZAM
    const shz_vec3_t r = shz_vec4_dot3(shz_vec4_init(x, y, z, 1.0f),
        shz_vec4_init(frame->rows[0][0], frame->rows[0][1], frame->rows[0][2], frame->rows[0][3]),
        shz_vec4_init(frame->rows[1][0], frame->rows[1][1], frame->rows[1][2], frame->rows[1][3]),
        shz_vec4_init(frame->rows[2][0], frame->rows[2][1], frame->rows[2][2], frame->rows[2][3]));

    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
#else
    int k;

    for(k = 0; k < 3; k++)
        out[k] = frame->rows[k][0] * x + frame->rows[k][1] * y + frame->rows[k][2] * z + frame->rows[k][3];
#endif
}

#ifndef SH4DOW_DC_NO_SH4ZAM
/** \brief   Load a frame's projection into XMTRX (SH4ZAM builds only).

    After it, shz_xmtrx_transform_point3(shz_vec3_init(x, y, z)) is
    sh4dow_project() in one FTRV: (vt, vu, vw) for sh4dow_strength_list() and
    sh4dow_strength_rel(). XMTRX keeps the frame until you load your own
    matrix again.

    \param  frame           A prepared frame.
*/
static inline void sh4dow_load_xmtrx(const sh4dow_frame_t *frame) {
    const shz_vec4_t r0 = shz_vec4_init(frame->rows[0][0], frame->rows[0][1], frame->rows[0][2], frame->rows[0][3]);
    const shz_vec4_t r1 = shz_vec4_init(frame->rows[1][0], frame->rows[1][1], frame->rows[1][2], frame->rows[1][3]);
    const shz_vec4_t r2 = shz_vec4_init(frame->rows[2][0], frame->rows[2][1], frame->rows[2][2], frame->rows[2][3]);

    shz_xmtrx_load_rows_3x4(&r0, &r1, &r2);
}
#endif

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
        perp2 = sh4dow_len2_(du, dw);
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
    const float vv = sh4dow_len2_(vu, vw);
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

    One four-term dot product and one compare per listed column. A FULL entry
    skips the fades, and one that lands in the solid core ends the search.

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
    const float vv = sh4dow_len2_(vu, vw);
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
