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
