/* SH4DOW-DC

   bench/bench.c
   Dreamcast timing of the three vertex loops on one scene: six figures of 14
   limbs (84 columns), 64 skin vertices a bone (5376 vertices), a leaning
   light. Prints to the screen and to dcload:

     every    sh4dow_strength() per vertex
     list     sh4dow_ball_columns() per bone, FTRV + sh4dow_strength_list() per vertex
     packed   sh4dow_ball_pack() per bone (empty and solid bones skip the vertices),
              FTRV + sh4dow_strength_packed() per vertex

   and each fast loop's largest difference from every. Built with
   SH4DOW_DC_NO_SH4ZAM it runs on a host (sh4dow_project() for the FTRV) to
   check the numbers; host times say nothing about the console.
*/

#include <stdio.h>
#include "sh4dow_dc.h"

#ifndef SH4DOW_DC_NO_SH4ZAM
#include <kos.h>
#include <dc/perfctr.h>

typedef shz_vec3_t proj_t;
#define PROJ_LOAD(fr)        sh4dow_load_xmtrx(fr)
#define PROJ(fr, p)          shz_xmtrx_transform_point3(shz_vec3_init((p)[0], (p)[1], (p)[2]))
#define PER_VERT(ns)         ((ns) / 5 / VERTS)   /* SH-4 cycles at 200 MHz */
#define PER_VERT_UNIT        "cyc"
#else
#include <math.h>
#include <time.h>

typedef struct { float x, y, z; } proj_t;
static proj_t proj_(const sh4dow_frame_t *fr, const float *p) {
    float o[3];
    proj_t q;

    sh4dow_project(fr, p[0], p[1], p[2], o);
    q.x = o[0]; q.y = o[1]; q.z = o[2];
    return q;
}
#define PROJ_LOAD(fr)        ((void)(fr))
#define PROJ(fr, p)          proj_(fr, p)
#define PER_VERT(ns)         ((ns) / VERTS)
#define PER_VERT_UNIT        "ns"

static uint64_t perf_cntr_timer_ns(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}
#endif

#define FIGS   6
#define LIMBS  14
#define VPB    64
#define VERTS  (FIGS * LIMBS * VPB)
#define REPS   30

/* A Z-up figure, 0.7 units tall: position, radius, flags. */
static const float body[LIMBS][5] = {
    {  0.00f, 0.00f, 0.65f, 0.060f, SH4DOW_LIMB_HEAD },
    {  0.00f, 0.00f, 0.58f, 0.035f, SH4DOW_LIMB_NECK },
    {  0.00f, 0.00f, 0.48f, 0.090f, 0 },
    {  0.00f, 0.00f, 0.36f, 0.080f, 0 },
    {  0.12f, 0.00f, 0.50f, 0.035f, 0 },
    { -0.12f, 0.00f, 0.50f, 0.035f, 0 },
    {  0.14f, 0.02f, 0.40f, 0.030f, 0 },
    { -0.14f, 0.02f, 0.40f, 0.030f, 0 },
    {  0.15f, 0.04f, 0.32f, 0.025f, 0 },
    { -0.15f, 0.04f, 0.32f, 0.025f, 0 },
    {  0.05f, 0.00f, 0.26f, 0.050f, 0 },
    { -0.05f, 0.00f, 0.26f, 0.050f, 0 },
    {  0.05f, 0.01f, 0.13f, 0.040f, 0 },
    { -0.05f, 0.01f, 0.13f, 0.040f, 0 },
};

static sh4dow_limb_t limb[FIGS][LIMBS];
static float xyz[VERTS][3] __attribute__((aligned(32)));
static uint8_t own[VERTS];
static float ball[FIGS][LIMBS][4];      /* centre, radius: every vertex of the bone is inside */
static float out_every[VERTS], out_list[VERTS], out_packed[VERTS], out_noskip[VERTS], out_batch[VERTS];
static sh4dow_frame_t frame[FIGS];

static uint32_t lcg = 12345u;
static float rnd(void) {
    lcg = lcg * 1664525u + 1013904223u;
    return (float)(lcg >> 8) * (1.0f / 16777216.0f);
}

