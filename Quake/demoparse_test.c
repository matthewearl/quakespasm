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


#include <stdio.h>

#include "quakedef.h"


typedef struct {
    qboolean print_callbacks;
    float time;
    float finish_time;
} dp_test_ctx_t;


static qboolean
server_info (const char *level_name, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("server_info: level_name=%s\n", level_name);
    }
    return true;
}


static qboolean
time (float time, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("time: %f\n", time);
    }
    tctx->time = time;
    return true;
}


static qboolean
baseline (int entity_num, vec3_t origin, vec3_t angle, int frame,
          void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("baseline: ent=%d origin=(%f %f %f), angle=(%f %f %f), frame=%d\n",
               entity_num,
               origin[0], origin[1], origin[2],
               angle[0], angle[1], angle[2],
               frame);
    }
    return true;
}


static qboolean
update(int entity_num, vec3_t origin, vec3_t angle,
       byte origin_bits, byte angle_bits, int frame, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("update: ent=%d origin=(%f %f %f), angle=(%f %f %f), "
               "origin_bits=%d angle_bits=%d frame=%d\n",
               entity_num,
               origin[0], origin[1], origin[2],
               angle[0], angle[1], angle[2],
               origin_bits, angle_bits,
               frame);
    }
    return true;
}


static qboolean
packet_end (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("packet_end\n");
    }
    return true;
}


static qboolean
set_view (int entity_num, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("set_view: ent=%d\n", entity_num);
    }
    return true;
}


static qboolean
intermission (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("intermission\n");
    }

    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return true;
}


static qboolean
finale (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("finale\n");
    }
    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return true;
}


static qboolean
cut_scene (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("cut_scene\n");
    }
    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return true;
}


static qboolean
disconnect (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("disconnect\n");
    }
    return true;
}


static void
read_file (const char *fname, byte **data, unsigned int *data_len)
{
    FILE *f;

    f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    *data_len = ftell(f);
    *data = malloc(*data_len);

    fseek(f, 0, SEEK_SET);
    fread(*data, *data_len, 1, f);
    fclose(f);
}


int
main (int argc, char **argv)
{
    dp_err_t dprc;
    byte *data;
    unsigned int data_len;
    qboolean print_finish_time = false;
    dp_test_ctx_t tctx = {
        .print_callbacks = false,
        .time = -1,
        .finish_time = -1,
    };
    dp_callbacks_t callbacks = {
        .server_info = server_info,
        .time = time,
        .baseline = baseline,
        .update = update,
        .packet_end = packet_end,
        .set_view = set_view,
        .intermission = intermission,
        .finale = finale,
        .cut_scene = cut_scene,
        .disconnect = disconnect,
    };

    if (argc < 3) {
        fprintf(stderr, "Usage: %s demo-file [callbacks|finishtime]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "callbacks") == 0) {
        tctx.print_callbacks = true;
    } else if (strcmp(argv[2], "finishtime") == 0) {
        print_finish_time = true;
    } else {
        fprintf(stderr, "Invalid command: %s\n", argv[2]);
    }

    read_file(argv[1], &data, &data_len);
    dprc = DP_ReadDemo(data, data_len, &callbacks, &tctx);
    free(data);

    if (print_finish_time) {
        int mins = ((int)tctx.finish_time) / 60;
        printf("Finish time: %d:%08.5f\n",
               mins, tctx.finish_time - mins * 60.f);
    }

    return dprc;
}
