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

/** \brief   One entry of a packed list: one 32-byte cache line, read front to back.

    The side test is the plain distance to the axis, origin-relative: no
    expanded square, so no precision limit on thin columns far from the origin.
*/
typedef struct sh4dow_entry {
    float lu, lw;               /**< \brief The axis across the light, origin-relative. */
    float outer2, inner2;       /**< \brief Feather and core radii, squared. */
    float inner, inv_feather;   /**< \brief The column's. */
    float t0r;                  /**< \brief The column's start, origin-relative (general entries). */
    float top;                  /**< \brief The column's fade-in reciprocal (general entries). */
} sh4dow_entry_t;

/** \brief   What a pack tests per column (sh4dow_capsule_pack()), beside its ready entry. */
typedef struct sh4dow_cull {
    float outer;                /**< \brief Feather's outer radius. */
    float fade_w;               /**< \brief Distance past the start before full strength. */
    uint32_t bone;              /**< \brief The column's bone. */
} sh4dow_cull_t;

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
    sh4dow_entry_t entry[SH4DOW_COLUMN_MAX];   /**< \brief Per column, the packed entry, ready to copy. */
    sh4dow_cull_t cull[SH4DOW_COLUMN_MAX];     /**< \brief Per column, the pack's reach test. */
} sh4dow_frame_t;


/** \brief   One bone's column list, packed for its vertex loop (sh4dow_ball_pack()).

    The entries sit in list order on consecutive cache lines, with the frame
    numbers the fades need, so sh4dow_strength_packed() walks one pointer and
    reads nothing else.
*/
typedef struct sh4dow_packed {
    sh4dow_entry_t entry[SH4DOW_COLUMN_MAX]; /**< \brief FULL entries first, then the rest in start order. */
    float length, hold_at, inv_fade;         /**< \brief The frame's. */
    uint8_t full;                            /**< \brief FULL entries, at the front. */
    uint8_t count;                           /**< \brief All entries; 0: every vertex of the ball is unshaded. */
    uint8_t solid;                           /**< \brief 1: the whole ball lies in a FULL column's core, every vertex is 1. */
    /* The Dreamcast walk reads up to 12 bytes past the last entry: length, hold_at and inv_fade cover it. */
} sh4dow_packed_t;

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

/** \brief   Pack a column list for sh4dow_strength_packed().
    \param  frame           The prepared frame the list came from.
    \param  list, n         From sh4dow_ball_columns() or sh4dow_capsule_columns().
    \param  out             The packed list; solid is left 0.
    \return                 Entries packed (n).
*/
unsigned sh4dow_pack(const sh4dow_frame_t *frame, const uint8_t *list, unsigned n, sh4dow_packed_t *out);

/** \brief   sh4dow_capsule_columns() and sh4dow_pack() in one, plus the solid test.

    out->count 0 means no vertex in the hull is shaded, and out->solid 1 means
    every one is at full strength: either way the vertex loop can skip the
    projection and write 0 or 1.

    \param  frame           A prepared frame.
    \param  a, ra, b, rb    The two balls, as sh4dow_capsule_columns().
    \param  own             The vertices' first bone, or SH4DOW_BONE_NONE.
    \param  out             The packed list.
    \return                 Entries packed.
*/
unsigned sh4dow_capsule_pack(const sh4dow_frame_t *frame, const float a[3], float ra,
                             const float b[3], float rb, unsigned own, sh4dow_packed_t *out);

/** \brief   sh4dow_capsule_pack() for one ball.
    \param  frame           A prepared frame.
    \param  ct, cu, cw      Ball centre, projected (sh4dow_project()).
    \param  r               Ball radius.
    \param  own             The vertices' first bone, or SH4DOW_BONE_NONE.
    \param  out             The packed list.
    \return                 Entries packed.
*/
unsigned sh4dow_ball_pack(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                          unsigned own, sh4dow_packed_t *out);

/** \brief   sh4dow_strength_packed() over one bone's vertices.

    An empty pack writes 0s and a solid one 1s without touching the
    positions. Otherwise each vertex is projected (on the Dreamcast, one FTRV,
    the next vertex's issued before this one is shaded, so the transform's
    latency overlaps the shade) and shaded with the pack's numbers held in
    registers. On the Dreamcast this loads the frame into XMTRX: load your own
    matrix again afterwards.

    \param  frame           The prepared frame the pack came from.
    \param  p               The bone's pack.
    \param  xyz             Vertex positions, xyz_stride bytes apart, every one inside the pack's ball.
    \param  xyz_stride      Bytes from one position to the next.
    \param  out             Strengths, 0..1, count floats.
    \param  count           Number of vertices.
*/
void sh4dow_strength_packed_n(const sh4dow_frame_t *frame, const sh4dow_packed_t *p,
                              const float *xyz, size_t xyz_stride, float *out, unsigned count);

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
#if defined(__GNUC__)
#define SH4DOW_FORCE_INLINE_ static inline __attribute__((always_inline))
#else
#define SH4DOW_FORCE_INLINE_ static inline
#endif
#ifndef SH4DOW_DC_NO_SH4ZAM
static inline float sh4dow_sqrt_(float x) { return shz_sqrtf(x); }
static inline float sh4dow_sqrt_pos_(float x) { return shz_sqrtf_fsrra(x); }   /* x > 0: no zero guard */
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
static inline float sh4dow_sqrt_pos_(float x) { return sqrtf(x); }
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

