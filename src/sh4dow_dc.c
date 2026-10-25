/* SH4DOW-DC

   sh4dow_dc.c
   Copyright (C) 2026 Emerald May
   See ../LICENSE.
*/

#include "sh4dow_dc.h"

/* Slack on the FULL test, in vertex units: t = vt - t0 rounds, so a ball this
   close to the end of the fade-in or the start of the fade-out stays general. */
#define FULL_SLACK 1.0e-5f

/* A bit test: under -ffast-math a float compare against NaN folds away. */
static int is_finite(float x) {
    union { float f; uint32_t u; } v;

    v.f = x;
    return (v.u & 0x7f800000u) != 0x7f800000u;
}

static float clamp01(float x) {
    if(x < 0.0f)
        return 0.0f;
    if(x > 1.0f)
        return 1.0f;
    return x;
}

static float recip(float x) {
#ifndef SH4DOW_DC_NO_SH4ZAM
    return shz_invf(x);
#else
    return 1.0f / x;
#endif
}

static int params_valid(const sh4dow_params_t *p) {
    return is_finite(p->length) && p->length > 0.0f &&
           is_finite(p->inner_scale) && is_finite(p->outer_scale) &&
           p->inner_scale >= 0.0f && p->outer_scale > p->inner_scale &&
           is_finite(p->hold) && is_finite(p->dark) &&
           is_finite(p->collar) && is_finite(p->fade_in);
}

/* The unit light axis and two side axes. The helper is the world axis least
   aligned with the light, so the cross product never degenerates. */
static int basis(sh4dow_frame_t *f, const float d[3]) {
#ifndef SH4DOW_DC_NO_SH4ZAM
    shz_vec3_t dn, h, un, wn;
    float ex, ey, ez;

    dn = shz_vec3_normalize_safe(shz_vec3_init(d[0], d[1], d[2]));
    if(dn.x == 0.0f && dn.y == 0.0f && dn.z == 0.0f)
        return 0;
    ex = shz_fabsf(dn.x);
    ey = shz_fabsf(dn.y);
    ez = shz_fabsf(dn.z);
    h = (ex <= ey && ex <= ez) ? shz_vec3_init(1.0f, 0.0f, 0.0f) :
        (ey <= ez)             ? shz_vec3_init(0.0f, 1.0f, 0.0f) :
                                 shz_vec3_init(0.0f, 0.0f, 1.0f);
    un = shz_vec3_normalize_safe(shz_vec3_cross(dn, h));
    wn = shz_vec3_cross(dn, un);
    f->dir[0] = dn.x; f->dir[1] = dn.y; f->dir[2] = dn.z;
    f->u[0]   = un.x; f->u[1]   = un.y; f->u[2]   = un.z;
    f->w[0]   = wn.x; f->w[1]   = wn.y; f->w[2]   = wn.z;
#else
    float x = d[0], y = d[1], z = d[2], len, hx, hy, hz, cx, cy, cz;

    len = sqrtf(x * x + y * y + z * z);
    if(!(len > 0.0f))
        return 0;
    x /= len; y /= len; z /= len;
    hx = hy = hz = 0.0f;
    if(fabsf(x) <= fabsf(y) && fabsf(x) <= fabsf(z))
        hx = 1.0f;
    else if(fabsf(y) <= fabsf(z))
        hy = 1.0f;
    else
        hz = 1.0f;
    cx = y * hz - z * hy;
    cy = z * hx - x * hz;
    cz = x * hy - y * hx;
    len = sqrtf(cx * cx + cy * cy + cz * cz);
    if(!(len > 0.0f))
        return 0;
    cx /= len; cy /= len; cz /= len;
    f->dir[0] = x;  f->dir[1] = y;  f->dir[2] = z;
    f->u[0]   = cx; f->u[1]   = cy; f->u[2]   = cz;
    f->w[0] = y * cz - z * cy;
    f->w[1] = z * cx - x * cz;
    f->w[2] = x * cy - y * cx;
#endif
    return 1;
}

/* p onto the frame's three axes. */
static void project3(const sh4dow_frame_t *f, const float p[3], float out[3]) {
#ifndef SH4DOW_DC_NO_SH4ZAM
    const shz_vec3_t r = shz_vec3_dot3(shz_vec3_init(p[0], p[1], p[2]),
                                       shz_vec3_init(f->dir[0], f->dir[1], f->dir[2]),
                                       shz_vec3_init(f->u[0], f->u[1], f->u[2]),
                                       shz_vec3_init(f->w[0], f->w[1], f->w[2]));

    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
#else
    out[0] = p[0] * f->dir[0] + p[1] * f->dir[1] + p[2] * f->dir[2];
    out[1] = p[0] * f->u[0] + p[1] * f->u[1] + p[2] * f->u[2];
    out[2] = p[0] * f->w[0] + p[1] * f->w[1] + p[2] * f->w[2];
#endif
}

