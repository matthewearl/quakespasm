#include "quakedef.h"


#define LGSCAN_MAX  (1 << 12)
#define LGSCAN_FOV  90


typedef struct {
    vec3_t dirs[LGSCAN_MAX];
    int num_dirs;
} lgscan_info_t;


static lgscan_info_t lgscan_info;
static qboolean lgscan_pick = false;
int lgscan_highlighted = 0;


static void
LgScan_Pick (void)
{
    lgscan_pick = true;
}


void
LgScan_Init(void)
{ 
    Cmd_AddCommand ("lgscan_pick", &LgScan_Pick); //johnfitz
}


static edict_t *
LgScan_Single (edict_t *ent, vec3_t forward)
{
    vec3_t start, end, side_start, side_end;
    vec3_t f;
    trace_t trace;
    edict_t *hit = NULL;

    // Simulate the trace in `W_FireLightning`.
    VectorCopy(ent->v.origin, start);
    start[2] += 16.0f;
    VectorMA(start, 600, forward, end);
    trace = SV_Move (start, vec3_origin, vec3_origin, end, true, ent);

    // Traces in `LightningDamage`.
    VectorCopy(ent->v.origin, start);
    VectorMA(trace.endpos, 4.0f, forward, end);
    VectorSubtract(end, start, f);
    f[0] = -f[1];
    f[1] = f[0];
    f[2] = 0.0f;
    VectorScale(f, 16.0f, f);

    trace = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
    if (trace.ent && trace.ent->v.takedamage) {
        hit = trace.ent;
    }
    if (!hit) {
        VectorAdd(start, f, side_start);
        VectorAdd(end, f, side_end);
        trace = SV_Move (side_start, vec3_origin, vec3_origin, side_end, false, ent);
        if (trace.ent && trace.ent->v.takedamage) {
            hit = trace.ent;
        }
    }
    if (!hit) {
        VectorSubtract(start, f, side_start);
        VectorSubtract(end, f, side_end);
        trace = SV_Move (side_start, vec3_origin, vec3_origin, side_end, false, ent);
        if (trace.ent && trace.ent->v.takedamage) {
            hit = trace.ent;
        }
    }

    return hit;
}


void
LgScan_DoScan (edict_t *ent)
{
    vec3_t forward, right, up, dir;
    int i;
    float r1, r2, scale;
    edict_t *hit;

    scale = tanf(DEG2RAD(LGSCAN_FOV / 2));
    lgscan_info.num_dirs = 0;
    AngleVectors (ent->v.v_angle, forward, right, up);
    for (i = 0; i < LGSCAN_MAX; i++) {
        r1 = ((rand() & 0x7fff) / ((float)0x7fff)) * 2.0f - 1.0f;
        r2 = ((rand() & 0x7fff) / ((float)0x7fff)) * 2.0f - 1.0f;

        VectorMA(forward, r1 * scale, right, dir);
        VectorMA(dir, r2 * scale, up, dir);
        VectorNormalize(dir);

        if (LgScan_Single(ent, dir)) {
            VectorCopy(dir, lgscan_info.dirs[lgscan_info.num_dirs]);
            lgscan_info.num_dirs++;
        }
    }

    hit = LgScan_Single(ent, forward);
    if (hit) {
        //Con_Printf("Lg scan hit time: %.3f\n", sv.time);
        //ED_Print(hit);
    }

    if (lgscan_pick) {
        if (hit) {
            lgscan_highlighted = NUM_FOR_EDICT(hit);
            Con_Printf("Highlighed ent %d (%s)\n",
                       lgscan_highlighted,
                       PR_GetString(hit->v.classname));
        } else {
            lgscan_highlighted = 0;
            Con_Printf("Cleared lgscan highlight\n");
        }
        lgscan_pick = false;
    }
}

void
LgScan_Draw (void)
{
    int i;
    vec3_t t;

    if (!sv.active) {
        return;
    }

    glDisable (GL_DEPTH_TEST);
    glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset (OFFSET_SHOWTRIS);
    glDisable (GL_TEXTURE_2D);
    glDisable (GL_CULL_FACE);
    glPointSize(5);
    glColor3f (1,0,1);

    glBegin(GL_POINTS);
    for (i = 0; i < lgscan_info.num_dirs; i++) {
        VectorMA(r_refdef.vieworg, 20.0f, lgscan_info.dirs[i], t);
        glVertex3f(t[0], t[1], t[2]);
    }
    glEnd ();

    glColor3f (1,1,1);
    glEnable (GL_TEXTURE_2D);
    glEnable (GL_CULL_FACE);
    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset (OFFSET_NONE);
    glEnable (GL_DEPTH_TEST);
}

