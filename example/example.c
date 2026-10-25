/* SH4DOW-DC

   example/example.c
   Copyright (C) 2026 Emerald May

   A head, a neck and a chest under a light straight down (Z up). Builds on a
   host with -DSH4DOW_DC_NO_SH4ZAM (make test), or on the Dreamcast with SH4ZAM.
*/

#include <stdio.h>
#include "sh4dow_dc.h"

typedef struct {
    float xyz[3];
    float rgb[3];
    uint8_t bone;               /* first bone */
} vertex_t;

enum { HEAD, NECK, CHEST, HAND = 9 };

int main(void) {
    /* Posed joints and sphere radii, in the vertices' space. */
    const sh4dow_limb_t limbs[3] = {
        { { 0.0f, 0.0f, 1.70f }, 0.10f, HEAD,  SH4DOW_LIMB_HEAD },
        { { 0.0f, 0.0f, 1.55f }, 0.06f, NECK,  SH4DOW_LIMB_NECK },
        { { 0.0f, 0.0f, 1.30f }, 0.18f, CHEST, 0 },
    };
    /* Lit vertices: the chin, the chest under the neck, the belly, a hand out to the side. */
    vertex_t v[4] = {
        { { 0.05f, 0.0f,  1.62f }, { 1.0f, 1.0f, 1.0f }, HEAD },
        { { 0.03f, 0.0f,  1.40f }, { 1.0f, 1.0f, 1.0f }, CHEST },
        { { 0.05f, 0.0f,  1.00f }, { 1.0f, 1.0f, 1.0f }, CHEST },
        { { 0.0f,  0.60f, 1.00f }, { 1.0f, 1.0f, 1.0f }, HAND },
    };
    const char *name[4] = { "chin", "upper chest", "belly", "hand" };
    const float down[3] = { 0.0f, 0.0f, -1.0f };
    sh4dow_frame_t frame;
    sh4dow_axes_t axes;
    float s;
    int i;

    if(sh4dow_prepare(&frame, NULL, limbs, 3, down) <= 0)
        return 1;

    /* Float colours, byte strides. */
    sh4dow_shade(&frame, v[0].xyz, sizeof(v[0]), &v[0].bone, sizeof(v[0]), v[0].rgb, sizeof(v[0]), 4);
    for(i = 0; i < 4; i++)
        printf("%-12s rgb %.3f %.3f %.3f\n", name[i], v[i].rgb[0], v[i].rgb[1], v[i].rgb[2]);

    /* Or your own loop, with your own tint and packing: the strength alone. */
    axes = sh4dow_axes(&frame);
    s = sh4dow_strength(&frame, &axes, 0.05f, 0.0f, 1.00f, CHEST);
    printf("belly strength %.3f -> rgb x %.3f\n", s, 1.0f - frame.dark * s);
    return 0;
}
