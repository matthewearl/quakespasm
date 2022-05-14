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
 * MESSAGE PARSING
 */


typedef struct {
    int version;
    int protocolflags;
} protocol_t;


typedef struct {
    protocol_t protocol;

    /* view entity properties */
    int view_entity;
    vec3_t baseline_origin;
    vec3_t baseline_angle;
    int baseline_frame;
    qboolean updated;

    ghostrec_t *rec;  // next element to be appended
    ghostreclist_t **next_ptr;  // pointer to last element's "next" pointer
} parse_context_t;


static byte *Ghost_BufRead(byte *buf, byte *buf_end, int count, byte *dest)
{
    if (buf + count > buf_end) {
        Sys_Error("Demo file unexpectedly ended");
    }

    if (dest != NULL) {
        Q_memcpy(dest, buf, count);
    }
    return buf + count;
}


static int Ghost_CoordSize(parse_context_t *ctx)
{
    if (ctx->protocol.flags == -1) {
        Sys_Error("Protocol flags not set");
    }

	if (ctx->protocol.flags & PRFL_FLOATCOORD) {
        return 4;
    } else if (ctx->protocol.flags & PRFL_INT32COORD) {
        return 4;
    } else if (ctx->protocol.flags & PRFL_24BITCOORD) {
        return 3;
    } else {
        return 2;
    }
}


static byte *Ghost_ReadFloat(byte *buf, byte *buf_end, float *out)
{
    union {
        int l;
        float f;
    } data;

    buf = Ghost_BufRead(buf, buf_end, sizeof(data.l), &data.l);
    data.l = LittleLong(data.l);

    if (out) {
        *out = data.f;
    }
}

static byte *Ghost_ReadCoord(parse_context_t *ctx, byte *buf, byte *buf_end,
                             float *out)
{
    if (ctx->protocol.flags == -1) {
        Sys_Error("Protocol flags not set");
    }
	if (ctx->protocol.flags & PRFL_FLOATCOORD) {
        float f;
        buf = GhostBufRead(buf, buf_end, sizeof(f), &f);
        *out = LittleFloat(f);
    } else if (ctx->protocol.flags & PRFL_INT32COORD) {
        int i;
        buf = GhostBufRead(buf, buf_end, sizeof(i), &i);
        *out = LittleLong(i) / 16.0f;
    } else if (ctx->protocol.flags & PRFL_24BITCOORD) {
        short s;
        byte b;
        buf = GhostBufRead(buf, buf_end, sizeof(s), &s);
        buf = GhostBufRead(buf, buf_end, sizeof(b), &b);
        *out = LittleShort(s) + b / 255.0f;
    } else {
        short s;
        buf = GhostBufRead(buf, buf_end, sizeof(s), &s);
        *out = LittleShort(s) / 16.0f;
    }

    return buf;
}


static int Ghost_AngleSize(parse_context_t *ctx)
{
    if (ctx->protocol.flags == -1) {
        Sys_Error("Protocol flags not set");
    }

	if (ctx->protocol.flags & PRFL_FLOATANGLE) {
        return 4;
    } else if (ctx->protocol.flags & PRFL_SHORTANGLE) {
        return 2;
    } else {
        return 1;
    }
}


static byte *Ghost_ReadAngle(parse_context_t *ctx, byte *buf, byte *buf_end,
                             float *out)
{
    if (ctx->protocol.flags == -1) {
        Sys_Error("Protocol flags not set");
    }

	if (ctx->protocol.flags & PRFL_FLOATANGLE) {
        float f;
        buf = GhostBufRead(buf, buf_end, sizeof(f), &f);
        *out = LittleFloat(f);
    } else if (ctx->protocol.flags & PRFL_SHORTANGLE) {
        short s;
        buf = GhostBufRead(buf, buf_end, sizeof(s), &s);
        *out = LittleShort(s) * 360.0f / 65536.0f;
    } else {
        char c;
        buf = GhostBufRead(buf, buf_end, sizeof(c), &c);
        *out = c * 360.0f / 256.0f;
    }
}


static byte *Ghost_ReadString(byte *buf, byte *buf_end, int *len_out)
{
    int len = 0;
    char c;

    buf = Ghost_BufRead(buf, buf_end, sizeof(char), &c);
    while (c != -1 && c != 0) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(char), &c);
        len ++;
    }

    if (len_out != NULL) {
        *len_out = len;
    }

    return buf;
}


