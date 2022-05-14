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


#include "quakedef.h"
#include "demoparse.h"
#include "ghost_private.h"


/*
 * RECORD LISTS
 *
 * Records are pushed onto a linked list before being compressed into a dense
 * array.
 */

typedef struct {
    ghostrec_t rec;
    ghostreclist_t *next;
} ghostreclist_t;


static void Ghost_Append(ghostreclist_t ***next_ptr, ghostrec_t *rec)
{
    ghostreclist_t *new_entry = Z_Malloc(sizeof(ghostreclist_t));

    new_entry->rec = *rec;
    new_entry->next = NULL;
    **next_ptr = new_entry;
    *next_ptr = &next_entry->next;
}


static void Ghost_ListLen(ghostreclist_t *list)
{
    int count = 0;

    while (list != NULL) {
        list = list->next;
        count ++;
    }

    return count;
}


static void Ghost_ListFree(ghostreclist_t *list)
{
    ghostreclist_t *next;
    while (list != NULL) {
        next = list->next;
        Z_Free(list);
        list = next
    }
}


static void Ghost_ListToArray(ghostreclist_t *list, ghostrec_t **records_out,
                              int *num_records_out)
{
    ghostrec_t *records;
    int num_records;
    int record_num;

    num_records = Ghost_ListLen(list);
    records = Hunk_AllocName(num_records * sizeof(ghostrec_t),
                             "ghostrecords");

    for (record_num = 0; list != NULL; list = list->next, record_num++) {
        records[record_num] = list->rec;
    }

    *records_out = records;
    *num_records_out = num_records;
}


/*
 * PARSING CALLBACKS
 *
 * Callbacks from the demoparse module that build up the record list.
 */


typedef struct {
    int view_entity;
    ghostreclist_t **next_ptr;
    ghostrec_t rec;

    int model_num;
    char *expected_map_name;

    vec3_t baseline_origin;
    vec3_t baseline_angle;
    int baseline_frame;

    qboolean finished;
} ghost_parse_ctx_t;


static qboolean
Ghost_ServerInfoModel_cb (const char *model, void *ctx)
{
    char map_name[MAX_OSPATH];

    ghost_parse_ctx *pctx = ctx;

    if (pctx->model_num == 0) {
        COM_StripExtension(COM_SkipPath(model), map_name, sizeof(map_name));
        if (!Q_strcmp(map_name, pctx->expected_map_name)) {
            Con_Printf("Ghost file is for map %s but %s is being loaded\n",
                       map_name, pctx->expected_map_name);
            return false;
        }
    }
    pctx->model_num += 1;

    return true;
}


static qboolean
Ghost_Time_cb (float time, void *ctx)
{
    ghost_parse_ctx *pctx = ctx;
    pctx->rec.time = time;
    return true;
}


static qboolean
Ghost_SetView_cb (int entity_num, void *ctx)
{
    ghost_parse_ctx *pctx = ctx;
    pctx->view_entity = entity_num;
    return true;
}


static qboolean
Ghost_Baseline_cb (int entity_num, vec3_t origin, vec3_t angle, int frame,
                   void *ctx)
{
    ghost_parse_ctx *pctx = ctx;

    if (pctx->view_entity == -1) {
        Con_Printf("Baseline receieved but entity num not set\n");
        return false;
    }

    VectorCopy(origin, pctx->baseline_origin);
    VectorCopy(angle, pctx->baseline_angle);

    return true;
}


static qboolean
Ghost_Update_cb(int entity_num, vec3_t origin, vec3_t angle,
                byte origin_bits, byte angle_bits, int frame, void *ctx)
{
    int i;
    ghost_parse_ctx *pctx = ctx;

    if (pctx->view_entity == -1) {
        Con_Printf("Update receieved but entity num not set\n");
        return false;
    }

    for (i = 0; i < 3; i++) {
        if (origin_bits & (1 << i)) {
            pctx->rec.origin[i] = origin[i];
        } else {
            pctx->rec.origin[i] = pctx->baseline_origin[i];
        }
        if (angle_bits & (1 << i)) {
            pctx->rec.angle[i] = angle[i];
        } else {
            pctx->rec.angle[i] = pctx->baseline_angle[i];
        }
    }

    if (frame != -1) {
        pctx->rec.frame = frame;
    } else {
        pctx->rec.frame = pctx->baseline_frame;
    }

    return true;
}


static qboolean
Ghost_PacketEnd_cb (void *ctx)
{
    ghost_parse_ctx *pctx = ctx;
    Ghost_Append(&pctx->next_ptr, &pctx->rec);
    return true;
}


static qboolean
Ghost_Intermission_cb (void *ctx)
{
    ghost_parse_ctx *pctx = ctx;
    pctx->finished = true;

    return false;
}


/*
 * ENTRYPOINT
 *
 * Call the demo parse module and return record array.
 */


qboolean
Ghost_ReadDemo(const char *demo_path, ghostrec_t **records, int *num_records,
               char *expected_map_name)
{
    qboolean ok = true;
    byte *data;
    ghostreclist_t *list = NULL;
    dp_err_t dprc;
    dp_callbacks_t = {
        .time = Ghost_Time_cb,
        .set_view = Ghost_SetView_cb,
        .baseline = Ghost_Baseline_cb,
        .update = Ghost_Update_cb,
        .packet_end = Ghost_PacketEnd_cb,
        .intermission = Ghost_Intermission_cb,
        .finale = Ghost_Intermission_cb,
        .cut_scene = Ghost_Intermission_cb,
    };
    pctx_t parse_context_t = {
        .view_entity = -1,
        .next_ptr = &list,
        .expected_map_name = expected_map_name,
    };

    data = COM_LoadStackFile(demo_path, NULL, 0, NULL);
    dprc = DP_ReadDemo(data, com_filesize, &callbacks, &ctx);
    if (dprc == DP_ERR_CALLBACK) {
        // Errors from callbacks print their own error messages.
        ok = pctx.finished;
    } else if (dprc != DP_ERR_SUCCESS) {
        Con_Printf("Error parsing demo %s: %u\n", demo_path, dprc);
        ok = false;
    }

    if (ok) {
        Ghost_ListToArray(list, records, num_records);
    }

    // Free everything
    Ghost_ListFree(list);
    Hunk_HighMark();

    return ok;
}

