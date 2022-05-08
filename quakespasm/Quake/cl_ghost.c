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


// TODO:  change to a binary search, or incremental search?
static ghostrec_t *Ghost_FindRecord (float time)
{
    int idx = 0;
    ghostrec_t *rec = ghost_records;

    if (ghost_records == NULL) {
        // not loaded
        return NULL;
    }

    for (idx = 0, rec = ghost_records;
         idx < ghost_num_records && time > rec->time;
         idx++, rec++);

    if (idx == ghost_num_records) {
        // gone beyond the last record
        return NULL;
    }

    return rec;
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


void Ghost_Update (void)
{
    ghostrec_t *rec = Ghost_FindRecord(cl.mtime[0]);

    if (rec == NULL) {
        ghost_show = false;
    } else {
        ghost_show = true;

        ghost_entity->frame = rec->frame;
        VectorCopy(rec->origin, ghost_entity->origin);
        VectorCopy(rec->angle, ghost_entity->angles);
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
        R_DrawAliasModel (ghost_entity);
    }
}
