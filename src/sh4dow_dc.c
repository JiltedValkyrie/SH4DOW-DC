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

/* 1 / x for x > 0: every caller has checked the sign, so no shz_invf() sign fix. */
static float recip_pos(float x) {
#ifndef SH4DOW_DC_NO_SH4ZAM
    return shz_invf_fsrra(x);
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
    shz_vec3_t dn, un, wn;

    dn = shz_vec3_normalize_safe(shz_vec3_init(d[0], d[1], d[2]));
    if(dn.x == 0.0f && dn.y == 0.0f && dn.z == 0.0f)
        return 0;
    un = shz_vec3_normalize_safe(shz_vec3_perp(dn));   /* dn x that world axis */
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
    c->inv_feather = feather > 0.0f ? recip_pos(feather) : 0.0f;
    c->fade_w = p->fade_in > 0.0f ? p->fade_in * r : 0.0f;
    c->top = c->fade_w > 0.0f ? recip_pos(c->fade_w) : 1.0e8f;
    c->bone = limb->bone;
}

/* sh4dow_strength_list()'s tables, relative to the mean joint so the disc
   test's expanded square does not cancel for a character far from the origin. */
static void fast_tables(sh4dow_frame_t *f, const float sum[3]) {
    const float inv_n = recip_pos((float)f->count);
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

        f->entry[i].lu = lu;
        f->entry[i].lw = lw;
        f->entry[i].outer2 = c->outer2;
        f->entry[i].inner2 = c->inner2 > 1.0e-30f ? c->inner2 : 1.0e-30f;   /* a feather hit then has perp2 > 0 */
        f->entry[i].inner = c->inner;
        f->entry[i].inv_feather = c->inv_feather;
        f->entry[i].t0r = f->t0r[i];
        f->entry[i].top = c->top;
        f->cull[i].outer = c->outer;
        f->cull[i].fade_w = c->fade_w;
        f->cull[i].bone = c->bone;
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
    frame->hold = sh4dow_sat_(params->hold);
    frame->dark = sh4dow_sat_(params->dark);
    frame->hold_at = frame->hold * frame->length;
    span = frame->length - frame->hold_at;
    frame->inv_fade = span > 0.0f ? recip_pos(span) : 0.0f;

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
    const float s = sh4dow_sat_(((lu - a[1]) * su + (lw - a[2]) * sw) * inv_seg2);

    return sh4dow_len2_(lu - (a[1] + su * s), lw - (a[2] + sw * s));
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
    seg2 = sh4dow_len2_(su, sw);
    inv_seg2 = seg2 > 0.0f ? recip_pos(seg2) : 0.0f;

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

/* 32 bytes as eight words: a struct assignment this size is a __movmem call on the SH-4. */
static void copy_entry(sh4dow_entry_t *dst, const sh4dow_entry_t *src) {
    const uint32_t *a = (const uint32_t *)src;
    uint32_t *b = (uint32_t *)dst;
    const uint32_t w0 = a[0], w1 = a[1], w2 = a[2], w3 = a[3], w4 = a[4], w5 = a[5], w6 = a[6], w7 = a[7];

    b[0] = w0; b[1] = w1; b[2] = w2; b[3] = w3; b[4] = w4; b[5] = w5; b[6] = w6; b[7] = w7;
}

static void pack_head(const sh4dow_frame_t *frame, sh4dow_packed_t *out) {
    out->length = frame->length;
    out->hold_at = frame->hold_at;
    out->inv_fade = frame->inv_fade;
}

unsigned sh4dow_pack(const sh4dow_frame_t *frame, const uint8_t *list, unsigned n, sh4dow_packed_t *out) {
    unsigned k, full = 0;

    if(!out)
        return 0;
    out->count = out->full = out->solid = 0;
    if(!frame || !list || frame->count <= 0)
        return 0;
    if(n > SH4DOW_COLUMN_MAX)
        n = SH4DOW_COLUMN_MAX;
    pack_head(frame, out);

    for(k = 0; k < n; k++) {
        copy_entry(&out->entry[k], &frame->entry[list[k] & SH4DOW_LIST_INDEX]);
        if(full == k && (list[k] & SH4DOW_LIST_FULL))
            full++;             /* A FULL entry after a general one stays general: the same strength. */
    }
    out->full = (uint8_t)full;
    out->count = (uint8_t)n;
    return n;
}

/* GCC's induction variable pass gives every entry field its own pointer, rebuilt per vertex: off, it walks one. */
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-ivopts")))
#endif
void sh4dow_strength_packed_n(const sh4dow_frame_t *frame, const sh4dow_packed_t *p,
                              const float *xyz, size_t xyz_stride, float *out, unsigned count) {
    const sh4dow_entry_t *e, *end_full, *end;
    const char *src = (const char *)xyz;
    float length, hold_at, inv_fade;
    unsigned i;

    if(!frame || !p || !out || !count)
        return;
    if(!p->count || p->solid) {
        const float s = p->solid ? 1.0f : 0.0f;

        for(i = 0; i < count; i++)
            out[i] = s;
        return;
    }
    if(!xyz)
        return;
    e = p->entry;
    end_full = e + p->full;
    end = e + p->count;
    length = p->length;
    hold_at = p->hold_at;
    inv_fade = p->inv_fade;

#ifndef SH4DOW_DC_NO_SH4ZAM
    sh4dow_load_xmtrx(frame);
    for(i = 0; i < count; i++, src += xyz_stride) {
        const float *v = (const float *)src;
        const shz_vec3_t q = shz_xmtrx_transform_point3(shz_vec3_init(v[0], v[1], v[2]));

        out[i] = sh4dow_entries_(e, end_full, end, length, hold_at, inv_fade, q.x, q.y, q.z);
    }
#else
    for(i = 0; i < count; i++, src += xyz_stride) {
        const float *v = (const float *)src;
        float q[3];

        sh4dow_project(frame, v[0], v[1], v[2], q);
        out[i] = sh4dow_entries_(e, end_full, end, length, hold_at, inv_fade, q[0], q[1], q[2]);
    }
#endif
}

/* Squared distance across the light from (lu, lw) to the point p. */
static float point_dist2(float lu, float lw, const float p[3]) {
    return sh4dow_len2_(lu - p[1], lw - p[2]);
}

/* sh4dow_capsule_columns() and sh4dow_pack() in one pass over the frame's ready entries and reach table; the
   list is the same, FULL entries first by distance, then the rest in start order. */
unsigned sh4dow_capsule_pack(const sh4dow_frame_t *frame, const float a[3], float ra,
                             const float b[3], float rb, unsigned own, sh4dow_packed_t *out) {
    uint8_t idx[SH4DOW_COLUMN_MAX];
    float key[SH4DOW_COLUMN_MAX], lo, hi, r, su, sw, seg2, inv_seg2, length, hold_at;
    unsigned n = 0, full = 0, j, k, count, solid = 0;
    const int ball = a && b && a[0] == b[0] && a[1] == b[1] && a[2] == b[2];

    if(!out)
        return 0;
    out->count = out->full = out->solid = 0;
    if(!frame || frame->count <= 0 || !a || !b)
        return 0;
    if(!(ra >= 0.0f))
        ra = 0.0f;
    if(!(rb >= 0.0f))
        rb = 0.0f;
    pack_head(frame, out);

    r = ra > rb ? ra : rb;
    lo = a[0] - ra < b[0] - rb ? a[0] - ra : b[0] - rb;
    hi = a[0] + ra > b[0] + rb ? a[0] + ra : b[0] + rb;
    su = b[1] - a[1];
    sw = b[2] - a[2];
    seg2 = sh4dow_len2_(su, sw);
    inv_seg2 = seg2 > 0.0f ? recip_pos(seg2) : 0.0f;
    length = frame->length;
    hold_at = frame->hold_at;
    count = (unsigned)frame->count;

    for(j = 0; j < count; j++) {
        const sh4dow_entry_t *e = &frame->entry[j];
        const sh4dow_cull_t *c = &frame->cull[j];
        const float t0 = e->t0r, reach = r + c->outer;
        float d2;

        if(hi <= t0)
            break;              /* This column and every later one start past the hull. */
        if(lo >= t0 + length || (own == c->bone && own != SH4DOW_BONE_NONE))
            continue;
        d2 = ball ? point_dist2(e->lu, e->lw, a) : seg_dist2(e->lu, e->lw, a, su, sw, inv_seg2);
        if(d2 >= reach * reach)
            continue;

        if(lo > t0 + c->fade_w + FULL_SLACK && hi < t0 + hold_at - FULL_SLACK) {
            /* Solid: the core holds the whole hull. Its farthest point from the axis is within r of an end,
               the distance being convex along a..b. */
            if(!solid) {
                const float core = e->inner - r, far2 = ball ? d2 :
                    (point_dist2(e->lu, e->lw, a) > point_dist2(e->lu, e->lw, b) ?
                     point_dist2(e->lu, e->lw, a) : point_dist2(e->lu, e->lw, b));

                solid = core > 0.0f && far2 < core * core;
            }
            for(k = n; k > full; k--)
                idx[k] = idx[k - 1];
            for(k = full; k > 0 && key[k - 1] > d2; k--) {
                idx[k] = idx[k - 1];
                key[k] = key[k - 1];
            }
            idx[k] = (uint8_t)j;
            key[k] = d2;
            full++;
            n++;
        }
        else {
            idx[n++] = (uint8_t)j;
        }
    }

    for(k = 0; k < n; k++)
        copy_entry(&out->entry[k], &frame->entry[idx[k]]);
    out->full = (uint8_t)full;
    out->count = (uint8_t)n;
    out->solid = (uint8_t)solid;
    return n;
}

unsigned sh4dow_ball_pack(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                          unsigned own, sh4dow_packed_t *out) {
    const float c[3] = { ct, cu, cw };

    return sh4dow_capsule_pack(frame, c, r, c, r, own, out);
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