static void column_init(const sh4dow_frame_t *f, sh4dow_column_t *c,
                        const sh4dow_params_t *p, const sh4dow_limb_t *limb) {
    const float r = limb->radius;
    float proj[3], feather;

    project3(f, limb->position, proj);
    c->lt = proj[0];
    c->lu = proj[1];
    c->lw = proj[2];
    c->from = 0.0f;
    c->inner = p->inner_scale * r;
    c->outer = p->outer_scale * r;
    c->outer2 = c->outer * c->outer;
    c->inner2 = c->inner * c->inner;
    feather = c->outer - c->inner;
    c->inv_feather = feather > 0.0f ? recip(feather) : 0.0f;
    c->fade_w = p->fade_in > 0.0f ? p->fade_in * r : 0.0f;
    c->top = c->fade_w > 0.0f ? recip(c->fade_w) : 1.0e8f;
    c->bone = limb->bone;
}

/* sh4dow_strength_list()'s tables, relative to the mean joint so the disc
   test's expanded square does not cancel for a character far from the origin. */
static void fast_tables(sh4dow_frame_t *f, const float sum[3]) {
    const float inv_n = recip((float)f->count);
    float origin[3], o[3];
    int i;

    for(i = 0; i < 3; i++)
        origin[i] = sum[i] * inv_n;
    project3(f, origin, o);
    f->ot = o[0];
    f->ou = o[1];
    f->ow = o[2];

    for(i = 0; i < 3; i++) {
        f->rows[0][i] = f->dir[i];
        f->rows[1][i] = f->u[i];
        f->rows[2][i] = f->w[i];
    }
    f->rows[0][3] = -f->ot;
    f->rows[1][3] = -f->ou;
    f->rows[2][3] = -f->ow;

    for(i = 0; i < f->count; i++) {
        const sh4dow_column_t *c = &f->column[i];
        const float lu = c->lu - f->ou, lw = c->lw - f->ow;
        sh4dow_disc_t *d = &f->disc[i];

        d->c[0] = -2.0f * lu;
        d->c[1] = -2.0f * lw;
        d->c[2] = 1.0f;
        d->c[3] = lu * lu + lw * lw - c->outer2;
        d->core = c->inner2 - c->outer2;
        d->inner = c->inner;
        d->inv_feather = c->inv_feather;
        d->outer2 = c->outer2;
        f->t0r[i] = c->t0 - f->ot;
    }
}

void sh4dow_params_default(sh4dow_params_t *params) {
    params->inner_scale = SH4DOW_INNER;
    params->outer_scale = SH4DOW_OUTER;
    params->length = SH4DOW_LENGTH;
    params->hold = SH4DOW_HOLD;
    params->dark = SH4DOW_DARK;
    params->collar = SH4DOW_COLLAR;
    params->fade_in = SH4DOW_FADE_IN;
}

int sh4dow_prepare(sh4dow_frame_t *frame, const sh4dow_params_t *params,
                   const sh4dow_limb_t *limbs, unsigned limb_count,
                   const float direction[3]) {
    sh4dow_params_t shipped;
    sh4dow_column_t *col;
    float sum[3] = { 0.0f, 0.0f, 0.0f }, span, head_r = 0.0f, neck_r = 0.0f;
    int head = -1, neck = -1, i, j;
    unsigned n;

    if(!frame)
        return 0;
    frame->count = 0;
    if(!params) {
        sh4dow_params_default(&shipped);
        params = &shipped;
    }
    if(!params_valid(params) || !limbs || !limb_count || !direction ||
       !is_finite(direction[0]) || !is_finite(direction[1]) || !is_finite(direction[2]) ||
       !basis(frame, direction))
        return 0;

    frame->length = params->length;
    frame->hold = clamp01(params->hold);
    frame->dark = clamp01(params->dark);
    frame->hold_at = frame->hold * frame->length;
    span = frame->length - frame->hold_at;
    frame->inv_fade = span > 0.0f ? recip(span) : 0.0f;

    col = frame->column;
    for(n = 0; n < limb_count && n < SH4DOW_COLUMN_MAX; n++) {
        const sh4dow_limb_t *l = &limbs[n];

        if(!(l->radius > 0.0f) || !is_finite(l->position[0]) ||
           !is_finite(l->position[1]) || !is_finite(l->position[2]))
            continue;
        column_init(frame, &col[frame->count], params, l);
        for(i = 0; i < 3; i++)
            sum[i] += l->position[i];
        if((l->flags & SH4DOW_LIMB_HEAD) && head < 0) {
            head = frame->count;
            head_r = l->radius;
        }
        if((l->flags & SH4DOW_LIMB_NECK) && neck < 0) {
            neck = frame->count;
            neck_r = l->radius;
        }
        frame->count++;
    }
    if(!frame->count)
        return 0;

    if(head >= 0) {
        float from = head_r;

        if(neck >= 0) {
            from = col[neck].lt + params->collar * neck_r - col[head].lt;
            if(!(from > 0.0f))
                from = head_r;
        }
        col[head].from = from;
    }

    /* Stable insertion sort by start; 32 columns at most, once a frame. */
    for(i = 0; i < frame->count; i++) {
        sh4dow_column_t key;

        col[i].t0 = col[i].lt + col[i].from;
        key = col[i];
        for(j = i - 1; j >= 0 && col[j].t0 > key.t0; j--)
            col[j + 1] = col[j];
        col[j + 1] = key;
    }
    frame->first = col[0].t0;
    frame->last = col[frame->count - 1].t0 + frame->length;

    fast_tables(frame, sum);
    return frame->count;
}

