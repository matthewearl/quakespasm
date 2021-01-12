#include "quakedef.h"


// Stub implementations

qpic_t *pic_ovr, *pic_ins; //johnfitz -- new cursor handling
int clearnotify;
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
qboolean    scr_disabled_for_loading;
int			glheight;
modestate_t	modestate = MS_UNINIT;
viddef_t	vid;
cvar_t	r_lerpmove;
vec3_t	vright;
float		scr_centertime_off;
vec3_t	r_origin;
int	scr_tileclear_updates = 0;
cvar_t	r_lerpmodels = {"r_lerpmodels", "1", CVAR_NONE};
refdef_t	r_refdef;
cvar_t		vid_gamma = {"gamma", "1", CVAR_ARCHIVE};
cvar_t		vid_contrast = {"contrast", "1", CVAR_ARCHIVE};
cvar_t		scr_viewsize = {"viewsize","100", CVAR_ARCHIVE};
cvar_t		scr_conscale = {"scr_conscale", "1", CVAR_ARCHIVE};
cvar_t		scr_sbaralpha = {"scr_sbaralpha", "0.75", CVAR_ARCHIVE};
float		scr_con_current;
cvar_t		bgmvolume = {"bgmvolume", "1", CVAR_ARCHIVE};
cvar_t		sfxvolume = {"volume", "0.7", CVAR_ARCHIVE};
cvar_t		bgm_extmusic = {"bgm_extmusic", "1", CVAR_ARCHIVE};
cvar_t	r_nolerp_list = {"r_nolerp_list", "", CVAR_NONE};
cvar_t	r_noshadow_list = {"r_noshadow_list", "", CVAR_NONE};
unsigned int d_8to24table[256];
cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
int gl_warpimagesize = 0;


void SCR_UpdateScreen (void)
{
}


void CL_RunParticles (void)
{
}


void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
}


void S_TouchSound (const char *name)
{
}


void S_BeginPrecaching (void)
{
}

void S_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
}

void Sbar_Changed (void)
{
}

sfx_t *S_PrecacheSound (const char *name)
{
    return NULL;
}


void S_EndPrecaching (void)
{
}


void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}


void S_StopSound (int entnum, int entchannel)
{
}


void S_StopAllSounds (qboolean clear)
{
}

void BGM_Stop (void)
{
}


void BGM_PlayCDtrack (byte track, qboolean looping)
{
}


void BGM_Pause (void)
{
}


void BGM_Resume (void)
{
}


void BGM_Update (void)
{
}


void SCR_BeginLoadingPlaque (void)
{
}


void SCR_EndLoadingPlaque (void)
{
}


void SCR_CenterPrint (const char *str) //update centerprint data
{
}


void Sys_Sleep (unsigned long msecs)
{
}


double Sys_DoubleTime (void)
{
    return 0.0;
}


// Like R_ParseParticleEffect in r_part.c except it just does the parsing
void R_ParseParticleEffect (void)
{
	int			i;

	for (i=0 ; i<3 ; i++)
		MSG_ReadCoord (cl.protocolflags);
	for (i=0 ; i<3 ; i++)
		MSG_ReadChar ();
	MSG_ReadByte ();
	MSG_ReadByte ();
}


void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
}


void D_FlushCaches (void)
{
}


void GL_SubdivideSurface (msurface_t *fa)
{
}


void GL_MakeAliasModelDisplayLists (qmodel_t *m, aliashdr_t *hdr)
{
}


gltexture_t *TexMgr_LoadImage (qmodel_t *owner, const char *name, int width, int height, enum srcformat format,
			       byte *data, const char *source_file, src_offset_t source_offset, unsigned flags)
{
    return NULL;
}


byte *Image_LoadImage (const char *name, int *width, int *height)
{
    return NULL;
}

void Sky_LoadTexture (texture_t *mt)
{
}


int TexMgr_PadConditional (int s)
{
    return s;
}


void R_RenderView (void)
{
}


void R_NewMap (void)
{
}


void R_TranslateNewPlayerSkin (int playernum)
{
}


void R_SplitEntityOnNode (mnode_t *node)
{
}


void R_CheckEfrags (void)
{
}


void R_AddEfrags (entity_t *ent)
{
}


void R_StoreEfrags (efrag_t **ppefrag)
{
}

// Input stubs


extern cvar_t cl_maxpitch;
extern cvar_t cl_minpitch;

static vec3_t   in_viewangle = {0.f, 0.f, 0.f};

static qboolean   in_override_usercmd = false;
static usercmd_t  in_usercmd = {};


void IN_Commands (void)
{
}


void IN_SendKeyEvents (void)
{
}


void IN_UpdateInputMode (void)
{
}


void IN_AddAngleDelta(float dx, float dy)
{
	in_viewangle[YAW] += dx;
	in_viewangle[PITCH] += dy;
}