/* sh4dow_feather_() for inner2 < perp2 < outer2, as the packed loop guarantees: perp2 > 0, so no zero guard, and
   the ramp is in 0..1 up to rounding, which lands on smoothstep's flat ends, so no clamps. Three compares and
   branches fewer on a feather hit, for the same shade. */
static inline float sh4dow_feather_in_(float perp2, float inner, float inv_feather) {
    const float t = (sh4dow_sqrt_pos_(perp2) - inner) * inv_feather;

    return 1.0f - t * t * (3.0f - 2.0f * t);
}

/* core scaled by the fade-in and the fade-out, t past the column's start. */
static inline float sh4dow_fades_(float core, float t, float top, float hold_at, float inv_fade) {
    const float out = t > hold_at ? 1.0f - sh4dow_smooth01((t - hold_at) * inv_fade) : 1.0f;
    return core * sh4dow_sat_(t * top) * out;
}

static inline float sh4dow_span_(const sh4dow_frame_t *f, float core, float t, float top) {
    return sh4dow_fades_(core, t, top, f->hold_at, f->inv_fade);
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

/** \brief   Shade strength at a projected vertex, against a packed list.

    sh4dow_strength_list() over the same list, with the side test done as a
    plain distance rather than an expanded square: the same strength as
    sh4dow_strength(), without sh4dow_strength_list()'s precision note. Each
    entry is one cache line read from the front.

    \param  p               From sh4dow_ball_pack(), sh4dow_capsule_pack() or sh4dow_pack().
    \param  vt, vu, vw      The vertex, from sh4dow_project(), inside the list's ball.
    \return                 0..1, as sh4dow_strength().
*/
/* The packed walk on plain values, so a vertex loop holds them in registers rather than reloading the pack. */
/* The first entry from f before end whose column the vertex is inside (perp2 < outer2): its address, with
   *perp2 set; end when there is none. Entries are 8 floats, lu, lw, outer2 first. */
SH4DOW_FORCE_INLINE_ const float *sh4dow_scan_c_(const float *f, const float *end, float vu, float vw, float *perp2) {
    for(; f < end; f += 8) {
        const float du = f[0] - vu, dw = f[1] - vw, p2 = du * du + dw * dw;

        if(p2 < f[2]) {
            *perp2 = p2;
            break;
        }
    }
    return f;
}

/* The assembly side test is opt-in (-DSH4DOW_ASM_SCAN): on the pipeline emulator it lands within code-placement
   noise of the C walk, so it waits for a hardware A/B. */
#if !defined(SH4DOW_DC_NO_SH4ZAM) && SHZ_BACKEND == SHZ_SH4 && defined(SH4DOW_ASM_SCAN)
#define SH4DOW_HAVE_ASM_ 1
/* Two entries a pass, interleaved, so one's load, subtract, square and sum fill the other's latency; squares by
   FMUL and FADD, not FMAC, so nothing waits on FR0; a miss falls through. It reads the second entry of a pass
   even when the first is the last one: up to 12 bytes past the end, which sh4dow_packed_t keeps inside itself. */
SH4DOW_FORCE_INLINE_ const float *sh4dow_scan_asm_(const float *f, const float *end, float vu, float vw, float *perp2) {
    /* fv4 only: fv8 is where FTRV leaves the next vertex in flight (sh4dow_strength_packed_n()). */
    register float a1 __asm__("fr4");
    register float b1 __asm__("fr5");
    register float a2 __asm__("fr6");
    register float b2 __asm__("fr7");
    const float *g;

    __asm__ (
        "0:\n\t"
        "cmp/hs  %[end], %[f]\n\t"
        "bt      9f\n\t"
        "mov     %[f], %[g]\n\t"
        "fmov.s  @%[g]+, %[a1]\n\t"     /* lu */
        "fmov.s  @%[g]+, %[b1]\n\t"     /* lw */
        "add     #24, %[g]\n\t"
        "fmov.s  @%[g]+, %[a2]\n\t"
        "fsub    %[vu], %[a1]\n\t"
        "fmov.s  @%[g]+, %[b2]\n\t"
        "fsub    %[vw], %[b1]\n\t"
        "fsub    %[vu], %[a2]\n\t"
        "fsub    %[vw], %[b2]\n\t"
        "fmul    %[a1], %[a1]\n\t"
        "fmul    %[b1], %[b1]\n\t"
        "fmul    %[a2], %[a2]\n\t"
        "fmul    %[b2], %[b2]\n\t"
        "fadd    %[b1], %[a1]\n\t"      /* perp2, first */
        "add     #-32, %[g]\n\t"
        "fmov.s  @%[g], %[b1]\n\t"      /* outer2, first */
        "fadd    %[b2], %[a2]\n\t"      /* perp2, second */
        "add     #32, %[g]\n\t"
        "fmov.s  @%[g], %[b2]\n\t"      /* outer2, second */
        "fcmp/gt %[a1], %[b1]\n\t"      /* outer2 > perp2: inside the first */
        "bt      9f\n\t"
        "add     #32, %[f]\n\t"
        "cmp/hs  %[end], %[f]\n\t"
        "bt      9f\n\t"
        "fcmp/gt %[a2], %[b2]\n\t"
        "bt      8f\n\t"
        "bra     0b\n\t"
        "add     #32, %[f]\n\t"         /* delay slot */
        "8:\n\t"
        "fmov    %[a2], %[a1]\n\t"
        "9:\n\t"
        : [f] "+r" (f), [g] "=&r" (g), [a1] "=&f" (a1), [b1] "=&f" (b1), [a2] "=&f" (a2), [b2] "=&f" (b2)
        : [end] "r" (end), [vu] "f" (vu), [vw] "f" (vw)
        : "t", "memory");
    (void)g; (void)b1; (void)a2; (void)b2;
    *perp2 = a1;
    return f;
}
#else
#define SH4DOW_HAVE_ASM_ 0
#define sh4dow_scan_asm_ sh4dow_scan_c_
#endif

/* The packed walk on plain values, so a vertex loop holds them in registers rather than reloading the pack. */
SH4DOW_FORCE_INLINE_ float sh4dow_entries_impl_(const sh4dow_entry_t *e, const sh4dow_entry_t *end_full,
                                                 const sh4dow_entry_t *end, float length, float hold_at, float inv_fade,
                                                 float vt, float vu, float vw, int use_asm) {
    const float *f = &e->lu, *const ff = &end_full->lu, *const fe = &end->lu;
    float dark = 0.0f, perp2 = 0.0f;

    /* FULL entries: no fades. f[3] inner2, f[4] inner, f[5] inv_feather. */
    for(;;) {
        float s;

        f = use_asm ? sh4dow_scan_asm_(f, ff, vu, vw, &perp2) : sh4dow_scan_c_(f, ff, vu, vw, &perp2);
        if(f >= ff)
            break;
        if(perp2 <= f[3])
            return 1.0f;
        s = sh4dow_feather_in_(perp2, f[4], f[5]);
        if(s > dark)
            dark = s;
        f += 8;
    }
    f = ff;
    /* General entries, in start order: f[6] t0r, f[7] top. */
    for(;;) {
        float t, s;

        f = use_asm ? sh4dow_scan_asm_(f, fe, vu, vw, &perp2) : sh4dow_scan_c_(f, fe, vu, vw, &perp2);
        if(f >= fe)
            break;
        t = vt - f[6];
        if(t <= 0.0f)
            break;              /* Later general entries start further along. */
        if(t < length) {
            s = perp2 <= f[3] ? 1.0f : sh4dow_feather_in_(perp2, f[4], f[5]);
            s = sh4dow_fades_(s, t, f[7], hold_at, inv_fade);
            if(s > dark)
                dark = s;
            if(dark >= 1.0f)
                break;
        }
        f += 8;
    }

    return dark;
}

SH4DOW_FORCE_INLINE_ float sh4dow_entries_(const sh4dow_entry_t *e, const sh4dow_entry_t *end_full, const sh4dow_entry_t *end,
                                            float length, float hold_at, float inv_fade, float vt, float vu, float vw) {
    return sh4dow_entries_impl_(e, end_full, end, length, hold_at, inv_fade, vt, vu, vw, SH4DOW_HAVE_ASM_);
}

static inline float sh4dow_strength_packed(const sh4dow_packed_t *p, float vt, float vu, float vw) {
    return sh4dow_entries_(p->entry, p->entry + p->full, p->entry + p->count,
                           p->length, p->hold_at, p->inv_fade, vt, vu, vw);
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DOW_DC_H */
