/* SH4DOW-DC

   sh4dow_foot.c
   Copyright (C) 2026 Emerald May
   See ../LICENSE.

   Foot cones: two small floor shadows per character, one under each foot,
   each starting at the heel and stretching away from the light. Geometry,
   picture and distance fade; the engine draws the quad.
*/

#include "sh4dow_dc.h"

/* One small vector layer for both builds, so each step is written once. */
#ifndef SH4DOW_DC_NO_SH4ZAM
typedef shz_vec3_t v3_t;

static inline v3_t v3(float x, float y, float z) { return shz_vec3_init(x, y, z); }
static inline v3_t v3_add(v3_t a, v3_t b) { return shz_vec3_add(a, b); }
static inline v3_t v3_sub(v3_t a, v3_t b) { return shz_vec3_sub(a, b); }
static inline v3_t v3_scale(v3_t a, float k) { return shz_vec3_scale(a, k); }
static inline v3_t v3_cross(v3_t a, v3_t b) { return shz_vec3_cross(a, b); }
static inline v3_t v3_unit(v3_t a) { return shz_vec3_normalize_safe(a); }
static inline float v3_dot(v3_t a, v3_t b) { return shz_vec3_dot(a, b); }
static inline float v3_len(v3_t a) { return shz_vec3_magnitude(a); }   /* 0 for the zero vector (SH4ZAM 0.10) */
static inline float inv(float x) { return shz_invf(x); }
static inline float root(float x) { return shz_sqrtf(x); }
static inline float absf(float x) { return shz_fabsf(x); }
static inline float smooth(float x, float e0, float e1) { return shz_smoothstepf_safe(x, e0, e1); }
#else
typedef struct { float x, y, z; } v3_t;