static void scene(void) {
    int f, b, v, k;

    for(f = 0; f < FIGS; f++) {
        const float ox = (float)(f % 3) * 1.5f - 1.5f, oy = (float)(f / 3) * 2.0f;

        for(b = 0; b < LIMBS; b++) {
            sh4dow_limb_t *l = &limb[f][b];
            const float r = body[b][3];

            l->position[0] = body[b][0] + ox;
            l->position[1] = body[b][1] + oy;
            l->position[2] = body[b][2];
            l->radius = r;
            l->bone = (uint8_t)b;
            l->flags = (uint8_t)body[b][4];
            for(k = 0; k < 3; k++)
                ball[f][b][k] = l->position[k];
            ball[f][b][3] = r * 1.4f;

            /* Skin: points between 0.9 and 1.4 radii from the joint. */
            for(v = 0; v < VPB; v++) {
                const int i = (f * LIMBS + b) * VPB + v;
                float d[3], len;

                do {
                    for(k = 0; k < 3; k++)
                        d[k] = rnd() * 2.0f - 1.0f;
                    len = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                } while(len > 1.0f || len < 0.01f);
                len = (0.9f + 0.5f * rnd()) * r / sqrtf(len);
                for(k = 0; k < 3; k++)
                    xyz[i][k] = l->position[k] + d[k] * len;
                own[i] = (uint8_t)b;
            }
        }
    }
}

static const float light[3] = { 0.35f, 0.25f, -1.0f };

static void prepare_all(void) {
    int f;

    for(f = 0; f < FIGS; f++)
        sh4dow_prepare(&frame[f], NULL, limb[f], LIMBS, light);
}

static void run_every(void) {
    int f, i;

    for(f = 0; f < FIGS; f++) {
        const sh4dow_axes_t axes = sh4dow_axes(&frame[f]);

        for(i = f * LIMBS * VPB; i < (f + 1) * LIMBS * VPB; i++)
            out_every[i] = sh4dow_strength(&frame[f], &axes, xyz[i][0], xyz[i][1], xyz[i][2], own[i]);
    }
}

static void run_list(void) {
    int f, b, v;

    for(f = 0; f < FIGS; f++) {
        const sh4dow_frame_t *fr = &frame[f];

        for(b = 0; b < LIMBS; b++) {
            uint8_t list[SH4DOW_COLUMN_MAX];
            const int i0 = (f * LIMBS + b) * VPB;
            float c[3];
            unsigned n;

            sh4dow_project(fr, ball[f][b][0], ball[f][b][1], ball[f][b][2], c);
            n = sh4dow_ball_columns(fr, c[0], c[1], c[2], ball[f][b][3], (unsigned)b, list);
            PROJ_LOAD(fr);
            for(v = 0; v < VPB; v++) {
                const proj_t q = PROJ(fr, xyz[i0 + v]);

                out_list[i0 + v] = sh4dow_strength_list(fr, q.x, q.y, q.z, list, n);
            }
        }
    }
}

static unsigned skipped_empty, skipped_solid;
static int use_skips = 1;

static void run_packed(void) {
    static sh4dow_packed_t pk __attribute__((aligned(32)));
    int f, b, v;

    skipped_empty = skipped_solid = 0;
    for(f = 0; f < FIGS; f++) {
        const sh4dow_frame_t *fr = &frame[f];

        for(b = 0; b < LIMBS; b++) {
            const int i0 = (f * LIMBS + b) * VPB;
            float c[3];

            sh4dow_project(fr, ball[f][b][0], ball[f][b][1], ball[f][b][2], c);
            sh4dow_ball_pack(fr, c[0], c[1], c[2], ball[f][b][3], (unsigned)b, &pk);
            if(use_skips && (!pk.count || pk.solid)) {
                const float s = pk.solid ? 1.0f : 0.0f;

                skipped_empty += !pk.count;
                skipped_solid += pk.solid;
                for(v = 0; v < VPB; v++)
                    out_packed[i0 + v] = s;
                continue;
            }
            PROJ_LOAD(fr);
            for(v = 0; v < VPB; v++) {
                const proj_t q = PROJ(fr, xyz[i0 + v]);

                out_packed[i0 + v] = sh4dow_strength_packed(&pk, q.x, q.y, q.z);
            }
        }
    }
}

static void run_batch(void) {
    static sh4dow_packed_t pk __attribute__((aligned(32)));
    int f, b;

    for(f = 0; f < FIGS; f++) {
        const sh4dow_frame_t *fr = &frame[f];

        for(b = 0; b < LIMBS; b++) {
            const int i0 = (f * LIMBS + b) * VPB;
            float c[3];

            sh4dow_project(fr, ball[f][b][0], ball[f][b][1], ball[f][b][2], c);
            sh4dow_ball_pack(fr, c[0], c[1], c[2], ball[f][b][3], (unsigned)b, &pk);
            sh4dow_strength_packed_n(fr, &pk, xyz[i0], sizeof xyz[0], &out_batch[i0], VPB);
        }
    }
}

