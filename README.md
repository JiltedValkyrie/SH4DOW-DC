# SH4DOW-DC

Soft self-shadow for skinned characters, computed per vertex on the CPU. Each limb casts a soft column away from the light; a vertex inside a column gets darker. No shadow map, no second mesh, no extra draw, no video memory.

And the floor half, **foot cones**: two small shadows per character, one under each foot, each a cone that starts at the heel and stretches away from the light. The library works out the quad, its picture and its fade; you draw the quad.

Three files, C99. Made for the Sega Dreamcast (SH-4) with [SH4ZAM](https://github.com/gyrovorbis/sh4zam) 0.10; builds in plain C anywhere else.

Originally developed for Halo: Dreamcast Evolved.

## Contents

- [What it looks like](#what-it-looks-like)
- [Quick start](#quick-start)
- [How it works](#how-it-works)
- [API](#api)
- [Tuning](#tuning)
- [Foot cones](#foot-cones)
- [Cost](#cost)
- [Build and test](#build-and-test)
- [Credits and license](#credits-and-license)

## What it looks like

<!-- PLACEHOLDER: two screenshots of the same shot, supplied after the next hardware build.
     1. before: the hard limb volumes            2. after: SH4DOW-DC
     Keep the same camera, the same light, the same character. -->

A dark band under the chin, the arm's shade across the chest, the chest's shade down the belly. The edge is a thin feather. The colour under the shade is the lit colour, darker: a red light stays red.

## Quick start

Add `include/` to your include path and `src/sh4dow_dc.c` to your build (`src/sh4dow_foot.c` too if you want the foot cones). On the Dreamcast, SH4ZAM must be on the include path too. Anywhere else, define `SH4DOW_DC_NO_SH4ZAM`.

```c
#include <stdio.h>
#include "sh4dow_dc.h"

int main(void) {
    /* Posed joints in the vertices' space, their sphere radii, their bone ids: a head, a neck, a chest. */
    const sh4dow_limb_t limbs[3] = {
        { { 0.0f, 0.0f, 1.70f }, 0.10f, 12, SH4DOW_LIMB_HEAD },
        { { 0.0f, 0.0f, 1.55f }, 0.06f,  9, SH4DOW_LIMB_NECK },
        { { 0.0f, 0.0f, 1.30f }, 0.18f,  6, 0 },
    };
    const float down[3] = { 0.0f, 0.0f, -1.0f };         /* the way the shadow falls: Z up, light overhead */
    float xyz[3] = { 0.03f, 0.0f, 1.00f };                /* one lit vertex on the chest ... */
    float rgb[3] = { 1.0f, 0.9f, 0.8f };                  /* ... with its lit colour */
    uint8_t bone = 6;                                     /* its first bone: the chest */
    sh4dow_frame_t frame;

    if(sh4dow_prepare(&frame, NULL, limbs, 3, down) <= 0)   /* once per character per frame, after posing */
        return 1;
    sh4dow_shade(&frame, xyz, sizeof(xyz), &bone, 1, rgb, sizeof(rgb), 1);   /* after lighting, before packing */
    printf("rgb %.3f %.3f %.3f\n", rgb[0], rgb[1], rgb[2]);                   /* 0.350 0.315 0.280: under the head and neck */
    return 0;
}
```

That file is `example/quickstart.c`. `make test` builds and runs it; the test suite checks that it matches this page. It prints `rgb 0.350 0.315 0.280`: the vertex sits under the head and neck columns, so it keeps 35% of its colour.

What you pass in:

- **Limbs.** For each limb you want to cast: the joint's position, a sphere radius around it, and the bone id of the vertices that belong to it. Positions and radii in the same space as your vertices (view space or world space, your choice). A skeleton's node sphere times the character's scale is the usual radius.
- **Direction.** The way the shadow falls, from the light toward the dark, in that same space. Any length. With no light, use straight down: `(0, 0, -1)` for Z up, `(0, -1, 0)` for Y up.
- **Vertices.** Position, lit colour, and first bone id. Call after lighting: the shade multiplies the colour by `1 - dark * strength`, it does not replace it.

Flag the head `SH4DOW_LIMB_HEAD` and the neck `SH4DOW_LIMB_NECK` and the head's column starts at the base of the neck, so the chin stays lit. Leave the flags off and every column starts at its joint.

## How it works

- Each limb is a column: a cylinder of radius `outer` that starts at the joint and runs `length` units along the light.
- A vertex inside a column is shaded. Full strength within `inner` of the axis, fading to zero at `outer`. Full along the first `hold` of the length, fading to zero at the end. A short fade-in just past the start, so a vertex crossing the start plane does not pop.
- Columns do not add. The darkest one wins.
- A column skips the vertices of its own bone (first bone only). The other columns still shade them, so the limb is still shaded by the body.
- The colour is scaled: `rgb *= 1 - dark * strength`. At the default `dark` 0.65 a fully shaded vertex keeps 35%.

Two ways to evaluate a vertex:

1. **Every column** (`sh4dow_strength`, `sh4dow_shade`). One dot product puts the vertex on the light axis; a vertex outside every column's span stops there. The columns are sorted by start, so the loop stops at the first column that starts past the vertex.
2. **Column lists** (`sh4dow_ball_columns`, `sh4dow_strength_list`). If you know a ball that holds every vertex of a bone (its posed node sphere, grown by anything that moves vertices after skinning), list once per bone per frame the columns that ball can touch. A column whose full-strength span holds the whole ball is flagged, and costs one dot product and one compare per vertex. A vertex blended between two bones lies in the hull of two balls: `sh4dow_capsule_columns`. The list only drops columns that cannot reach the ball, so the result is the same strength up to float rounding (see Cost).
3. **Packed lists** (`sh4dow_ball_pack`, `sh4dow_capsule_pack`, `sh4dow_strength_packed`). The same list, copied once per bone into consecutive 32-byte entries that hold everything the vertex loop reads, so it walks one pointer instead of indexing the 3 KB frame. The side test is the plain distance to the axis, not the expanded square, so it has no precision note and stays within 1e-5 of the every-column loop. A pack also says when no vertex in the ball can be shaded (`count` 0) or every one is at full strength (`solid`, the ball inside a column's core): skip that bone's projections and write 0 or 1. `sh4dow_strength_packed_n` runs a whole bone: it does those skips, holds the pack's numbers in registers, and issues the next vertex's FTRV before shading the current one, so the transform's latency overlaps the shade. It is the fastest way to use the library when your positions sit in an array.

## API

Everything is in `include/sh4dow_dc.h`. The per-vertex functions are `static inline`: no call per vertex.

| Function | When | What |
| --- | --- | --- |
| `sh4dow_params_default` | once | fills `sh4dow_params_t` with the shipped tuning (the Tuning table) |
| `sh4dow_prepare` | per character, per frame | builds the frame: the columns, sorted by start, and the fast-path tables. Returns the column count; 0 means nothing to shade |
| `sh4dow_shade` | per vertex batch | multiplies float rgb triples by the shade. Byte strides; `bone0` NULL turns the own-bone skip off |
| `sh4dow_axes`, `sh4dow_strength` | per vertex | the strength 0..1 of one vertex against every column, for your own colour math |
| `sh4dow_project` | per ball, per vertex | a point onto the frame's axes, origin-relative. On the Dreamcast, `sh4dow_load_xmtrx` once per frame and one FTRV per point does the same |
| `sh4dow_ball_columns` | per bone, per frame | the columns a ball can touch, at most `frame->count` bytes |
| `sh4dow_capsule_columns` | per bone pair, per frame | the same for the hull of two balls (a blended vertex) |
| `sh4dow_strength_list` | per vertex | the strength from the projection, against a list |
| `sh4dow_strength_rel` | per vertex | the strength from the projection, against every column |
| `sh4dow_ball_pack`, `sh4dow_capsule_pack` | per bone, per frame | the list, packed for the vertex loop, with the empty (`count` 0) and `solid` flags |
| `sh4dow_pack` | per list | packs a list you already have; no `solid` test |
| `sh4dow_strength_packed` | per vertex | the strength from the projection, against a packed list |
| `sh4dow_strength_packed_n` | per bone | the strengths of a bone's vertices into a float array: skips, FTRV overlapped (loads XMTRX on the Dreamcast) |

The prototypes, as the header declares them (the test suite compiles this block against the header):

```c
#include "sh4dow_dc.h"

void sh4dow_params_default(sh4dow_params_t *params);
int sh4dow_prepare(sh4dow_frame_t *frame, const sh4dow_params_t *params,
                   const sh4dow_limb_t *limbs, unsigned limb_count, const float direction[3]);
unsigned sh4dow_ball_columns(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                             unsigned own, uint8_t out[SH4DOW_COLUMN_MAX]);
unsigned sh4dow_capsule_columns(const sh4dow_frame_t *frame, const float a[3], float ra,
                                const float b[3], float rb, unsigned own, uint8_t out[SH4DOW_COLUMN_MAX]);
void sh4dow_shade(const sh4dow_frame_t *frame, const float *xyz, size_t xyz_stride,
                  const uint8_t *bone0, size_t bone_stride, float *rgb, size_t rgb_stride, unsigned count);
static inline sh4dow_axes_t sh4dow_axes(const sh4dow_frame_t *frame);
static inline void sh4dow_project(const sh4dow_frame_t *frame, float x, float y, float z, float out[3]);
static inline float sh4dow_strength(const sh4dow_frame_t *frame, const sh4dow_axes_t *axes,
                                    float x, float y, float z, unsigned own);
static inline float sh4dow_strength_rel(const sh4dow_frame_t *frame, float vt, float vu, float vw, unsigned own);
static inline float sh4dow_strength_list(const sh4dow_frame_t *frame, float vt, float vu, float vw,
                                         const uint8_t *list, unsigned n);
unsigned sh4dow_pack(const sh4dow_frame_t *frame, const uint8_t *list, unsigned n, sh4dow_packed_t *out);
unsigned sh4dow_capsule_pack(const sh4dow_frame_t *frame, const float a[3], float ra,
                             const float b[3], float rb, unsigned own, sh4dow_packed_t *out);
unsigned sh4dow_ball_pack(const sh4dow_frame_t *frame, float ct, float cu, float cw, float r,
                          unsigned own, sh4dow_packed_t *out);
static inline float sh4dow_strength_packed(const sh4dow_packed_t *p, float vt, float vu, float vw);
void sh4dow_strength_packed_n(const sh4dow_frame_t *frame, const sh4dow_packed_t *p,
                              const float *xyz, size_t xyz_stride, float *out, unsigned count);
```

Types: `sh4dow_limb_t` (position, radius, bone, flags), `sh4dow_params_t` (the Tuning table), `sh4dow_frame_t` (about 3 KB: up to `SH4DOW_COLUMN_MAX` = 32 columns and their tables; one per character drawn, or one reused). `own` is a vertex's first bone, or `SH4DOW_BONE_NONE` for no skip. The lists: `SH4DOW_LIST_INDEX` masks the column, `SH4DOW_LIST_FULL` marks an entry the fades do not touch.

Rules of use:

- Call `sh4dow_prepare` after posing and before the vertex pass. `direction` may be any length; a zero or non-finite direction, or bad params, store no columns and return 0.
- The list functions are exact for points inside the ball and say nothing about points outside it. Give the ball a radius that really holds the vertices.
- `sh4dow_strength_list` expands the distance test algebraically, so a column far thinner than its distance from the frame's origin (the mean joint) loses precision: about one colour step at a ratio of 1:100, a wrong answer near 1:3000. Keep such limbs out of the list loop, or use `sh4dow_strength` or `sh4dow_strength_packed`.
- Nothing allocates, touches globals or needs a floating point mode. Separate frames are independent, so separate threads may use separate frames.

## Tuning

Distances are in your vertex units. The `_scale` values multiply the limb's radius. Change one number and look at one character up close before the next.

| Field of `sh4dow_params_t` | Macro | Default | Means | Raise it | Lower it |
| --- | --- | --- | --- | --- | --- |
| `inner_scale` | SH4DOW_INNER | 1.25 | solid core radius, in limb radii | fatter shade | thinner shade |
| `outer_scale` | SH4DOW_OUTER | 1.35 | radius where the shade reaches zero, in limb radii | wider, softer edge | harder edge. Keep it above `inner_scale` |
| `length` | SH4DOW_LENGTH | 1.8 | how far the column runs along the light | shade reaches farther down the body | shade stops near the limb |
| `hold` | SH4DOW_HOLD | 0.8 | fraction of the length at full strength | the end stays dark longer | the column fades sooner |
| `dark` | SH4DOW_DARK | 0.65 | colour lost at full strength | darker. 1.0 leaves black | lighter. 0 leaves the colour alone |
| `collar` | SH4DOW_COLLAR | 0.8 | neck radii past the neck centre where the head column starts | starts lower, misses more of the jaw | starts higher, can stain the chin |
| `fade_in` | SH4DOW_FADE_IN | 0.1 | limb radii past the start before full strength | softer start, less flicker | sharper start |

`sh4dow_params_default` fills these. Pass `NULL` to `sh4dow_prepare` for the same. The header's macros are the source of truth; the test suite checks this table against them.

Budgets the library leaves to you: how many limbs cast (the original game spends 14 on a character: the head, neck, torso and both arms, then the largest joint spheres), how many characters a frame (the original: the six nearest), and any fade with distance.

Symptoms: the shade falls toward the light, flip `direction`. The whole character darkens, `outer_scale` times your radii is bigger than the body. Nothing darkens, the direction is flipped, the radii are in another unit than the vertices, or joints and vertices are in different spaces.

## Foot cones

Two small floor shadows per character, one under each foot. Each is a cone: a round heel that widens away from the light and fades out, long when the light is low and short when it is overhead. It needs no column frame and no vertices; it is a quad, a picture and a fade.

Add `src/sh4dow_foot.c`. The same example, `example/feet.c`, builds and runs in `make test`; the test suite checks that it matches this page. It prints the two cones' quads, each fading to 0.89 because the heel is 0.04 above the floor, and a distance fade:

```c
#include <stdio.h>
#include "sh4dow_dc.h"

int main(void) {
    /* A skeleton's bind pose node spheres, model space, Z up, +Y left: pelvis, left foot, right foot. Height 0 to 1. */
    const float node[3][3] = { { 0.0f, 0.0f, 0.50f }, { 0.0f, 0.10f, 0.04f }, { 0.0f, -0.10f, 0.04f } };
    const float node_radius[3] = { 0.12f, 0.05f, 0.05f };
    const float up[3] = { 0.0f, 0.0f, 1.0f }, forward[3] = { 1.0f, 0.0f, 0.0f };
    const float floor_point[3] = { 0.0f, 0.0f, 0.0f };      /* a flat floor under the character ... */
    const float light[3] = { 1.0f, 0.0f, -1.0f };           /* ... and the brightest light's travel: 45 degrees */
    uint8_t feet[2], texture[32 * 32];
    unsigned i, found = sh4dow_pick_feet(&node[0][0], node_radius, 3, 2, 1, 0.0f, 1.0f, feet);   /* once per model */

    sh4dow_foot_texture(texture, 32);                       /* once, then upload it as your shadow texture */
    for(i = 0; i < found; i++) {                            /* per character per frame */
        sh4dow_foot_t foot;
        float quad[4][3], fade;

        sh4dow_foot_place(&foot, NULL, node[feet[i]], node_radius[feet[i]], forward, up);   /* from the posed foot sphere */
        foot.away[0] = light[0]; foot.away[1] = light[1]; foot.away[2] = light[2];
        fade = sh4dow_foot_cone(&foot, NULL, floor_point, up, up, forward, quad);
        if(fade > 0.0f)                                     /* 0: a wall, or the foot is in the air: no quad */
            printf("foot %u fade %.2f  TL (%.3f, %.3f)  BR (%.3f, %.3f)\n", feet[i], fade, quad[0][0], quad[0][1], quad[3][0], quad[3][1]);
    }
    printf("camera 12: %.2f  19: %.2f  21: %.2f\n", sh4dow_foot_range_fade(NULL, 12.0f, 1.0f),
           sh4dow_foot_range_fade(NULL, 19.0f, 1.0f), sh4dow_foot_range_fade(NULL, 21.0f, 1.0f));
    return 0;
}
```

What you pass in and what you draw:

- **Feet.** Once per model, from its bind pose node spheres: `sh4dow_pick_feet` finds the lowest node and the lowest one on the other side of the middle, and only below the lowest quarter of the model's height (a hovering or legless model has none). You say which axis is up and which is left-right.
- **Place.** Each frame, `sh4dow_foot_place` turns a posed foot sphere into a `sh4dow_foot_t`: the heel is 0.7 sphere radii behind the centre along the character's facing (height kept), the half width 0.8 of the radius, clamped to 3 to 12 cm. Set `foot.away` yourself: the light's travel at the foot, from the light toward it. Use your brightest light near the character, or straight down when there is none.
- **Cone.** `sh4dow_foot_cone` puts the cone on the floor plane under the foot (a point and a normal, from your collision), as a quad: TL and TR at the heel, BL and BR at the far end, texture u across and v along. It returns the fade: 1 with the heel on the floor, 0 at 0.35 above it, so a jumping character's shadow melts away. It returns 0 for a wall, a ceiling, a plane a storey above, or a foot with no radius. The cone's length is 3 foot radii with the light overhead and grows by 3 radii for each unit of the light's slope, to 9 radii.
- **Picture.** `sh4dow_foot_texture` fills a one byte per texel picture, `sh4dow_foot_texture565` the same as grey RGB565 (what the Dreamcast game uploads: 32 x 32 is plenty). Make it once.
- **Distance.** `sh4dow_foot_range_fade` is 1 up to 18 from the camera, 0 at 20, smooth between, with a scope's magnification as `zoom`. How many characters get cones, and the choice of light, are yours.
- **Draw.** A multiply: the floor keeps `1 - texel * strength` of its light. Draw the quad over the floor, a little above it (the cone is already lifted 0.012), with the picture as the texture and a vertex colour of grey `strength` (your darkness times the cone's fade times the distance fade), blended `dst * (1 - src)` (zero / inverse source colour). No depth write.

Units. The distances are in the original game's world units: 1 unit is 3.048 m and a standing character is about 0.7 tall. If yours differ, carry them over once: `sh4dow_foot_params_default(&p); sh4dow_foot_params_scale(&p, 3.048f);` for metres. The radii, slopes and the picture are ratios and do not change. Z up or Y up does not matter: you pass `up`.

| Function | When | What |
| --- | --- | --- |
| `sh4dow_foot_params_default`, `sh4dow_foot_params_scale` | once | the shipped tuning (the table below), and the six distances in another unit |
| `sh4dow_pick_feet` | per model | the two foot nodes of a bind pose |
| `sh4dow_foot_place` | per foot, per frame | the heel and half width from a posed foot sphere |
| `sh4dow_foot_cone` | per foot, per frame | the cone as a quad on a floor plane, and its fade |
| `sh4dow_foot_range_fade` | per character | the fade with distance from the camera |
| `sh4dow_foot_texel`, `sh4dow_foot_texture`, `sh4dow_foot_texture565` | once | the picture, one point or a whole texture |

`NULL` for `params` is the shipped tuning. Everything here is plain arithmetic: no allocation, no globals.

The foot tuning. Lengths and widths are in foot radii; the distances are in the units above.

| Macro | Default | Unit | Means | Raise it | Lower it |
| --- | --- | --- | --- | --- | --- |
| SH4DOW_FOOT_MIN_LEN | 3.0 | radii | cone length with the light overhead | longer stub | shorter, rounder |
| SH4DOW_FOOT_GAIN | 3.0 | radii per slope | extra length per unit of the light's slope (across the floor over into it) | low light stretches more | the length changes less with the light |
| SH4DOW_FOOT_MAX_SLOPE | 2.0 | slope | where the length stops growing (9 radii) | longer streaks near the horizon | capped sooner |
| SH4DOW_FOOT_MIN_FACING | 0.25 | normal . up | the floor normal must point up more than this | steeper floors take a cone | only flat floors |
| SH4DOW_FOOT_HEEL_BACK | 0.7 | sphere radii | the heel's distance behind the foot sphere's centre | starts further back | starts nearer the toe |
| SH4DOW_FOOT_WIDTH | 0.8 | sphere radii | half width at the heel | fatter | thinner |
| SH4DOW_FOOT_WIDTH_MIN | 0.03 | distance | smallest half width | small feet keep a visible cone | |
| SH4DOW_FOOT_WIDTH_MAX | 0.12 | distance | largest half width | | big feet stay neat |
| SH4DOW_FOOT_FADE_HEIGHT | 0.35 | distance | heel height above the floor where the cone is gone | a jumper keeps its shadow longer | gone sooner |
| SH4DOW_FOOT_LIFT | 0.012 | distance | the quad's offset off the floor | no flicker on a rough floor | closer to the floor |
| SH4DOW_FOOT_RANGE | 20.0 | distance | camera distance where the cone is gone | farther cones | nearer |
| SH4DOW_FOOT_RANGE_FADE | 2.0 | distance | the fade out spans this much before the range | softer | a sharper edge |
| SH4DOW_FOOT_APEX | 0.25 | fraction of the cone | where the picture's round heel sits | | |
| SH4DOW_FOOT_ZONE | 0.25 | fraction of the height | feet lie in this lowest part of the model | | |

The header's macros are the source of truth; the test suite checks this table against them. The prototypes, as the header declares them (the test suite compiles this block against the header):

```c
#include "sh4dow_dc.h"

void sh4dow_foot_params_default(sh4dow_foot_params_t *params);
void sh4dow_foot_params_scale(sh4dow_foot_params_t *params, float units_per_unit);
void sh4dow_foot_place(sh4dow_foot_t *foot, const sh4dow_foot_params_t *params, const float centre[3],
                       float sphere_radius, const float forward[3], const float up[3]);
float sh4dow_foot_cone(const sh4dow_foot_t *foot, const sh4dow_foot_params_t *params,
                       const float floor_point[3], const float floor_normal[3],
                       const float up[3], const float forward[3], float quad[4][3]);
float sh4dow_foot_range_fade(const sh4dow_foot_params_t *params, float dist, float zoom);
float sh4dow_foot_texel(float u, float v);
void sh4dow_foot_texture(uint8_t *out, unsigned side);
void sh4dow_foot_texture565(uint16_t *out, unsigned side);
unsigned sh4dow_pick_feet(const float *centre, const float *radius, unsigned count,
                          unsigned up_axis, unsigned side_axis, float lo, float hi, uint8_t out[2]);
```

Cost: a cone is a few dozen multiplies, a handful of dot products and three reciprocals, once per foot per frame; the picture is made once. Not yet measured on hardware.

## Cost

**Measured on the Dreamcast** by the original game (one cutscene, six characters, 84 columns, 5406 vertices, 33 ms frame): **14.70 ms** for the shade, with the every-column loop before it was sorted. That is 2.72 us, or 544 SH-4 cycles, per vertex.

**Measured on a PC**, over the 1,063,664 skinned vertices of that game's shipped character models in the bind pose, under two lights (a census program in the game's source tree): columns visited per vertex, every column 13.3; the sorted loop 5.2; the column lists 2.6. Four vertices in a hundred belong to a bone no column reaches. The list loop and the every-column loop agree within 0.000063 of strength, which moves a packed 8-bit colour by one step on about one vertex in 75,000.

**Measured again on the Dreamcast after the column lists and the one-FTRV-per-vertex loop**, the same scene, the same six characters, 84 columns and 5406 vertices: **4.5 ms**, down from 14.70 ms. That is 3.3 times faster: about 0.83 us, or roughly 166 SH-4 cycles at 200 MHz, per vertex (544 before).

| Scene                        | Columns | Vertices | Before   | After  |
| --- | --- | --- | --- | --- |
| six characters, 33 ms frame  | 84      | 5406     | 14.70 ms | 4.5 ms |

The 4.5 ms is one hardware run of that scene as the game's author timed it, not a benchmark suite. It includes everything the sorting, the column lists and the SH4ZAM routines changed together, not one trick alone.

**Packed lists, not yet measured on hardware.** `make bench` builds `build/bench.elf`: six figures, 84 columns, 5376 vertices, timing the every-column loop, the list loop and the packed loop with the SH-4 performance counter, and printing each fast loop's largest difference from the every-column loop on screen and to dcload. On the SH-4 code GCC emits, a listed column the vertex misses costs about 15 instructions in the packed loop against about 29 in the list loop (no index byte, shift and large frame offset per column), and in that scene 12 of the 84 bones skip their vertices (6 empty, 6 solid).

On WoWDC's cache and pipeline timing Flycast (Bruceleeto's `cache` branch: SH-4 latencies, dual issue and the direct-mapped caches modelled; `bench/pipeline.sh` runs it), the same scene:

| Loop | Time | Cycles a vertex | Largest difference from every column |
| --- | --- | --- | --- |
| every column, `sh4dow_strength` | 6294 us | 234 | (the reference) |
| list, `sh4dow_strength_list` | 4273 us | 159 | 0.000030 |
| packed, `sh4dow_strength_packed` in the bench's own loop | 2490 us | 92 | 0.000029 |
| `sh4dow_strength_packed_n` per bone | 2278 us | 84 | 0.000029 |

`sh4dow_strength_packed_n` is 1.88 times the list loop. Plain Flycast, which charges about one cycle an instruction, gives 1.55. That emulator is deterministic for one ELF but moves 10 to 15% with code placement (the same loop at another address measured 2618 us), so only the large differences here are findings. The bench's `ab` rows run variants inside one ELF.

What moved it: the packed entries (one pointer, no per-column index decode); the pack built in one pass from entries `sh4dow_prepare` makes once a frame (the pack alone went from 687 to 371 us there); the feather without its zero guard and clamps; the empty and solid bone skips. Tried and left out, for no measured gain: an inline-assembly side test, two entries a pass (`-DSH4DOW_ASM_SCAN` keeps it for a hardware A/B), a batched FTRV pass into a buffer, prefetching the positions, and `movca.l` on the output.

Why it is cheap: the CPU already holds every vertex to transform it (the SH-4 has no vertex shader). The shade is a few dot products on that vertex, and the colour write it was already going to do. Nothing extra is drawn or stored. The soft edge is a fade, so it costs the same as a hard one.

## Build and test

```sh
make test                        # host, plain C: test/shade_test, test/foot_test, example/example, example/quickstart, example/feet
make test-shz SH4ZAM=/path/to/sh4zam   # host, the SH4ZAM path on SH4ZAM's software back-end: the same two tests
make dc SH4ZAM=/path/to/sh4zam   # Dreamcast: build/libsh4dow_dc.a, both objects (KallistiOS environment sourced)
make bench SH4ZAM=/path/to/sh4zam  # Dreamcast: build/bench.elf, the three vertex loops timed on the console
make test-dc SH4ZAM=/path/to/sh4zam  # Dreamcast: build/shade_test.elf, build/foot_test.elf (the assembly path); run in Flycast or on the console
```

The Dreamcast build wants SH4ZAM's include path and the flags its fast paths expect: `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`. Link SH4ZAM's library as you already do, and do not copy SH4ZAM's files into your project: use it as a library, at 0.10 or later. The library needs no KallistiOS header.

`test/shade_test.c`: one column's core, feather, fade-out, lit side and own-bone skip; a grid and random 14-limb frames against the plain formula with no early-outs; ball lists and capsule lists against the every-column loop, with a shrunk ball and a shrunk capsule that must be caught; packed lists against the every-column loop, solid and empty packs; a figure under a straight-down and a leaning light: the shade falls away from the light, the chin stays lit, a mirrored figure mirrors.

`test/foot_test.c`: the shipped numbers; the cone on a flat floor under a 45 degree light, an overhead one, a horizon one and no light at all; the walls, ceilings and storeys it refuses; the fade as the heel rises; a sloped floor, a Y up world and another unit giving the same cone; the placement and its clamps; the distance fade with a scope; the picture (full at the heel, empty at the far end, mirror symmetric, one texel at a time against the byte and RGB565 textures); the foot finder on Z up and Y up skeletons, one foot, two feet on one side, hovering. Both tests run on the plain C path and on the SH4ZAM path.

## Credits and license

SH4DOW-DC is by Emerald May, 2026. GitHub: [JiltedValkyrie](https://github.com/JiltedValkyrie). The line the game shows: `SH4DOW-DC (c) 2026 Jilted Valkyrie`.

Dreamcast math: [SH4ZAM](https://github.com/gyrovorbis/sh4zam), Copyright (c) 2025, 2026 Falco Girgis, MIT. Keep that notice with the library.

A credits line you can copy:

> Soft self-shadow: SH4DOW-DC by Emerald May (JiltedValkyrie), from Halo: Dreamcast Evolved. Dreamcast math: SH4ZAM by Falco Girgis.

The license, the same text as [LICENSE](LICENSE):

SH4DOW-DC
Copyright (c) 2026 Emerald May

Originally developed for Halo: Dreamcast Evolved

Permission is granted, free of charge, to use, modify, and incorporate SH4DOW-DC into personal and commercial software, including games, engines, and applications.

Attribution to the original author is required in an accessible credits, acknowledgments, or third-party notices section. Redistributions of the source must retain the original copyright and license notices.

The original implementation, or substantially similar derivatives, may not be sold as standalone shadow-rendering source code, libraries, plugins, or software packages.

Commercial distribution of larger products incorporating this implementation is expressly permitted.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