static v3_t v3(float x, float y, float z) { v3_t r; r.x = x; r.y = y; r.z = z; return r; }
static v3_t v3_add(v3_t a, v3_t b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static v3_t v3_sub(v3_t a, v3_t b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static v3_t v3_scale(v3_t a, float k) { return v3(a.x * k, a.y * k, a.z * k); }
static v3_t v3_cross(v3_t a, v3_t b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float v3_dot(v3_t a, v3_t b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float v3_len(v3_t a) { return sqrtf(v3_dot(a, a)); }
static v3_t v3_unit(v3_t a) { const float l = v3_len(a); return l > 0.0f ? v3_scale(a, 1.0f / l) : v3(0.0f, 0.0f, 0.0f); }
static float inv(float x) { return 1.0f / x; }
static float root(float x) { return sqrtf(x); }
static float absf(float x) { return fabsf(x); }

static float smooth(float x, float e0, float e1) {
    float t;

    if(e0 == e1)
        return x < e0 ? 0.0f : 1.0f;
    t = sh4dow_sat_((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
#endif

static v3_t v3_of(const float p[3]) { return v3(p[0], p[1], p[2]); }

/* Smaller than this is "no horizontal part": a light straight overhead, a facing straight up. */
#define FLAT_EPS 1.0e-4f

void sh4dow_foot_params_default(sh4dow_foot_params_t *params) {
    params->min_len = SH4DOW_FOOT_MIN_LEN;
    params->gain = SH4DOW_FOOT_GAIN;
    params->max_slope = SH4DOW_FOOT_MAX_SLOPE;
    params->min_facing = SH4DOW_FOOT_MIN_FACING;
    params->heel_back = SH4DOW_FOOT_HEEL_BACK;
    params->width = SH4DOW_FOOT_WIDTH;
    params->fade_height = SH4DOW_FOOT_FADE_HEIGHT;
    params->lift = SH4DOW_FOOT_LIFT;
    params->width_min = SH4DOW_FOOT_WIDTH_MIN;
    params->width_max = SH4DOW_FOOT_WIDTH_MAX;
    params->range = SH4DOW_FOOT_RANGE;
    params->range_fade = SH4DOW_FOOT_RANGE_FADE;
}

void sh4dow_foot_params_scale(sh4dow_foot_params_t *params, float units_per_unit) {
    params->fade_height *= units_per_unit;
    params->lift *= units_per_unit;
    params->width_min *= units_per_unit;
    params->width_max *= units_per_unit;
    params->range *= units_per_unit;
    params->range_fade *= units_per_unit;
}

void sh4dow_foot_place(sh4dow_foot_t *foot, const sh4dow_foot_params_t *params,
                       const float centre[3], float sphere_radius,
                       const float forward[3], const float up[3]) {
    sh4dow_foot_params_t shipped;
    v3_t c, f, u, flat, heel;
    float fl, r;

    if(!foot || !centre || !forward || !up)
        return;
    if(!params) {
        sh4dow_foot_params_default(&shipped);
        params = &shipped;
    }
    c = v3_of(centre);
    f = v3_of(forward);
    u = v3_of(up);
    flat = v3_sub(f, v3_scale(u, v3_dot(f, u)));                /* the facing across up: the height stays */
    fl = v3_len(flat);
    heel = c;
    if(fl > FLAT_EPS)
        heel = v3_sub(c, v3_scale(flat, params->heel_back * sphere_radius * inv(fl)));
    foot->heel[0] = heel.x;
    foot->heel[1] = heel.y;
    foot->heel[2] = heel.z;
    r = sphere_radius * params->width;
    foot->radius = r < params->width_min ? params->width_min : r > params->width_max ? params->width_max : r;
}

float sh4dow_foot_cone(const sh4dow_foot_t *foot, const sh4dow_foot_params_t *params,
                       const float floor_point[3], const float floor_normal[3],
                       const float up[3], const float forward[3], float quad[4][3]) {
    sh4dow_foot_params_t shipped;
    v3_t n, base, away, flat, side;
    float height, fade, down, across, slope, len;
    int k;

    if(!foot || !floor_point || !floor_normal || !up || !forward || !quad)
        return 0.0f;
    if(!params) {
        sh4dow_foot_params_default(&shipped);
        params = &shipped;
    }
    if(!(params->fade_height > 0.0f))
        return 0.0f;
    n = v3_unit(v3_of(floor_normal));
    if(!(v3_dot(n, v3_of(up)) > params->min_facing) || !(foot->radius > 0.0f))
        return 0.0f;                                            /* a wall or a ceiling under the foot */

    /* The heel's height over the plane: full on it, gone at fade_height. */
    height = v3_dot(v3_sub(v3_of(foot->heel), v3_of(floor_point)), n);
    fade = sh4dow_sat_(1.0f - height * inv(params->fade_height));
    if(fade <= 0.0f || height < -params->fade_height)
        return 0.0f;                                            /* off the floor, or the plane is a storey up */
    base = v3_add(v3_sub(v3_of(foot->heel), v3_scale(n, height)), v3_scale(n, params->lift));

    /* The light's travel into the floor and across it: the slope sets the length. */
    away = v3_of(foot->away);
    down = -v3_dot(away, n);
    flat = v3_sub(away, v3_scale(n, v3_dot(away, n)));
    across = v3_len(flat);
    slope = down > FLAT_EPS ? across * inv(down) : params->max_slope;
    if(slope > params->max_slope)
        slope = params->max_slope;
    if(across > FLAT_EPS) {
        flat = v3_scale(flat, inv(across));
    }
    else {                                                      /* the light straight overhead: the facing, flattened */
        const v3_t h = v3_of(forward);

        flat = v3_unit(v3_sub(h, v3_scale(n, v3_dot(h, n))));
        slope = 0.0f;
    }
    side = v3_cross(n, flat);
    len = foot->radius * (params->min_len + params->gain * slope);

    for(k = 0; k < 4; k++) {                                    /* TL, TR at the heel; BL, BR at the far end */
        const float sx = (k & 1) ? 1.0f : -1.0f, along = (k & 2) ? len : 0.0f;
        const v3_t q = v3_add(base, v3_add(v3_scale(side, sx * foot->radius), v3_scale(flat, along)));

        quad[k][0] = q.x;
        quad[k][1] = q.y;
        quad[k][2] = q.z;
    }
    return fade;
}

float sh4dow_foot_range_fade(const sh4dow_foot_params_t *params, float dist, float zoom) {
    sh4dow_foot_params_t shipped;
    float z, end;

    if(!params) {
        sh4dow_foot_params_default(&shipped);
        params = &shipped;
    }
    z = zoom > 1.0f ? zoom : 1.0f;
    end = params->range * z;
    if(!(dist < end))
        return 0.0f;
    return 1.0f - smooth(dist, end - params->range_fade * z, end);
}

/* In units of the cone's half width across (x, -1..1) and two half widths along
   (0 at the heel end): a round heel of 0.6 at 2 * APEX, the cone widening to the
   full width 1.2 half widths past it, the shade falling to a third at the far end. */
float sh4dow_foot_texel(float u, float v) {
    const float heel = 2.0f * SH4DOW_FOOT_APEX, x = u * 2.0f - 1.0f, along = v * 2.0f, d = along - heel;
    float half, edge, tail, end;

    if(d <= 0.0f)
        return 1.0f - smooth(root(x * x + d * d), 0.25f, 0.6f);
    half = 0.6f + 0.4f * sh4dow_sat_(d * 0.8333333f);
    edge = 1.0f - smooth(absf(x), 0.45f * half, half);
    tail = 1.0f - 0.67f * smooth(d, 0.0f, 2.0f - heel);
    end = 1.0f - smooth(along, 1.75f, 2.0f);
    return edge * tail * end;
}

void sh4dow_foot_texture(uint8_t *out, unsigned side) {
    unsigned x, y;
    float step;

    if(!out || !side)
        return;
    step = inv((float)side);
    for(y = 0; y < side; y++)
        for(x = 0; x < side; x++)
            out[y * side + x] = (uint8_t)(sh4dow_foot_texel(((float)x + 0.5f) * step, ((float)y + 0.5f) * step) * 255.0f + 0.5f);
}

void sh4dow_foot_texture565(uint16_t *out, unsigned side) {
    unsigned x, y;
    float step;

    if(!out || !side)
        return;
    step = inv((float)side);
    for(y = 0; y < side; y++)
        for(x = 0; x < side; x++) {
            const float v = sh4dow_foot_texel(((float)x + 0.5f) * step, ((float)y + 0.5f) * step);
            const unsigned g6 = (unsigned)(v * 63.0f + 0.5f), rb = (unsigned)(v * 31.0f + 0.5f);

            out[y * side + x] = (uint16_t)(rb << 11 | g6 << 5 | rb);
        }
}

unsigned sh4dow_pick_feet(const float *centre, const float *radius, unsigned count,
                          unsigned up_axis, unsigned side_axis, float lo, float hi, uint8_t out[2]) {
    const float top = lo + SH4DOW_FOOT_ZONE * (hi - lo);
    unsigned s, n;

    if(!out)
        return 0;
    out[0] = out[1] = SH4DOW_NO_FOOT;
    if(!centre || !radius || up_axis > 2 || side_axis > 2)
        return 0;
    if(count > SH4DOW_NO_FOOT)
        count = SH4DOW_NO_FOOT;
    for(s = 0; s < 2; s++) {
        float best = top;

        for(n = 0; n < count; n++) {
            if(!(radius[n] > 0.0f) || !(centre[n * 3 + up_axis] < best))
                continue;
            if(s && (out[0] == SH4DOW_NO_FOOT ||
                     !(centre[n * 3 + side_axis] * centre[out[0] * 3 + side_axis] < 0.0f)))
                continue;                                       /* the other foot: across the middle */
            best = centre[n * 3 + up_axis];
            out[s] = (uint8_t)n;
        }
    }
    return (out[0] != SH4DOW_NO_FOOT) + (out[1] != SH4DOW_NO_FOOT);
}