static byte *Ghost_ReadStringList(byte *buf, byte *buf_end)
{
    int len = -1;

    while (len != 0) {
        buf = Ghost_ReadString(buf, buf_end, &len);
    }

    return buf;
}


static byte *Ghost_ReadServerInfo(parse_context_t *ctx, byte *buf,
                                  byte *buf_end)
{
    buf = Ghost_BufRead(buf, buf_end, sizeof(ctx->protocol.version),
                        &ctx->protocol.version);
    ctx->protocol.version = LittleLong(ctx->protocol.version);

    if (ctx->protocol.version == PROTOCOL_RMQ) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(ctx->protocol.flags),
                            &ctx->protocol.flags);
        ctx->protocol.flags = LittleLong(ctx->protocol.flags);
    } else {
        ctx->protocol.flags = 0;
    }

    // skip maxclients and gametype
    buf = Ghost_BufRead(buf, buf_end, 2, NULL);

    // level name
    // TODO: Check this matches loaded level.
    buf = Ghost_ReadString(buf, buf_end, NULL);

    // models and sounds
    buf = Ghost_ReadStringList(buf, buf_end);
    buf = Ghost_ReadStringList(buf, buf_end);

    return buf;
}


static byte *Ghost_ReadSoundStart(parse_context_t *ctx, byte *buf,
                                  byte *buf_end)
{
    int skip = 0;
    byte field_mask;
    buf = Ghost_BufRead(buf, buf_end, sizeof(field_mask), &field_mask);

    if (field_mask & SND_VOLUME) {
        skip++;
    }
    if (field_mask & SND_ATTENUATION) {
        skip++;
    }
    skip += 2;
	if (field_mask & SND_LARGEENTITY) {
        skip ++;
    }
    skip ++;
	if (field_mask & SND_LARGESOUND) {
        skip ++;
    }
    skip += 3 * Ghost_CoordSize(ctx);

    buf = Ghost_BufRead(buf, buf_end, skip, NULL);

    return buf;
}


static byte *Ghost_ReadTempEntity(parse_context_t *ctx, byte *buf,
                                  byte *buf_end)
{
    byte type;

    buf = Ghost_BufRead(buf, buf_end, sizeof(byte), &type);

    switch (type) {
        case TE_LIGHTNING1:
        case TE_LIGHTNING2:
        case TE_LIGHTNING3:
        case TE_BEAM:
            skip = 2 + 6 * Ghost_CoordSize(ctx);
            break;
        case TE_EXPLOSION2:
            skip = 3 * Ghost_CoordSize(ctx);
            break;
        default:
            skip = 2 + 3 * Ghost_CoordSize(ctx);
            break;
    }

    return Ghost_BufRead(buf, buf_end, skip, NULL);
}


static byte *Ghost_ReadBaseline(parse_context_t *ctx,
                                byte *buf, byte *buf_end,
                                int view_entity,
                                qboolean include_entity_num,
                                int version)
{
    short entity_num = -1;
    byte bits = 0;
    int i;
    float origin_element, angle_element;

    if (include_entity_num) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(entity_num), &entity_num);
        entity_num = LittleShort(entity_num);
    }

    if (version == 2) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(bits), &bits);
    }

    buf = Ghost_BufRead(
        buf, buf_end,
        (bits & B_LARGEMODEL) ? sizeof(short) : sizeof(byte),
        NULL
    );

    if (bits & B_LARGEFRAME) {
        short frame_short;
        buf = Ghost_BufRead(buf, buf_end, sizeof(frame_short), &frame_short);
        ctx->baseline_frame = LittleShort(frame_short);
    } else {
        byte frame_byte;
        buf = Ghost_BufRead(buf, buf_end, sizeof(frame_byte), &frame_byte);
        ctx->baseline_frame = frame_byte;
    }

    // skip colormap, skin
    buf = Ghost_BufRead(buf, buf_end, 2, NULL);

	for (i = 0; i < 3; i++) {
        buf = Ghost_ReadCoord(ctx, buf, buf_end, &origin_element);
        buf = Ghost_ReadAngle(ctx, buf, buf_end, &angle_element);

        if (include_entity_num && view_entity == entity_num) {
            ctx->baseline_origin[i] = origin_element;
            ctx->baseline_angle[i] = angle_element;
        }
    }

    if (bits & B_ALPHA) {
        buf = Ghost_BufRead(buf, buf_end, 1, NULL);
    }

    return buf;
}


