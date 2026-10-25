# SH4DOW-DC: how to call it

This folder is the whole library. It stands alone: no game header, no game data. Halo: Dreamcast Evolved is one client (it calls `sh4dow_prepare` and `sh4dow_strength` from its own vertex loop); tune the game in the game, and change the math here, once.

## Files

- `include/sh4dow_dc.h` is the API. `sh4dow_axes` and `sh4dow_strength` are inline in it.
- `src/sh4dow_dc.c` is `sh4dow_prepare` and `sh4dow_shade`: the soft limb self-shadow.
- `src/sh4dow_foot.c` is the foot cones: `sh4dow_foot_place`, `sh4dow_foot_cone`, `sh4dow_foot_range_fade`, the picture and `sh4dow_pick_feet`. It needs no frame and nothing from `sh4dow_dc.c` but the header; link it only when you draw cones.
- `test/shade_test.c`, `test/foot_test.c`, `example/example.c`, `example/quickstart.c` and `example/feet.c` run with `make test`; `make test-shz SH4ZAM=...` runs the two tests on the SH4ZAM path. `make dc SH4ZAM=...` builds the Dreamcast library from both sources.
- `bench/bench.c` times the every-column, list and packed vertex loops on the console: `make bench SH4ZAM=...` builds `build/bench.elf`. Built with `-DSH4DOW_DC_NO_SH4ZAM` it runs on a host to check its numbers, not its times.
- `README.md` is the explanation. The shipped numbers and what raising them does live there. Do not paste those tables into this file.
- `LICENSE` is the license.

There is no `.s` file. The one piece of SH-4 assembly is inline in the header and off by default: `sh4dow_scan_asm_`, a packed-walk side test, built with `-DSH4DOW_ASM_SCAN` for a hardware A/B (on the pipeline emulator it is within code-placement noise of the C walk). `make test-dc` runs the tests on the Dreamcast path in Flycast; `bench/pipeline.sh` times `build/bench.elf` on WoWDC's cache/pipeline Flycast. Judge a change by that emulator or the console, not by plain Flycast, and only by differences larger than its 10 to 15% placement noise. On the Dreamcast the square roots and the reciprocals are FSRRA inside [SH4ZAM](https://github.com/gyrovorbis/sh4zam) (0.10 or later), and its vector and matrix routines do the dots, the cross and the 3x4 transform. Compile the sources without `SH4DOW_DC_NO_SH4ZAM` and link SH4ZAM. Use SH4ZAM as a library; do not copy its files into this folder. Keep Falco Girgis's MIT notice with SH4ZAM.

`SH4DOW_DC_NO_SH4ZAM` compiles the same steps as plain C for a host tool or a game that does not ship SH4ZAM. That path does not need the SH4ZAM credit.

Units. The soft shadow takes your vertex units. The foot cones' distances are in the original game's world units (1 unit = 3.048 m): `sh4dow_foot_params_scale` carries them to yours. Radii and slopes are ratios.

## Call order

1. Pose the skeleton.
2. Light the vertices. This multiply only dims a color that already exists.
3. Fill one `sh4dow_limb_t` per limb you want to cast. Position and radius are in the same space as the vertices. Radius is already scaled.
4. `sh4dow_prepare` once per character per frame. `direction` is the way the shadow falls, away from the light, in that same space. It does not need to be unit length. Straight down is `(0, 0, -1)` when Z is up and `(0, -1, 0)` when Y is up.
5. `sh4dow_shade` on the vertices you are about to draw. Then pack the color for the GPU yourself. There is no second draw and no video memory.
   Or, in your own loop: `sh4dow_axes` once, then `sh4dow_strength` per vertex (0..1), and darken or tint the colour yourself.
   Fastest, per bone: `sh4dow_load_xmtrx` once per frame, `sh4dow_project` the bone's ball centre, `sh4dow_ball_pack` (or `sh4dow_capsule_pack` for blended vertices). If `count` is 0 every vertex is 0; if `solid` every vertex is 1; otherwise one FTRV per vertex and `sh4dow_strength_packed`. With the bone's positions in an array, `sh4dow_strength_packed_n` does all of that, overlaps each FTRV with the previous vertex's shade, and is the fastest path. `sh4dow_load_xmtrx` replaces XMTRX, so load your own matrix back afterwards.

`xyz_stride` and `rgb_stride` are byte steps. A packed `float[3]` uses 12. A bone inside a vertex struct uses `sizeof` that struct as `bone_stride`. Pass `bone0 == NULL` to turn the first-bone skip off.

## Limbs

Up to `SH4DOW_COLUMN_MAX` (32). The Dreamcast game spends 14, on at most six characters. Both caps are budgets, not part of the math.

Flag the head with `SH4DOW_LIMB_HEAD` and the neck with `SH4DOW_LIMB_NECK` when you want the chin rule. The head column then starts at the neck: `neck` centre plus `collar` neck radii along the light. If that start would fall on the lit side of the head, it starts one head radius along the light instead. No neck flag does that same head-radius start. A limb with neither flag starts at its joint.

`bone` is the vertex's first bone, not its second. A column skips vertices whose first bone equals `bone`. Other columns still shade those vertices, so the limb still goes dark. `SH4DOW_BONE_NONE` (`255`) skips nobody.

The Halo hero skeleton uses first-bone 12 for the head, 9 for the neck, 6 for the torso, 13 and 14 for the arms. Those numbers belong to that skeleton. Other characters in that game do not use the chin rule: leave the flags off and the columns start at the joints.

## Leave out

Do not add a shadow-map pass, a modifier volume, a floor blob, or a tint toward another palette. Those are other shadows. This multiply is grey: `rgb *= 1 - dark * strength`, and the darkest column wins.

If the shade points at the light, flip `direction`. If a column is the wrong size, the radius is not in vertex space.

## Foot cones, call order

1. Once per model: `sh4dow_pick_feet` over the bind pose node sphere centres and radii, with your up and left-right axes and the model's height range.
2. Per character per frame, when `sh4dow_foot_range_fade` is above 0: per foot, `sh4dow_foot_place` from the posed foot sphere (character scale applied to the radius), then set `foot.away` to the light's travel at the foot (straight down when there is no light).
3. `sh4dow_foot_cone` with the floor plane under the foot (a point and a normal from your collision), `up`, and the character's `forward`. A return of 0 means no quad. Otherwise draw the quad (TL, TR, BL, BR; u across, v along) with `sh4dow_foot_texture565` or `sh4dow_foot_texture`, blended `dst * (1 - src)`, at a grey strength of your darkness x the returned fade x the distance fade.

If a cone points at the light, `away` is the wrong way: it travels from the light toward the foot. If cones vanish, the normal and `up` disagree (a wall) or the heel is more than `fade_height` above the plane. If the cone is the wrong size, the radii are not in the units of `sh4dow_foot_params_scale`.
