#include "quakedef.h"


typedef struct {
    float time;
    float origin[3];
    float angle[3];
    unsigned int frame;
} ghostrec_t;


static ghostrec_t  *ghost_records = NULL;
static int          ghost_num_records = 0;
static entity_t    *ghost_entity = NULL;
static qboolean     ghost_show = false;


float Ghost_FindClosest (vec3_t origin)
{
    int idx;
    ghostrec_t *rec;
    vec3_t diff;
    ghostrec_t *closest_rec = NULL;
    float closest_dist_sqr;
    float dist_sqr;

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records;
         rec++, idx++) {
        VectorSubtract(origin, rec->origin, diff);

        dist_sqr = DotProduct(diff, diff);
        if (closest_rec == NULL || dist_sqr < closest_dist_sqr) {
            closest_dist_sqr = dist_sqr;
            closest_rec = rec;
        }
    }

    return cl.time - closest_rec->time;
}

// Find the index of the first record that is >= time.
static int Ghost_FindRecord (float time)
{
    int idx = 0;
    ghostrec_t *rec = ghost_records;

    if (ghost_records == NULL) {
        // not loaded
        return -1;
    }

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records && time > rec->time;
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
    char    name[MAX_OSPATH];

    q_snprintf (name, sizeof(name), "ghosts/%s.ghost", map_name);
    ghost_records = (ghostrec_t *) COM_LoadHunkFile (name, NULL);

    if (ghost_records != NULL) {
        ghost_num_records = com_filesize / sizeof(ghostrec_t);
        ghost_entity = (entity_t *)Hunk_AllocName(sizeof(entity_t),
                                                  "ghost_entity");

        ghost_entity->model = Mod_ForName ("progs/player.mdl", false);
        ghost_entity->colormap = vid.colormap;
        ghost_entity->lerpflags |= LERP_RESETMOVE|LERP_RESETANIM;
        ghost_entity->alpha = 128;
        ghost_entity->skinnum = 0;
    }
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


void Ghost_Update (void)
{
    int after_idx = Ghost_FindRecord(cl.time);
    ghostrec_t *rec_before;
    ghostrec_t *rec_after;
    float frac;

    if (after_idx == -1) {
        ghost_show = false;
    } else {
        ghost_show = true;

        rec_after = &ghost_records[after_idx];
        rec_before = &ghost_records[after_idx - 1];

        frac = (cl.time - rec_before->time)
                / (rec_after->time - rec_before->time);

        // TODO: lerp animation frames
        ghost_entity->frame = rec_after->frame;

        Ghost_LerpOrigin(rec_before->origin, rec_after->origin,
                         frac,
                         ghost_entity->origin);
        Ghost_LerpAngle(rec_before->angle, rec_after->angle,
                        frac,
                        ghost_entity->angles);
    }
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
    if (ghost_show) {
        currententity = ghost_entity;
        R_DrawAliasModel (ghost_entity);
    }
}


// Modified from the JoeQuake speedometer
void Ghost_DrawGhostTime (void)
{
    int   x, y, size, bg_color;
    float scale;
    char  st[8];
    float relative_time;
    float alpha;
    float width;

    entity_t *ent = &cl_entities[cl.viewentity];

    if (!ghost_show)
        return;
    relative_time = Ghost_FindClosest(ent->origin);

    GL_SetCanvas (CANVAS_CROSSHAIR);

    scale = 1;
    size = 8;
    x = -80;
    y = 20;
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