#ifndef SH4DOW_DC_NO_SH4ZAM
/* A/B rows in one ELF, so code placement is the same for both: the bone loop with the transform only, the C side
   test and the assembly side test. Same shape as sh4dow_strength_packed_n(). */
#define BONE_LOOP(name, BODY) BONE_LOOP2(name, BODY, (void)0)
/* PRE runs at the top of each vertex: prefetch, line allocation. */
#define BONE_LOOP2(name, BODY, PRE)                                                                        \
static void name(const sh4dow_frame_t *fr, const sh4dow_packed_t *p, const float (*v)[3], float *out, int n) { \
    const sh4dow_entry_t *e = p->entry, *ef = e + p->full, *ee = e + p->count;                            \
    const float length = p->length, hold_at = p->hold_at, inv_fade = p->inv_fade;                         \
    int i;                                                                                                \
                                                                                                          \
    (void)e; (void)ef; (void)ee; (void)length; (void)hold_at; (void)inv_fade;                             \
    sh4dow_load_xmtrx(fr);                                                                                \
    for(i = 0; i < n; i++) {                                                                              \
        PRE;                                                                                              \
        const shz_vec3_t q = shz_xmtrx_transform_point3(shz_vec3_init(v[i][0], v[i][1], v[i][2]));        \
        out[i] = BODY;                                                                                    \
    }                                                                                                     \
}
BONE_LOOP(bone_xform, q.x + q.y + q.z)
BONE_LOOP(bone_c, sh4dow_entries_impl_(e, ef, ee, length, hold_at, inv_fade, q.x, q.y, q.z, 0))
BONE_LOOP(bone_asm, sh4dow_entries_impl_(e, ef, ee, length, hold_at, inv_fade, q.x, q.y, q.z, 1))
BONE_LOOP(bone_none, ((void)q, 0.0f))

/* The source two lines (about five vertices) ahead; the output line claimed, not read, at each line start. */
#define PREF_SRC   __builtin_prefetch((const char *)&v[i] + 64)
#define CLAIM_OUT  do { if(!(i & 7)) __asm__ volatile("movca.l r0, @%0" : : "r"(&out[i]) : "memory"); } while(0)
BONE_LOOP2(bone_xform_pref, q.x + q.y + q.z, PREF_SRC)
BONE_LOOP2(bone_c_pref, sh4dow_entries_impl_(e, ef, ee, length, hold_at, inv_fade, q.x, q.y, q.z, 0), PREF_SRC)
BONE_LOOP2(bone_c_claim, sh4dow_entries_impl_(e, ef, ee, length, hold_at, inv_fade, q.x, q.y, q.z, 0), CLAIM_OUT)
BONE_LOOP2(bone_c_both, sh4dow_entries_impl_(e, ef, ee, length, hold_at, inv_fade, q.x, q.y, q.z, 0),
           do { PREF_SRC; CLAIM_OUT; } while(0))

static int ab_pack_only;

static void (*bone_fn)(const sh4dow_frame_t *, const sh4dow_packed_t *, const float (*)[3], float *, int);
static float out_ab[VERTS] __attribute__((aligned(32)));

static void run_ab(void) {
    static sh4dow_packed_t pk __attribute__((aligned(32)));
    int f, b;

    for(f = 0; f < FIGS; f++)
        for(b = 0; b < LIMBS; b++) {
            const int i0 = (f * LIMBS + b) * VPB;
            float c[3];

            sh4dow_project(&frame[f], ball[f][b][0], ball[f][b][1], ball[f][b][2], c);
            sh4dow_ball_pack(&frame[f], c[0], c[1], c[2], ball[f][b][3], (unsigned)b, &pk);
            if(!ab_pack_only)
                bone_fn(&frame[f], &pk, (const float (*)[3])xyz[i0], &out_ab[i0], VPB);
        }
}
#endif

static uint64_t time_ns(void (*fn)(void)) {
    uint64_t t0, best = ~0ull;
    int r;

    for(r = 0; r < REPS; r++) {
        t0 = perf_cntr_timer_ns();
        fn();
        t0 = perf_cntr_timer_ns() - t0;
        if(t0 < best)
            best = t0;
    }
    return best;
}

