# SH4DOW-DC

Soft self-shadow for skinned characters, computed per vertex on the CPU. Each limb casts a soft column away from the light; a vertex inside a column gets darker. No shadow map, no second mesh, no extra draw, no video memory.

Two files. C99. Made for the Sega Dreamcast (SH-4) with [SH4ZAM](https://github.com/gyrovorbis/sh4zam); builds in plain C anywhere else.

Originally developed for Halo: Dreamcast Evolved.

## Contents

- [What it looks like](#what-it-looks-like)
- [Quick start](#quick-start)
- [How it works](#how-it-works)
- [API](#api)
- [Tuning](#tuning)
- [Cost](#cost)
- [Build and test](#build-and-test)
- [Credits and license](#credits-and-license)

## What it looks like

<!-- PLACEHOLDER: two screenshots of the same shot, supplied after the next hardware build.
     1. before: the hard limb volumes            2. after: SH4DOW-DC
     Keep the same camera, the same light, the same character. -->

A dark band under the chin, the arm's shade across the chest, the chest's shade down the belly. The edge is a thin feather. The colour under the shade is the lit colour, darker: a red light stays red.

## Quick start

Add `include/` to your include path and `src/sh4dow_dc.c` to your build. On the Dreamcast, SH4ZAM must be on the include path too. Anywhere else, define `SH4DOW_DC_NO_SH4ZAM`.

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

## API

Everything is in `include/sh4dow_dc.h`. The per-vertex functions are `static inline`: no call per vertex.

| Function | When | What |
| --- | --- | --- |
| `sh4dow_params_default` | once | fills `sh4dow_params_t` with the shipped tuning (the Tuning table) |
| `sh4dow_prepare` | per character, per frame | builds the frame: the columns, sorted by start, and the fast-path tables. Returns the column count; 0 means nothing to shade |
| `sh4dow_shade` | per vertex batch | multiplies float rgb triples by the shade. Byte strides; `bone0` NULL turns the own-bone skip off |
| `sh4dow_axes`, `sh4dow_strength` | per vertex | the strength 0..1 of one vertex against every column, for your own colour math |
| `sh4dow_project` | per ball, per vertex | a point onto the frame's axes, origin-relative. On the Dreamcast, `frame->rows` in XMTRX does the same with one FTRV |
| `sh4dow_ball_columns` | per bone, per frame | the columns a ball can touch, at most `frame->count` bytes |
| `sh4dow_capsule_columns` | per bone pair, per frame | the same for the hull of two balls (a blended vertex) |
| `sh4dow_strength_list` | per vertex | the strength from the projection, against a list |
| `sh4dow_strength_rel` | per vertex | the strength from the projection, against every column |

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
```

Types: `sh4dow_limb_t` (position, radius, bone, flags), `sh4dow_params_t` (the Tuning table), `sh4dow_frame_t` (about 3 KB: up to `SH4DOW_COLUMN_MAX` = 32 columns and their tables; one per character drawn, or one reused). `own` is a vertex's first bone, or `SH4DOW_BONE_NONE` for no skip. The lists: `SH4DOW_LIST_INDEX` masks the column, `SH4DOW_LIST_FULL` marks an entry the fades do not touch.

Rules of use:

- Call `sh4dow_prepare` after posing and before the vertex pass. `direction` may be any length; a zero or non-finite direction, or bad params, store no columns and return 0.
- The list functions are exact for points inside the ball and say nothing about points outside it. Give the ball a radius that really holds the vertices.
- `sh4dow_strength_list` expands the distance test algebraically, so a column far thinner than its distance from the frame's origin (the mean joint) loses precision: about one colour step at a ratio of 1:100, a wrong answer near 1:3000. Keep such limbs out of the list loop, or use `sh4dow_strength`.
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

## Cost

**Measured on the Dreamcast** by the original game (one cutscene, six characters, 84 columns, 5406 vertices, 33 ms frame): **14.70 ms** for the shade, with the every-column loop before it was sorted. That is 2.72 us, or 544 SH-4 cycles, per vertex.

**Measured on a PC**, over the 1,063,664 skinned vertices of that game's shipped character models in the bind pose, under two lights (a census program in the game's source tree): columns visited per vertex, every column 13.3; the sorted loop 5.2; the column lists 2.6. Four vertices in a hundred belong to a bone no column reaches. The list loop and the every-column loop agree within 0.000063 of strength, which moves a packed 8-bit colour by one step on about one vertex in 75,000.

**Estimate**, from an SH-4 instruction model fitted to the one hardware number and not yet measured: the sorted loop at about half of the 14.70 ms, the column lists at about a fifth. Memory stalls on the vertex stream are not in the model, so expect the hardware figure above the model's; a reviewer's reading is 3.5 to 4.5 ms for that scene.

<!-- PLACEHOLDER: after the next hardware build, the same scene measured again. Fill in and delete the estimate above.
| Scene                        | Columns | Vertices | Before   | After |
| --- | --- | --- | --- | --- |
| six characters, 33 ms frame  | 84      | 5406     | 14.70 ms |       | -->

Why it is cheap: the CPU already holds every vertex to transform it (the SH-4 has no vertex shader). The shade is a few dot products on that vertex, and the colour write it was already going to do. Nothing extra is drawn or stored. The soft edge is a fade, so it costs the same as a hard one.

## Build and test

```sh
make test                        # host, plain C: test/shade_test, example/example, example/quickstart
make dc SH4ZAM=/path/to/sh4zam   # Dreamcast: build/libsh4dow_dc.a (KallistiOS environment sourced)
```

The Dreamcast build wants SH4ZAM's include path and the flags its fast paths expect: `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno`. Link SH4ZAM's library as you already do. The library needs no KallistiOS header.

`test/shade_test.c`: one column's core, feather, fade-out, lit side and own-bone skip; a grid and random 14-limb frames against the plain formula with no early-outs; ball lists and capsule lists against the every-column loop, with a shrunk ball and a shrunk capsule that must be caught; a figure under a straight-down and a leaning light: the shade falls away from the light, the chin stays lit, a mirrored figure mirrors.

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