static byte *Ghost_ReadUpdateFlags(parse_context_t *ctx,
                                   byte *buf, byte *buf_end,
                                   unsigned int *flags_out)
{
    byte base_flags, more_flags, extend1_flags, extend2_flags;
    unsigned int flags = 0;

    buf = Ghost_BufRead(buf, buf_end, sizeof(base_flags), &base_flags);
    flags |= base_flags;

    if (flags & U_MOREBITS) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(more_flags), &more_flags);
        flags |= more_flags << 8;
    }

    if (ctx->protocol.version != PROTOCOL_NETQUAKE) {
        if (flags & U_EXTEND1) {
            buf = Ghost_BufRead(buf, buf_end,
                                sizeof(extend1_flags), &extend1_flags);
            flags |= extend1_flags << 16;
        }
        if (flags & U_EXTEND2) {
            buf = Ghost_BufRead(buf, buf_end,
                                sizeof(extend2_flags), &extend2_flags);
            flags |= extend2_flags << 24;
        }
    }

    *flags_out = flags;
}


static byte *Ghost_ReadUpdate(parse_context_t *ctx,
                              byte *buf, byte *buf_end)
{
    int i;
    bool is_view;
    unsigned int flags;
    int entity_num;
    int skip;

    buf = Ghost_ReadUpdateFlags(buf, buf_end, &flags);

    if (flags & U_LONGENTITY) {
        short entity_num_short;
        buf = Ghost_BufRead(buf, buf_end,
                            sizeof(entity_num_short),
                            &entity_num_short);
        entity_num = entity_num_short;
    } else {
        byte entity_num_byte;
        buf = Ghost_BufRead(buf, buf_end,
                            sizeof(entity_num_byte),
                            &entity_num_byte);
        entity_num = entity_num_byte;
    }

    if (ctx->view_entity == -1) {
        Sys_Error("Update received but view entity not set");
    }
    is_view = entity_num == ctx->view_entity;

    if (flags & U_MODEL) {
        buf = Ghost_BufRead(buf, buf_end, sizeof(byte), NULL);
    }

    if (flags & U_FRAME) {
        byte frame_byte;
        buf = Ghost_BufRead(buf, buf_end, sizeof(frame_byte), &frame_byte);
        if (is_view) {
            ctx->rec->frame = frame_byte;
        }
    } else {
        if (is_view) {
            ctx->rec->frame = ctx->baseline_frame;
        }
    }

    skip = ((flags & U_COLORMAP) != 0)
           + ((flags & U_SKIN) != 0)
           + ((flags & U_EFFECTS) != 0);
    buf = Ghost_BufRead(buf, buf_end, skip, NULL);

    for (i = 0; i < 3; i++) {
        if (flags & (U_ORIGIN1 << i)) {
            Ghost_ReadCoord(ctx, buf, buf_end, &ctx->rec->origin[i]);
        } else {
            ctx->rec->origin[i] = ctx->baseline_origin[i];
        }
        if (flags & (U_ANGLE1 << i)) {
            Ghost_ReadAngle(ctx, buf, buf_end, &ctx->rec->angle[i]);
        } else {
            ctx->rec->angle[i] = ctx->baseline_angle[i];
        }
    }

	if (ctx->protocol == PROTOCOL_FITZQUAKE || ctx->protocol == PROTOCOL_RMQ)
	{
        skip = ((flags & U_ALPHA) != 0)
               + ((flags & U_SCALE) != 0);
        buf = Ghost_BufRead(buf, buf_end, skip, NULL);

        if (flags & U_FRAME2) {
            byte frame2;

            buf = Ghost_BufRead(buf, buf_end, sizeof(frame2), &frame2);
            ctx->rec->frame = (ctx->rec->frame & 0xFF) | (frame2 << 8);
        }

        skip = ((flags & U_MODEL2) != 0)
               + ((flags & U_LERPFINISH) != 0);
        buf = Ghost_BufRead(buf, buf_end, skip, NULL);
    } else if (ctx->protocol == PROTOCOL_NETQUAKE
               && (flags & U_TRANS)) {
        float a;

        buf = Ghost_ReadFloat(buf, buf_end, &a);
        buf = Ghost_ReadFloat(buf, buf_end, NULL);
        if (a == 2) {
            buf = Ghost_ReadFloat(buf, buf_end, NULL);
        }
    }

    return buf;
}