static float worst(const float *a, const float *b) {
    float w = 0.0f;
    int i;

    for(i = 0; i < VERTS; i++) {
        const float d = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];

        if(d > w)
            w = d;
    }
    return w;
}

int main(int argc, char **argv) {
    uint64_t t_prep, t_every, t_list, t_packed, t_noskip, t_batch;
    char line[8][96];
    int i, shaded = 0;

    (void)argc; (void)argv;
#ifndef SH4DOW_DC_NO_SH4ZAM
    if(!perf_cntr_timer_enabled())
        perf_cntr_timer_enable();
#endif
    scene();
    prepare_all();

    t_prep = time_ns(prepare_all);
    t_every = time_ns(run_every);
    t_list = time_ns(run_list);
    use_skips = 0;
    t_noskip = time_ns(run_packed);
    for(i = 0; i < VERTS; i++)
        out_noskip[i] = out_packed[i];
    use_skips = 1;
    t_packed = time_ns(run_packed);
    t_batch = time_ns(run_batch);
    for(i = 0; i < VERTS; i++)
        shaded += out_every[i] > 0.0f;

    snprintf(line[0], sizeof line[0], "SH4DOW bench: %d verts, %d columns, %d shaded", VERTS, FIGS * LIMBS, shaded);
    snprintf(line[1], sizeof line[1], "prepare x%d  %6lu us", FIGS, (unsigned long)(t_prep / 1000));
    snprintf(line[2], sizeof line[2], "every       %6lu us  %4lu " PER_VERT_UNIT "/vert", (unsigned long)(t_every / 1000),
             (unsigned long)PER_VERT(t_every));
    snprintf(line[3], sizeof line[3], "list        %6lu us  %4lu " PER_VERT_UNIT "/vert  diff %.6f", (unsigned long)(t_list / 1000),
             (unsigned long)PER_VERT(t_list), (double)worst(out_every, out_list));
    snprintf(line[4], sizeof line[4], "packed      %6lu us  %4lu " PER_VERT_UNIT "/vert  diff %.6f", (unsigned long)(t_packed / 1000),
             (unsigned long)PER_VERT(t_packed), (double)worst(out_every, out_packed));
    snprintf(line[6], sizeof line[6], "packed, no skips %6lu us  %4lu " PER_VERT_UNIT "/vert  diff %.6f",
             (unsigned long)(t_noskip / 1000), (unsigned long)PER_VERT(t_noskip), (double)worst(out_every, out_noskip));
    snprintf(line[7], sizeof line[7], "packed_n    %6lu us  %4lu " PER_VERT_UNIT "/vert  diff %.6f",
             (unsigned long)(t_batch / 1000), (unsigned long)PER_VERT(t_batch), (double)worst(out_every, out_batch));
    snprintf(line[5], sizeof line[5], "packed skips: %u empty, %u solid bones of %d", skipped_empty, skipped_solid,
             FIGS * LIMBS);

#ifndef SH4DOW_DC_NO_SH4ZAM
    {
        static const char *const name[] = { "ab pack only", "ab loop, no work", "ab xform only", "ab xform+pref",
                                            "ab C walk", "ab asm walk", "ab C+pref", "ab C+movca", "ab C+pref+movca" };
        void (*const fn[])(const sh4dow_frame_t *, const sh4dow_packed_t *, const float (*)[3], float *, int) = {
            bone_none, bone_none, bone_xform, bone_xform_pref, bone_c, bone_asm, bone_c_pref, bone_c_claim, bone_c_both };
        int k;

        for(k = 0; k < (int)(sizeof fn / sizeof fn[0]); k++) {
            uint64_t t;

            bone_fn = fn[k];
            ab_pack_only = k == 0;
            t = time_ns(run_ab);
            printf("%-16s %6lu us  %4lu " PER_VERT_UNIT "/vert  diff %.6f\n", name[k], (unsigned long)(t / 1000),
                   (unsigned long)PER_VERT(t), k >= 4 ? (double)worst(out_every, out_ab) : 0.0);
        }
    }
#endif
    for(i = 0; i < 8; i++) {
        printf("%s\n", line[i]);
#ifndef SH4DOW_DC_NO_SH4ZAM
        bfont_draw_str_vram_fmt(24, 40 + i * 30, true, "%s", line[i]);
#endif
    }
#ifndef SH4DOW_DC_NO_SH4ZAM
    for(;;)
        thd_sleep(1000);
#endif
    return 0;
}
