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
