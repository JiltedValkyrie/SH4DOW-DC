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
