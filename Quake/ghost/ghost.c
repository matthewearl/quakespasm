/*
Copyright (C) 2022 Matthew Earl

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "../quakedef.h"
#include "ghost_private.h"


static cvar_t ghost_delta = {"ghost_delta", "1", CVAR_ARCHIVE};
static cvar_t ghost_range = {"ghost_range", "128", CVAR_ARCHIVE};
static cvar_t ghost_alpha = {"ghost_alpha", "0.5", CVAR_ARCHIVE};


static char         ghost_demo_path[MAX_OSPATH] = "";
static ghostrec_t  *ghost_records = NULL;
static int          ghost_num_records = 0;
static entity_t    *ghost_entity = NULL;
static float        ghost_shift = 0.0f;
static float        ghost_finish_time = -1.0f;


// This could be done more intelligently, no doubt.
static float Ghost_FindClosest (vec3_t origin, qboolean *match)
{
    int idx;
    ghostrec_t *rec;
    vec3_t diff;
    ghostrec_t *closest_rec = NULL;
    float closest_dist_sqr;
    float dist_sqr;

    // Ignore any matches that are not close by.
    closest_dist_sqr = 256.0f * 256.0f;

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records;
         rec++, idx++) {
        VectorSubtract(origin, rec->origin, diff);

        dist_sqr = DotProduct(diff, diff);
        if (dist_sqr < closest_dist_sqr) {
            closest_dist_sqr = dist_sqr;
            closest_rec = rec;
        }
    }

    if (closest_rec != NULL) {
        *match = true;
        return cl.time - closest_rec->time;
    } else {
        *match = false;
        return 0.0f;
    }
}


// Find the index of the first record that is > time.
//
// If speed becomes a concern we could do this iteratively or with a binary
// search.
static int Ghost_FindRecord (float time)
{
    int idx;
    ghostrec_t *rec;

    if (ghost_records == NULL) {
        // not loaded
        return -1;
    }

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records && time >= rec->time;
         idx++, rec++);

    if (idx == 0) {
        // not yet at the first record
        return -1;
    }

    if (idx == ghost_num_records) {
        // gone beyond the last record
        return -1;
    }

    return idx;
}


void Ghost_Load (const char *map_name)
{
    int i;
    ghost_info_t ghost_info;

    memset(&ghost_info, 0, sizeof(ghost_info));
    ghost_records = NULL;
    ghost_num_records = 0;
    ghost_entity = NULL;
    ghost_finish_time = -1.0f;

    if (ghost_demo_path[0] == '\0') {
        return;
    }

    if (!Ghost_ReadDemo(ghost_demo_path, &ghost_info, map_name)) {
        return;
    }
    ghost_records = ghost_info.records;
    ghost_num_records = ghost_info.num_records;

    // Print player names
    Con_Printf("Ghost player(s): ");
    for (i = 0; i < GHOST_MAX_CLIENTS; i++) {
        if (ghost_info.client_names[i][0] != '\0') {
            Con_Printf(" %s ", ghost_info.client_names[i]);
        }
    }
    Con_Printf("\n");

    // Print finish time
    ghost_finish_time = ghost_info.finish_time;
    if (ghost_info.finish_time > 0) {
        Con_Printf("Ghost time:       %.4f s\n", ghost_info.finish_time);
    }

    ghost_entity = (entity_t *)Hunk_AllocName(sizeof(entity_t),
                                              "ghost_entity");

    ghost_entity->model = Mod_ForName ("progs/player.mdl", false);
    ghost_entity->colormap = vid.colormap;  // TODO: Cvar for colors.
    ghost_entity->lerpflags |= LERP_RESETMOVE|LERP_RESETANIM;
    ghost_entity->skinnum = 0;

    ghost_shift = 0.0f;
}


static void Ghost_LerpOrigin(vec3_t origin1, vec3_t origin2, float frac,
                             vec3_t origin)
{
    int i;
    float d;

    for (i=0; i<3; i++) {
        d = origin2[i] - origin1[i];
        origin[i] = origin1[i] + frac * d;
    }
}


static void Ghost_LerpAngle(vec3_t angles1, vec3_t angles2, float frac,
                            vec3_t angles)
{
    int i;
    float d;

    for (i=0; i<3; i++) {
        d = angles2[i] - angles1[i];
        if (d > 180)
            d -= 360;
        else if (d < -180)
            d += 360;
        angles[i] = angles1[i] + frac * d;
    }
}


static void Ghost_SetAlpha(void)
{
    entity_t *ent = &cl_entities[cl.viewentity];
    float alpha;
    vec3_t diff;
    float dist;

    // distance from player to ghost
    VectorSubtract(ent->origin, ghost_entity->origin, diff);
    dist = VectorLength(diff);

    // fully opaque at range+32, fully transparent at range
    alpha = CLAMP(0.0f, (dist - ghost_range.value) / 32.0f, 1.0f);

    // scale by cvar alpha
    alpha *= CLAMP(0.0f, ghost_alpha.value, 1.0f);

    ghost_entity->alpha = (byte)(alpha * 254.0f + 1.0f);
}


static qboolean Ghost_Update (void)
{
    float lookup_time = cl.time + ghost_shift;
    int after_idx = Ghost_FindRecord(lookup_time);
    ghostrec_t *rec_before;
    ghostrec_t *rec_after;
    float frac;
    qboolean ghost_show;

    if (after_idx == -1) {
        ghost_show = false;
    } else {
        ghost_show = true;

        rec_after = &ghost_records[after_idx];
        rec_before = &ghost_records[after_idx - 1];

        frac = (lookup_time - rec_before->time)
                / (rec_after->time - rec_before->time);

        // TODO: lerp animation frames
        ghost_entity->frame = rec_after->frame;

        Ghost_LerpOrigin(rec_before->origin, rec_after->origin,
                         frac,
                         ghost_entity->origin);
        Ghost_LerpAngle(rec_before->angle, rec_after->angle,
                        frac,
                        ghost_entity->angles);

        // Set alpha based on distance to player.
        Ghost_SetAlpha();
    }

    return ghost_show;
}


void Ghost_Draw (void)
{
    /*
     * These attributes are required by R_DrawAliasModel:
     *  - frame
     *  - model
     *  - origin
     *  - angles
     *  - lerpflags
     *  - alpha
     *  - skinnum
     *  - colormap
     */
    if (Ghost_Update()) {
        currententity = ghost_entity;
        R_DrawAliasModel (ghost_entity);
    }
}