void IN_SetAngle(float yaw, float pitch)
{
	in_viewangle[YAW] = yaw;
	in_viewangle[PITCH] = pitch;
}


void IN_Activate (void)
{
}


void IN_Deactivate (qboolean free_cursor)
{
}


void IN_Move(usercmd_t *cmd)
{
    if (in_override_usercmd) {
        VectorCopy(in_usercmd.viewangles, cl.viewangles);

        cmd->forwardmove = in_usercmd.forwardmove;
        cmd->sidemove = in_usercmd.sidemove;
        cmd->upmove = in_usercmd.upmove;

        in_override_usercmd = false;
    } else {
        cl.viewangles[YAW] = in_viewangle[YAW];
        cl.viewangles[PITCH] = in_viewangle[PITCH];

        if (cl.viewangles[PITCH] > cl_maxpitch.value)
            cl.viewangles[PITCH] = cl_maxpitch.value;
        if (cl.viewangles[PITCH] < cl_minpitch.value)
            cl.viewangles[PITCH] = cl_minpitch.value;
    }
}


void TexMgr_FreeTexturesForOwner (qmodel_t *owner)
{
}


static quakeparms_t	parms;


#define DEFAULT_MEMORY (256 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)


void
start_host(int argc, char *argv[])
{
	int t;

	host_parms = &parms;
	parms.basedir = ".";

	parms.argc = argc;
	parms.argv = argv;

	parms.errstate = 0;

	COM_InitArgv(parms.argc, parms.argv);

    isLibrary = true;
    if (COM_CheckParm("-dedicated") != 0)
		Sys_Error ("-dedicated must be 0 when in library mode\n");
	isDedicated = false;

	Sys_Init();

	parms.memsize = DEFAULT_MEMORY;
	if (COM_CheckParm("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;
		if (t < com_argc)
			parms.memsize = Q_atoi(com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

    // TODO: Wrap Sys_Printf so it calls a callback in the library?
	Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
	Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
	Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
	Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
	Sys_Printf("QuakeSpasm " QUAKESPASM_VER_STRING " (c) Ozkan Sezer, Eric Wasylishen & others\n");

	Sys_Printf("Host_Init\n");
	Host_Init();
}


void
add_command(const char *command)
{
    Cbuf_InsertText(command);
}


void
add_key_event(int key, int down)
{
    Key_Event(key, down);
}


void
add_mouse_motion(int dx, int dy)
{
    IN_AddAngleDelta(dx, dy);
}


void
set_angle(float yaw, float pitch)
{
    IN_SetAngle(yaw, pitch);
}


void
set_usercmd(vec3_t view_angle, float fmove, float smove, float upmove)
{
    VectorCopy(view_angle, in_usercmd.viewangles);

    in_usercmd.forwardmove = fmove;
    in_usercmd.sidemove = smove;
    in_usercmd.upmove = upmove;

    in_override_usercmd = true;
}


void
do_frame(void)
{
    Host_Frame(0.);     // Time argument should be ignored
}


int
read_player_info(vec3_t pos, vec3_t vel, vec3_t view_angle, qboolean *jump_released, qboolean *on_ground,
                 vec3_t cl_pos, vec3_t cl_vel)
{
    int rc;
    client_t *host_client = svs.clients;
    edict_t	*sv_player;
    entity_t *ent;

    if (host_client && host_client->active) {
        sv_player = host_client->edict;

        VectorCopy(sv_player->v.origin, pos);
        VectorCopy(sv_player->v.velocity, vel);
        VectorCopy(sv_player->v.v_angle, view_angle);

        ent = &cl_entities[cl.viewentity];

        *jump_released = (int)(((int)sv_player->v.flags & FL_JUMPRELEASED) != 0);
        *on_ground = (int)(((int)sv_player->v.flags & FL_ONGROUND) != 0);

        VectorCopy(ent->msg_origins[0], cl_pos);
        VectorCopy(cl.mvelocity[0], cl_vel);

        rc = 1;
    } else {
        rc = 0;
    }

    return rc;
}


void
check_intermission(int *intermission, double *completed_time)
{
    *intermission = cl.intermission;
    *completed_time = cl.completed_time_double;
}


double
get_time(void)
{
    return sv.time;
}


// Enable this for profiling
#if 0
int
main(int argc, char **argv)
{
    vec3_t pos, vel, view_angle;
    int b = 0;
    int i, j;

    start_host(argc, argv);
    add_command("map e1m1");

    for (j = 0; j < 100; j++) {
        while (!b) {
            do_frame();
            b = (read_player_info(pos, vel, view_angle) == 1);
        }

        for (i = 0; i < 72; i++) {
            do_frame();
            read_player_info(pos, vel, view_angle);
        }

        add_key_event(119, true);

        for (i = 0; i < 72 * 4; i++) {
            do_frame();
            read_player_info(pos, vel, view_angle);
        }

        add_command("restart");
    }
}
#endif