static byte *Ghost_ReadClientDataFlags(parse_context_t *ctx,
                                       byte *buf, byte *buf_end,
                                       unsigned int *flags_out)
{
    unsigned shot base_flags;
    byte extend1_flags, extend2_flags;
    unsigned int flags = 0;

    buf = Ghost_BufRead(buf, buf_end, sizeof(base_flags), &base_flags);
    flags |= base_flags;

    if (flags & SU_EXTEND1) {
        buf = Ghost_BufRead(buf, buf_end,
                            sizeof(extend1_flags), &extend1_flags);
        flags |= extend1_flags << 16;
    }
    if (flags & SU_EXTEND2) {
        buf = Ghost_BufRead(buf, buf_end,
                            sizeof(extend2_flags), &extend2_flags);
        flags |= extend2_flags << 24;
    }

    *flags_out = flags;
}


static byte *Ghost_ReadClientData(parse_context_t *ctx, byte *buf,
                                  byte *buf_end)
{
    unsigned int flags;
    int skip;

    buf = Ghost_ReadClientDataFlags(ctx, buf, buf_end, &flags)

    skip = ((flags & SU_VIEWHEIGHT) != 0)
           + ((flags & SU_IDEALPITCH) != 0)
           + ((flags & SU_PUNCH1) != 0)
           + ((flags & SU_PUNCH2) != 0)
           + ((flags & SU_PUNCH3) != 0)
           + ((flags & SU_VELOCITY1) != 0)
           + ((flags & SU_VELOCITY2) != 0)
           + ((flags & SU_VELOCITY3) != 0)
           + 4  // items
           + ((flags & SU_WEAPONFRAME) != 0)
           + ((flags & SU_ARMOR) != 0)
           + ((flags & SU_WEAPON) != 0)
           + 2 + 6  // health,ammo,shells,nails,rockets,cells,active_weapon
           + ((flags & SU_WEAPON2) != 0)
           + ((flags & SU_ARMOR2) != 0)
           + ((flags & SU_AMMO2) != 0)
           + ((flags & SU_SHELLS2) != 0)
           + ((flags & SU_NAILS2) != 0)
           + ((flags & SU_ROCKETS2) != 0)
           + ((flags & SU_CELLS2) != 0)
           + ((flags & SU_WEAPONFRAME2) != 0)
           + ((flags & SU_WEAPONALPHA) != 0);
    buf = Ghost_BufRead(buf, buf_end, skip, NULL);
}