/* Squared distance across the light from (lu, lw) to the segment from a along (su, sw). */
static float seg_dist2(float lu, float lw, const float a[3], float su, float sw, float inv_seg2) {
    float s = ((lu - a[1]) * su + (lw - a[2]) * sw) * inv_seg2, du, dw;

    if(s < 0.0f)
        s = 0.0f;
    if(s > 1.0f)
        s = 1.0f;
    du = lu - (a[1] + su * s);
    dw = lw - (a[2] + sw * s);
    return du * du + dw * dw;
}

unsigned sh4dow_capsule_columns(const sh4dow_frame_t *frame, const float a[3], float ra,
                                const float b[3], float rb, unsigned own,
                                uint8_t out[SH4DOW_COLUMN_MAX]) {
    float key[SH4DOW_COLUMN_MAX], lo, hi, r, su, sw, seg2, inv_seg2;
    unsigned n = 0, full = 0, j, k;

    if(!frame || frame->count <= 0 || !out || !a || !b)
        return 0;
    if(!(ra >= 0.0f))
        ra = 0.0f;
    if(!(rb >= 0.0f))
        rb = 0.0f;

    /* The hull lies within max(ra, rb) of the segment a..b and spans lo..hi along the light. */
    r = ra > rb ? ra : rb;
    lo = a[0] - ra < b[0] - rb ? a[0] - ra : b[0] - rb;
    hi = a[0] + ra > b[0] + rb ? a[0] + ra : b[0] + rb;
    su = b[1] - a[1];
    sw = b[2] - a[2];
    seg2 = su * su + sw * sw;
    inv_seg2 = seg2 > 0.0f ? recip(seg2) : 0.0f;

    for(j = 0; j < (unsigned)frame->count; j++) {
        const sh4dow_column_t *c = &frame->column[j];
        const float t0 = frame->t0r[j], reach = r + c->outer;
        float d2;

        if(hi <= t0)
            break;              /* This column and every later one start past the hull. */
        if(lo >= t0 + frame->length || (own == c->bone && own != SH4DOW_BONE_NONE))
            continue;
        d2 = seg_dist2(c->lu - frame->ou, c->lw - frame->ow, a, su, sw, inv_seg2);
        if(d2 >= reach * reach)
            continue;

        if(lo > t0 + c->fade_w + FULL_SLACK && hi < t0 + frame->hold_at - FULL_SLACK) {
            /* Shift the general entries up one; insert among the FULL ones by distance. */
            for(k = n; k > full; k--)
                out[k] = out[k - 1];
            for(k = full; k > 0 && key[k - 1] > d2; k--) {
                out[k] = out[k - 1];
                key[k] = key[k - 1];
            }
            out[k] = (uint8_t)(j | SH4DOW_LIST_FULL);
            key[k] = d2;
            full++;
            n++;
        }
        else {
            out[n++] = (uint8_t)j;
        }
    }

    return n;
}

unsigned sh4dow_ball_columns(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                             unsigned own, uint8_t out[SH4DOW_COLUMN_MAX]) {
    const float c[3] = { ct, cu, cw };

    return sh4dow_capsule_columns(frame, c, r, c, r, own, out);
}

void sh4dow_shade(const sh4dow_frame_t *frame,
                  const float *xyz, size_t xyz_stride,
                  const uint8_t *bone0, size_t bone_stride,
                  float *rgb, size_t rgb_stride,
                  unsigned count) {
    sh4dow_axes_t axes;
    unsigned i;

    if(!frame || frame->count <= 0 || !xyz || !rgb || !count)
        return;
    axes = sh4dow_axes(frame);

    for(i = 0; i < count; i++) {
        const float *p = (const float *)((const char *)xyz + i * xyz_stride);
        const unsigned own = bone0 ? bone0[i * bone_stride] : SH4DOW_BONE_NONE;
        const float s = sh4dow_strength(frame, &axes, p[0], p[1], p[2], own);

        if(s > 0.0f) {
            float *c = (float *)((char *)rgb + i * rgb_stride);
            const float k = 1.0f - frame->dark * s;

            c[0] *= k;
            c[1] *= k;
            c[2] *= k;
        }
    }
}
