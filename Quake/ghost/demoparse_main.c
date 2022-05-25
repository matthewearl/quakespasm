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

#include "../quakedef.h"
#include "demoparse.h"


typedef struct {
    FILE *demo_file;
    qboolean print_callbacks;
    float time;
    float finish_time;
    int stats[MAX_CL_STATS];
    char map_name[64];
    int protocol;
    unsigned int protocol_flags;

    qboolean collect_stats;
    unsigned long msg_sizes[256];
    unsigned long msg_counts[256];
    unsigned long packet_count;
} dp_test_ctx_t;


static qboolean
read (void *dest, unsigned int size, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    return fread (dest, size, 1, tctx->demo_file) != 0;
}


static dp_cb_response_t
server_info (int protocol, unsigned int protocol_flags, const char *level_name,
             void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("server_info: protocol=%d protocol_flags=%u level_name=%s\n",
               protocol, protocol_flags, level_name);
    }
    tctx->protocol = protocol;
    tctx->protocol_flags = protocol_flags;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
server_info_model (const char *model, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("server_info_model: model=%s\n", model);
    }

    if (tctx->map_name[0] == '\0') {
        int len;

        // First model is the map path
        snprintf(tctx->map_name, sizeof(tctx->map_name), "%s", model);

        // remove the "maps/" from the start.
        if (strncmp(tctx->map_name, "maps/", 5) == 0) {
            len = strlen(tctx->map_name);
            memmove(tctx->map_name, tctx->map_name + 5, len - 5 + 1);
        }

        // remove the ".bsp" from the end.
        len = strlen(tctx->map_name);
        if (len >= 4 && strcmp(tctx->map_name + len - 4, ".bsp") == 0) {
            tctx->map_name[len - 4] = '\0';
        }
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
server_info_sound (const char *sound, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("server_info_sound: sound=%s\n", sound);
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
time (float time, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("time: %f\n", time);
    }
    tctx->time = time;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
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
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
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
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
packet_end (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("packet_end\n");
    }

    if (tctx->collect_stats) {
        tctx->packet_count ++;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
set_view (int entity_num, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("set_view: ent=%d\n", entity_num);
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
intermission (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("intermission\n");
    }

    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
finale (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("finale\n");
    }
    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
cut_scene (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("cut_scene\n");
    }
    if (tctx->finish_time == -1) {
        tctx->finish_time = tctx->time;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
disconnect (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("disconnect\n");
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
update_stat (byte stat, int count, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("update_stat stat=%d count=%d\n", stat, count);
    }

    if (stat < MAX_CL_STATS) {
        tctx->stats[stat] = count;
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
found_secret (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("found_secret\n");
    }
    tctx->stats[STAT_SECRETS]++;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
killed_monster (void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("killed_monster\n");
    }
    tctx->stats[STAT_MONSTERS]++;
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
update_name (int client_num, const char *name, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("update_name client_num=%d name=%s\n", client_num, name);
    }
    return DP_CBR_CONTINUE;
}


static dp_cb_response_t
message (unsigned long offset, const byte *msg,
         unsigned int len, void *ctx)
{
    dp_test_ctx_t *tctx = ctx;
    if (tctx->print_callbacks) {
        printf("message type=0x%x offset=0x%lx len=%d\n", msg[0], offset, len);
    }

    if (tctx->collect_stats) {
        byte msg_type = msg[0];
        if (msg_type & U_SIGNAL) {
            msg_type = U_SIGNAL;
        }

        tctx->msg_counts[msg_type] ++;
        tctx->msg_sizes[msg_type] += len;
    }

    return DP_CBR_CONTINUE;
}


int
main (int argc, char **argv)
{
    dp_err_t dprc;
    qboolean print_info = false;
    dp_test_ctx_t tctx = {
        .print_callbacks = false,
        .time = -1,
        .finish_time = -1,
    };
    dp_callbacks_t callbacks = {
        .read = read,
        .server_info = server_info,
        .server_info_model = server_info_model,
        .server_info_sound = server_info_sound,
        .time = time,
        .baseline = baseline,
        .update = update,
        .packet_end = packet_end,
        .set_view = set_view,
        .intermission = intermission,
        .finale = finale,
        .cut_scene = cut_scene,
        .disconnect = disconnect,
        .update_stat = update_stat,
        .killed_monster = killed_monster,
        .found_secret = found_secret,
        .update_name = update_name,
        .message = message,
    };

    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s [callbacks|info|stats] demo-file\n",
                argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "callbacks") == 0) {
        tctx.print_callbacks = true;
    } else if (strcmp(argv[1], "info") == 0) {
        print_info = true;
    } else if (strcmp(argv[1], "stats") == 0) {
        tctx.collect_stats = true;
    } else {
        fprintf(stderr, "Invalid command: %s\n", argv[2]);
    }

    tctx.demo_file = fopen(argv[2], "rb");
    dprc = DP_ReadDemo(&callbacks, &tctx);
    fclose(tctx.demo_file);

    if (print_info) {
        printf("protocol=%d", tctx.protocol);
        printf(" flags=%x", tctx.protocol_flags);
        printf(" map=%s", tctx.map_name);
        printf(" kills=%d/%d",
               tctx.stats[STAT_MONSTERS],
               tctx.stats[STAT_TOTALMONSTERS]);
        printf(" secrets=%d/%d",
               tctx.stats[STAT_SECRETS],
               tctx.stats[STAT_TOTALSECRETS]);
        if (tctx.finish_time != -1) {
            int mins = ((int)tctx.finish_time) / 60;
            printf(" time=%d:%08.5f",
                   mins, tctx.finish_time - mins * 60.f);
        }
        printf("\n");
    }

    if (tctx.collect_stats) {
        int msg_type;
        unsigned long total_msg_size = 0;
        unsigned long total_msg_count = 0;
        unsigned long total_packet_header_size;
        unsigned long total_size;
        printf("%8s %10s %10s %8s\n",
               "msg_type", "count", "total_size", "avg_size");
        for (msg_type = 0; msg_type < 256; msg_type ++) {
            unsigned long msg_count = tctx.msg_counts[msg_type];
            unsigned long msg_size = tctx.msg_sizes[msg_type];
            float avg_size = (float)msg_size / msg_count;
            if (tctx.msg_counts[msg_type]) {
                printf("%8u %10lu %10lu %5.8f\n",
                       msg_type, msg_count, msg_size, avg_size);
                total_msg_size += msg_size;
                total_msg_count += msg_count;
            }

        }
        printf("\n");
        total_packet_header_size = 4 * 4 * tctx.packet_count;
        total_size = total_msg_size + total_packet_header_size;
        printf("         total_msg_count: %lu\n", total_msg_count);
        printf("          total_msg_size: %lu\n", total_size);
        printf("            packet_count: %lu\n", tctx.packet_count);
        printf("total_packet_header_size: %lu\n", total_packet_header_size);
        printf("              total_size: %lu\n", total_size);
        printf("\n");
        printf("note: total_size excludes any trailing bytes, and the initial "
               "force track command\n");
    }

    return dprc;
}