static byte *Ghost_ReadMessage(parse_context_t *ctx,
                               byte *buf, byte *buf_end)
{
    byte cmd;
    float time;
    int skip;
    ghostrec_t rec;

    ctx->rec = &rec;
    ctx->updated = false;

    buf = Ghost_BufRead(buf, buf_end, sizeof(msg_type), &cmd);

    if (cmd & U_SIGNAL) {
        buf = Ghost_ReadUpdate(ctx, buf, buf_end);
    } else {
        skip = 0;
        switch (cmd) {
            case svc_time:
                buf = Ghost_BufRead(buf, buf_end, sizeof(float), &rec.time);
                rec.time = LittleFloat(rec.time);
                break;
            case svc_nop:
            case svc_disconnect:
            case svc_killedmonster:
            case svc_foundsecret:
            case svc_intermission:
            case svc_sellscreen:
            case svc_bf:
                break;
            case svc_clientdata:
                buf = Ghost_ReadClientData(buf, buf_end);
                break;
            case svc_version:
                buf = Ghost_BufRead(buf, buf_end, sizeof(ctx->protocol.version),
                                    &ctx->protocol.version);
                ctx->protocol.version = LittleLong(ctx->protocol.version);
                if (ctx->protocol.version != PROTOCOL_NETQUAKE
                    && ctx->protocol.version != PROTOCOL_FITZQUAKE
                    && ctx->protocol.version != PROTOCOL_RMQ) {
                    Sys_Error("Invalid protocol %d", ctx->protocol.version);
                }
                break;
            case svc_print:
            case svc_centerprint:
            case svc_stufftext:
            case svc_finale:
            case svc_cutscene:
            case svc_skybox:
                buf = Ghost_ReadString(buf, buf_end, NULL);
                break;
            case svc_damage:
                skip = 2 + 3 * Ghost_CoordSize(ctx);
                break;
            case svc_serverinfo:
                buf = Ghost_ReadServerInfo(ctx, buf, buf_end);
                break;
            case svc_setangle:
                skip = 3 * Ghost_AngleSize(ctx);
                break;
            case svc_setview:
                buf = Ghost_BufRead(buf, buf_end, sizeof(view_entity),
                                    &view_entity);
                view_entity = LittleShort(view_entity);
                break;
            case svc_sound:
                buf = Ghost_ReadSoundStart(ctx, buf, buf_end);
                break;
            case svc_stopsound:
                buf = Ghost_BufRead(buf, buf_end, sizeof(short), NULL);
                break;
            case svc_updatename:
                buf = Ghost_BufRead(buf, buf_end, sizeof(byte), NULL);
                buf = Ghost_ReadString(buf, buf_end, NULL);
                break;
            case svc_updatefrags:
                skip = 3;
                break;
            case svc_colors:
                skip = 2;
                break;
            case svc_particle:
                skip = 3 * Ghost_CoordSize(ctx) + 3 + 2;
                break;
            case svc_spawnbaseline:
                buf = Ghost_ReadBaseline(ctx, buf, buf_end,
                                         view_entity, true, 1, &rec);
                break;
            case svc_spawnstatic:
                buf = Ghost_ReadBaseline(ctx, buf, buf_end,
                                         view_entity, false, 1, &rec);
                break;
            case svc_tent:
                buf = Ghost_ReadTempEntity(ctx, buf, buf_end);
                break;
            case svc_pause:
                skip = 1;
                break;
            case svc_signonnum:
                skip = 1
                break;
            case svc_updatestat:
                skip = 5;
                break;
            case svc_spawnstaticsound:
                skip = 3 * Ghost_CoordSize(ctx) + 3;
                break;
            case svc_cdtrack:
                skip = 2;
                break;
            case svc_fog:
                skip = 6;
                break;
            case svc_spawnbaseline2:
                buf = Ghost_ReadBaseline(ctx, buf, buf_end,
                                         view_entity, true, 2, &rec);
                break;
            case svc_spawnstatic2:
                buf = Ghost_ReadBaseline(ctx, buf, buf_end,
                                         view_entity, false, 2, &rec);
                break;
            case svc_spawnstaticsound2:
                skip = 3 * Ghost_CoordSize(ctx) + 4;
                break;
        }

        if (size) {
            buf = Ghost_BufRead(buf, buf_end, size, NULL);
        }
    }

    // If an update for the view entity was received, append it to the list.
    if (ctx->updated) {
        Ghost_Append(&ctx->next_ptr, &ctx->rec);
    }
    ctx->rec = NULL;
}


static byte *Ghost_ReadDemoPacket(byte *buf, byte *buf_end,
                                  ghostreclist_t ***next_ptr) {
    byte *msg_end;
    parse_context_t ctx = {.protocol = {-1, -1}, .view_entity = -1};

    // demo message header
    buf = Ghost_BufRead(buf, buf_end, sizeof(int), &msg_len);
	net_message.cursize = LittleLong (net_message.cursize);
    buf = Ghost_BufRead(buf, buf_end, sizeof(float) * 3, NULL); // view angles

    msg_end = buf + msg_len;
    if (msg_end > buf_end) {
        Sys_Error("Demo file unexpectedly ended");
    }

    while (buf < msg_end) {
        buf = Ghost_ReadMessage(&ctx, buf, msg_end);
    }
}


/*
 * ENTRYPOINT
 */


void Ghost_ReadDemo(const char *demo_path, ghostrec_t **records,
                    int *num_records) {
    byte *buf;
    byte *buf_end;
    ghostreclist_t *list = NULL;
    ghostreclist_t **next_ptr = &list;

    buf = COM_LoadStackFile(demo_path, NULL, 0, NULL);
    buf_end = buf + com_filesize;
    while (buf < buf_end) {
        buf = Ghost_ReadDemoPacket(buf, &next_ptr);
    }
    Ghost_ListToArray(list, records, num_records);

    // Free everything
    Ghost_ListFree(list);
    Hunk_HighMark();
}

