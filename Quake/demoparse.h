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


#ifndef __DEMOPARSE_H
#define __DEMOPARSE_H

#include "quakedef.h"


#define DP_FOREACH_ERR(F)          \
    F(DP_ERR_SUCCESS)              \
    F(DP_ERR_UNEXPECTED_END)       \
    F(DP_ERR_PROTOCOL_NOT_SET)     \
    F(DP_ERR_STRING_TOO_LONG)      \
    F(DP_ERR_CALLBACK)             \
    F(DP_ERR_BAD_SIZE)             \
    F(DP_ERR_INVALID_VERSION)      \
    F(DP_ERR_UNKNOWN_MESSAGE_TYPE) \

#define DP_GENERATE_ENUM(ENUM) ENUM,
#define DP_GENERATE_STRING(STRING) #STRING,


typedef enum {
    DP_FOREACH_ERR(DP_GENERATE_ENUM)
} dp_err_t;


typedef struct {
    qboolean (*server_info)(int protocol, unsigned int protocol_flags,
                            const char *level_name, void *ctx);
    qboolean (*server_info_model)(const char *level_name, void *ctx);
    qboolean (*server_info_sound)(const char *level_name, void *ctx);
    qboolean (*time)(float time, void *ctx);
    qboolean (*baseline)(int entity_num, vec3_t origin, vec3_t angle, int frame,
                         void *ctx);
    qboolean (*update)(int entity_num, vec3_t origin, vec3_t angle,
                       byte origin_bits, byte angle_bits, int frame, void *ctx);
    qboolean (*packet_end)(void *ctx);
    qboolean (*set_view)(int entity_num, void *ctx);
    qboolean (*intermission)(void *ctx);
    qboolean (*finale)(void *ctx);
    qboolean (*cut_scene)(void *ctx);
    qboolean (*disconnect)(void *ctx);
    qboolean (*update_stat)(byte stat, int count, void *ctx);
    qboolean (*killed_monster)(void *ctx);
    qboolean (*found_secret)(void *ctx);
} dp_callbacks_t;


dp_err_t DP_ReadDemo(const byte *data, unsigned int data_len,
                     dp_callbacks_t *callbacks, void *callback_ctx);

#endif /* __DEMOPARSE_H */
