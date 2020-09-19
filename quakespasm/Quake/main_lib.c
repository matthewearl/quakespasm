#include "quakedef.h"


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
    IN_MouseMotion(dx, dy);
}


void
do_frame(void)
{
    Host_Frame(0.);     // Time argument should be ignored
}


int
read_player_info(vec3_t pos, vec3_t vel, vec3_t view_angle)
{
    int rc;
    client_t *host_client = svs.clients;
    edict_t	*sv_player;

    if (host_client && host_client->active) {
        sv_player = host_client->edict;

        VectorCopy(sv_player->v.origin, pos);
        VectorCopy(sv_player->v.velocity, vel);
        VectorCopy(sv_player->v.v_angle, view_angle);

        rc = 1;
    } else {
        rc = 0;
    }

    return rc;
}