// Modified from the JoeQuake speedometer
extern cvar_t scr_speed_offset;
static void Ghost_DrawGhostTime (int y)
{
    int   x, size, bg_color;
    float scale;
    char  st[8];
    float relative_time;
    float alpha;
    float width;
    qboolean match;

    entity_t *ent = &cl_entities[cl.viewentity];

    if (!ghost_delta.value || ghost_records == NULL)
        return;
    relative_time = Ghost_FindClosest(ent->origin, &match);
    if (!match)
        return;

    GL_SetCanvas (CANVAS_CROSSHAIR);

    scale = 1;
    size = 8;
    x = -80;
    alpha = 0.5f;

    if (relative_time < 1e-3) {
        sprintf (st, "%.2f", relative_time);
    } else {
        sprintf (st, "+%.2f", relative_time);
    }

    bg_color = relative_time > 0 ? 251 : 10;
    Draw_Fill (x, y - (int)(1 * scale), 160, 1, bg_color, alpha);
    Draw_Fill (x, y + (int)(9 * scale), 160, 1, bg_color, alpha);
    Draw_Fill (x + (int)(32 * scale), y - (int)(2 * scale), 1, 13, bg_color, alpha);
    Draw_Fill (x + (int)(64 * scale), y - (int)(2 * scale), 1, 13, bg_color, alpha);
    Draw_Fill (x + (int)(96 * scale), y - (int)(2 * scale), 1, 13, bg_color, alpha);
    Draw_Fill (x + (int)(128 * scale), y - (int)(2 * scale), 1, 13, bg_color, alpha);
    Draw_Fill (x, y, 160, 9, 52, 0.9);

    width = fabs(relative_time) * 160;
    if (width > 160) {
        width = 160;
    }

    if (relative_time < 0) {
        Draw_Fill (x + 160 - width, y, width, 9, 100, alpha);
    } else {
        Draw_Fill (x, y, width, 9, 100, alpha);
    }

    Draw_String (x + (int)(5.5 * size) - (strlen(st) * size), y, st);
}

void Ghost_DrawSpeed(int y)
{
    float lookup_time;
    int after_idx;
    vec3_t hvel;
    float speed;

    lookup_time = cl.time + ghost_shift;
    after_idx = Ghost_FindRecord(lookup_time);
    if (after_idx != -1) {
        VectorCopy(ghost_records[after_idx - 1].velocity, hvel);
        hvel[2] = 0;
        speed = VectorLength(hvel);

        SCR_DrawSpeedGeneric(y, "ghost", speed, 0, false, 0, 0);
    }
}

void Ghost_DrawHud (void)
{
    if (ghost_records == NULL) {
        return;
    }

    Ghost_DrawSpeed(scr_speed_offset.value + 15);
    Ghost_DrawGhostTime(scr_speed_offset.value + 30);
}


void Ghost_Finish (void)
{
    float delta;
    if (ghost_finish_time > 0) {
        delta = cl.mtime[0] - ghost_finish_time;
        Con_Printf("Finished %.3f s %s ghost\n",
                   fabs(delta), delta > 0 ? "behind" : "ahead of");

    }
}


static void Ghost_Command_f (void)
{
    char demo_path[MAX_OSPATH];

    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 2)
    {
        if (ghost_demo_path[0] == '\0') {
            Con_Printf("no ghost has been added\n");
        } else {
            Con_Printf("ghost %s has been added\n", ghost_demo_path);
        }
        Con_Printf("ghost <demoname> : add a ghost\n");
        return;
    }

    q_strlcpy(demo_path, Cmd_Argv(1), sizeof(demo_path));
    COM_AddExtension(demo_path, ".dem", sizeof(demo_path));

    if (!COM_FileExists(demo_path, NULL)) {
        Con_Printf("cannot find demo %s", demo_path);
        return;
    }
    q_strlcpy(ghost_demo_path, demo_path, sizeof(ghost_demo_path));

    Con_Printf("ghost will be loaded on next map load\n");
}


static void Ghost_RemoveCommand_f (void)
{
    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 1)
    {
        Con_Printf("ghost_remove : remove ghost\n");
        return;
    }

    if (ghost_demo_path[0] == '\0') {
        Con_Printf("no ghost has been added\n");
    } else {
        Con_Printf("ghost %s will be removed on next map load\n", ghost_demo_path);
        ghost_demo_path[0] = '\0';
    }
}


static void Ghost_ShiftCommand_f (void)
{
    float delta;
    qboolean match;
    entity_t *ent = &cl_entities[cl.viewentity];

    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 2)
    {
        Con_Printf("ghost_shift <t> : place ghost <t> seconds ahead of "
                   "player\n");
        return;
    }

    if (ghost_records == NULL) {
        Con_Printf("ghost not loaded\n");
        return;
    }


    delta = Ghost_FindClosest(ent->origin, &match);
    if (match) {
        ghost_shift = Q_atof(Cmd_Argv(1)) - delta;
    } else {
        Con_Printf("Cannot shift: Player cannot be matched to ghost's path\n");
    }
}


static void Ghost_ShiftResetCommand_f (void)
{
    if (cmd_source != src_command) {
        return;
    }

    if (Cmd_Argc() != 1)
    {
        Con_Printf("ghost_reset_shift : undo ghost shift\n");
        return;
    }

    if (ghost_records == NULL) {
        Con_Printf("ghost not loaded\n");
        return;
    }

    ghost_shift = 0.0f;
}



void Ghost_Init (void)
{
    Cmd_AddCommand ("ghost", Ghost_Command_f);
    Cmd_AddCommand ("ghost_remove", Ghost_RemoveCommand_f);
    Cmd_AddCommand ("ghost_shift", Ghost_ShiftCommand_f);
    Cmd_AddCommand ("ghost_shift_reset", Ghost_ShiftResetCommand_f);

    Cvar_RegisterVariable (&ghost_delta);
    Cvar_RegisterVariable (&ghost_range);
    Cvar_RegisterVariable (&ghost_alpha);
}

