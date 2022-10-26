/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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
#include "q_ctype.h"

extern cvar_t	pausable;
extern cvar_t	nomonsters;

cvar_t			sv_autoload = {"sv_autoload", "1", CVAR_ARCHIVE}; // 0 = no, 1 = ask, 2 = always

int	current_skill;

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f (void)
{
	if (key_dest != key_console && cls.state != ca_dedicated)
	{
		M_Menu_Quit_f ();
		return;
	}
	CL_Disconnect ();
	Host_ShutdownServer(false);

	Sys_Quit ();
}

//==============================================================================
//johnfitz -- extramaps management
//==============================================================================

/*
==================
FileList_Compare

Case-insensitive string comparison that also handles integer sequences
(e.g. d1m9 comes before d1m10)
==================
*/
static int FileList_Compare (const char *s1, const char *s2)
{
	if (s1 == s2)
		return 0;

skip_prefix:
	while (*s1 && !q_isdigit (*s1) && q_toupper (*s1) == q_toupper (*s2))
	{
		s1++;
		s2++;
		continue;
	}

	if (q_isdigit (*s1) && q_isdigit (*s2))
	{
		unsigned int num1 = *s1++ - '0';
		unsigned int num2 = *s2++ - '0';
		while (q_isdigit (*s1))
			num1 = num1 * 10 + (*s1++ - '0');
		while (q_isdigit (*s2))
			num2 = num2 * 10 + (*s2++ - '0');
		if (num1 != num2)
			return num1 < num2 ? -1 : 1;
		goto skip_prefix;
	}

	return q_toupper (*s1) - q_toupper (*s2);
}

/*
==================
FileList_AddWithData
==================
*/
static void FileList_AddWithData (const char *name, const void *data, size_t datasize, filelist_item_t **list)
{
	filelist_item_t	*item,*cursor,*prev;

	// ignore duplicate
	for (item = *list; item; item = item->next)
	{
		if (!Q_strcmp (name, item->name))
			return;
	}

	item = (filelist_item_t *) Z_Malloc(sizeof(filelist_item_t) + datasize);
	q_strlcpy (item->name, name, sizeof(item->name));
	if (datasize)
		memcpy (item + 1, data, datasize);

	// insert each entry in alphabetical order
	if (*list == NULL ||
	    FileList_Compare (item->name, (*list)->name) < 0) //insert at front
	{
		item->next = *list;
		*list = item;
	}
	else //insert later
	{
		prev = *list;
		cursor = (*list)->next;
		while (cursor && (FileList_Compare (item->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		item->next = prev->next;
		prev->next = item;
	}
}

/*
==================
FileList_Add
==================
*/
static void FileList_Add (const char *name, filelist_item_t **list)
{
	FileList_AddWithData (name, NULL, 0, list);
}

/*
==================
FileList_Clear
==================
*/
static void FileList_Clear (filelist_item_t **list)
{
	filelist_item_t *blah;

	while (*list)
	{
		blah = (*list)->next;
		Z_Free(*list);
		*list = blah;
	}
}

/*
==================
RightPad
==================
*/
static const char *RightPad (const char *str, size_t minlen, char c)
{
	static char buf[1024];
	size_t len = strlen (str);

	minlen = q_min (minlen, sizeof (buf) - 1);
	if (len >= minlen)
		return str;

	memcpy (buf, str, len);
	for (; len < minlen; len++)
		buf[len] = c;
	buf[len] = '\0';

	return buf;
}

filelist_item_t *extralevels;
filelist_item_t **extralevels_sorted;
size_t maxlevelnamelen;

static SDL_Thread*	extralevels_parsing_thread;
static SDL_atomic_t	extralevels_cancel_parsing;

/*
==================
FileList_Print

Prints all items in list (containing substr, if any)
Note: types array contains singular/plural forms for the list type
==================
*/
static void FileList_Print (filelist_item_t *list, const char *types[2], const char *substr)
{
	int				i;
	filelist_item_t	*item;
	const char		*desc;
	char			buf[256], buf2[256];
	char			padchar = '.' | 0x80;
	size_t			ofsdesc = list == extralevels ? maxlevelnamelen + 2 : 0;

	if (substr && *substr)
	{
		for (item = list, i = 0; item; item = item->next)
		{
			if (list == extralevels && ExtraMaps_GetType (item) >= MAPTYPE_ID_START)
				continue;
			desc = ofsdesc ? ExtraMaps_GetMessage (item) : NULL;
			if (!desc)
				desc = "";
			if (q_strcasestr (item->name, substr) || q_strcasestr (desc, substr))
			{
				const char *tinted_name = COM_TintSubstring (item->name, substr, buf, sizeof (buf));
				const char *tinted_desc = COM_TintSubstring (desc, substr, buf2, sizeof (buf2));
				if (*desc)
					Con_SafePrintf ("   %s%c%s\n", RightPad (tinted_name, ofsdesc, padchar), padchar, tinted_desc);
				else
					Con_SafePrintf ("   %s\n", tinted_name);
				i++;
			}
		}

		if (i)
			Con_SafePrintf ("%i %s containing \"%s\"\n", i, types[i!=1], substr);
		else
			Con_SafePrintf ("no %s found containing \"%s\"\n", types[1], substr);
	}
	else
	{
		for (item = list, i = 0; item; item = item->next)
		{
			if (list == extralevels && ExtraMaps_GetType (item) >= MAPTYPE_ID_START)
				continue;
			desc = ofsdesc ? ExtraMaps_GetMessage (item) : NULL;
			if (desc && *desc)
				Con_SafePrintf ("   %s%c%s\n", RightPad (item->name, ofsdesc, padchar), padchar, desc);
			else
				Con_SafePrintf ("   %s\n", item->name);
			i++;
		}

		if (i)
			Con_SafePrintf ("%i %s\n", i, types[i!=1]);
		else
			Con_SafePrintf ("no %s found\n", types[1]);
	}
}

/*
==================
ExtraMaps_Categorize
==================
*/
static maptype_t ExtraMaps_Categorize (const char *name, const searchpath_t *source)
{
	size_t len = strlen (name);
	maptype_t base;
	qboolean is_start, is_end, is_dm;

	if (!source)
	{
		switch (name[0])
		{
		case 'd':
			if (name[1] == 'm')
				return MAPTYPE_ID_DM;
			break;
		case 's':
			if (!strcmp (name + 1, "tart"))
				return MAPTYPE_ID_START;
			break;
		case 'e':
			if (name[1] >= '1' && name[1] <= '4')
				return MAPTYPE_ID_EP1_LEVEL + (name[1] - '1');
			if (!strcmp (name + 1, "nd"))
				return MAPTYPE_ID_END;
			break;
		default:
			break;
		}
		return MAPTYPE_ID_LEVEL;
	}

	is_start = (len >= 5 && (!memcmp (name + len - 5, "start", 5) ||
		!memcmp (name, "start", 5) || !memcmp (name + len - 5, "intro", 5)));
	is_end = (len >= 3 && !memcmp (name + len - 3, "end", 3));
	while (len > 0 && (unsigned int)(name[len - 1] - '0') <= 9)
		len--;
	is_dm = (len >= 2 && !memcmp (name + len - 2, "dm", 2));

	if (source->path_id != com_searchpaths->path_id)
	{
		if (is_start)
			return MAPTYPE_CUSTOM_ID_START;
		if (is_end)
			return MAPTYPE_CUSTOM_ID_END;
		if (is_dm)
			return MAPTYPE_CUSTOM_ID_DM;
		return MAPTYPE_CUSTOM_ID_LEVEL;
	}

	base = *source->filename ? MAPTYPE_CUSTOM_MOD_START : MAPTYPE_MOD_START;
	if (is_start)
		return base + MAPTYPE_CUSTOM_MOD_START;
	if (is_end)
		return base + MAPTYPE_CUSTOM_MOD_END;
	if (is_dm)
		return base + MAPTYPE_CUSTOM_MOD_DM;
	return base + MAPTYPE_CUSTOM_MOD_LEVEL;
}

typedef struct levelinfo_s
{
	SDL_atomic_t	type;
	const char		*message;
} levelinfo_t;

/*
==================
ExtraMaps_GetInfo
==================
*/
static const levelinfo_t *ExtraMaps_GetInfo (const filelist_item_t *item)
{
	return (const levelinfo_t *) (item + 1);
}

/*
==================
ExtraMaps_GetType
==================
*/
maptype_t ExtraMaps_GetType (const filelist_item_t *item)
{
	const levelinfo_t *info = ExtraMaps_GetInfo (item);
	return SDL_AtomicGet ((SDL_atomic_t *) &info->type);
}

/*
==================
ExtraMaps_GetMessage
==================
*/
const char *ExtraMaps_GetMessage (const filelist_item_t *item)
{
	const levelinfo_t *info = ExtraMaps_GetInfo (item);
	return (const char *) SDL_AtomicGetPtr ((void **) &info->message);
}

/*
==================
ExtraMaps_IsStart
==================
*/
qboolean ExtraMaps_IsStart (maptype_t type)
{
	return
		type == MAPTYPE_CUSTOM_MOD_START ||
		type == MAPTYPE_MOD_START ||
		type == MAPTYPE_CUSTOM_ID_START ||
		type == MAPTYPE_ID_START
	;
}

/*
==================
ExtraMaps_Sort
==================
*/
static void ExtraMaps_Sort (void)
{
	int counts[MAPTYPE_COUNT];
	int i, sum;
	filelist_item_t *item;

	memset (counts, 0, sizeof (counts));
	for (item = extralevels; item; item = item->next)
		counts[ExtraMaps_GetType (item)]++;

	for (i = sum = 0; i < MAPTYPE_COUNT; i++)
	{
		int tmp = counts[i];
		counts[i] = sum;
		sum += tmp;
	}
	sum++; // NULL terminator

	extralevels_sorted = (filelist_item_t **) realloc (extralevels_sorted, sizeof (*extralevels_sorted) * sum);
	if (!extralevels_sorted)
		Sys_Error ("ExtraMaps_Sort: out of memory on %d items", sum);

	for (item = extralevels; item; item = item->next)
		extralevels_sorted[counts[ExtraMaps_GetType (item)]++] = item;
	extralevels_sorted[sum - 1] = NULL;
}

/*
==================
ExtraMaps_Add
==================
*/
static void ExtraMaps_Add (const char *name, const searchpath_t *source)
{
	levelinfo_t info;
	memset (&info, 0, sizeof (info));
	info.type.value = ExtraMaps_Categorize (name, source);
	FileList_AddWithData (name, &info, sizeof (info), &extralevels);
	maxlevelnamelen = q_max (maxlevelnamelen, strlen (name));
}

/*
==================
ExtraMaps_ParseDescriptions
==================
*/
static int ExtraMaps_ParseDescriptions (void *unused)
{
	char buf[1024];
	int i;

	for (i = 0; extralevels_sorted[i]; i++)
	{
		filelist_item_t	*item = extralevels_sorted[i];
		levelinfo_t		*extra = (levelinfo_t *) (item + 1);

		if (SDL_AtomicGet (&extralevels_cancel_parsing))
			return 1;

		if (!Mod_LoadMapDescription (buf, sizeof (buf), item->name))
			SDL_AtomicSet (&extra->type, MAPTYPE_BMODEL);
		SDL_AtomicSetPtr ((void **) &extra->message, buf[0] ? strdup (buf) : "");
	}

	return 0;
}

/*
==================
ExtraMaps_WaitForParsingThread
==================
*/
static void ExtraMaps_WaitForParsingThread (void)
{
	if (extralevels_parsing_thread)
	{
		SDL_WaitThread (extralevels_parsing_thread, NULL);
		extralevels_parsing_thread = NULL;
		SDL_AtomicSet (&extralevels_cancel_parsing, 0);
	}
}

/*
==================
ExtraMaps_Init
==================
*/
void ExtraMaps_Init (void)
{
	char			mapname[32];
	char			ignorepakdir[32];
	searchpath_t	*search;
	pack_t			*pak;
	int				i;

	// we don't want to list the maps in id1 pakfiles,
	// because these are not "add-on" levels
	q_snprintf (ignorepakdir, sizeof(ignorepakdir), "/%s/", GAMENAME);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (*search->filename) //directory
		{
			char		dir[MAX_OSPATH];
			findfile_t	*find;

			q_snprintf (dir, sizeof (dir), "%s/maps", search->filename);
			for (find = Sys_FindFirst (dir, "bsp"); find; find = Sys_FindNext (find))
			{
				if (find->attribs & FA_DIRECTORY)
					continue;
				COM_StripExtension (find->name, mapname, sizeof (mapname));
				ExtraMaps_Add (mapname, search);
			}
		}
		else //pakfile
		{
			qboolean isbase = (strstr(search->pack->filename, ignorepakdir) != NULL);
			for (i = 0, pak = search->pack; i < pak->numfiles; i++)
			{
				if (pak->files[i].filelen > 32*1024 &&				// don't list files under 32k (ammo boxes etc)
					!strncmp (pak->files[i].name, "maps/", 5) &&	// don't list files outside of maps/
					!strchr (pak->files[i].name + 5, '/') &&		// don't list files in subdirectories
					!strcmp (COM_FileGetExtension (pak->files[i].name), "bsp"))
				{
					COM_StripExtension (pak->files[i].name + 5, mapname, sizeof (mapname));
					ExtraMaps_Add (mapname, isbase ? NULL : search);
				}
			}
		}
	}

	ExtraMaps_Sort ();

	SDL_AtomicSet (&extralevels_cancel_parsing, 0);
	extralevels_parsing_thread = SDL_CreateThread (ExtraMaps_ParseDescriptions, "Map parser", NULL);
}

/*
==================
ExtraMaps_Clear
==================
*/
void ExtraMaps_Clear (void)
{
	filelist_item_t *item;

	SDL_AtomicSet (&extralevels_cancel_parsing, 1);
	ExtraMaps_WaitForParsingThread ();

	maxlevelnamelen = 0;
	for (item = extralevels; item; item = item->next)
	{
		levelinfo_t *extra = (levelinfo_t *) (item + 1);
		if (extra->message && *extra->message)
		{
			free ((void *)extra->message);
			extra->message = NULL;
		}
	}

	FileList_Clear(&extralevels);
}

/*
==================
ExtraMaps_ShutDown
==================
*/
void ExtraMaps_ShutDown (void)
{
	ExtraMaps_Clear ();
}

/*
==================
Host_Maps_f
==================
*/
static void Host_Maps_f (void)
{
	const char *types[] = {"map", "maps"};
	FileList_Print (extralevels, types, Cmd_Argc () >= 2 ? Cmd_Argv (1) : NULL);
}

//==============================================================================
//johnfitz -- modlist management
//==============================================================================

filelist_item_t	*modlist;

static void Modlist_Add (const char *name)
{
	FileList_Add(name, &modlist);
}

static qboolean Modlist_Check (const char *modname, const char *base)
{
	const char	*assetdirs[] = {"maps", "progs", "gfx", "sound"};
	char		modpath[MAX_OSPATH];
	char		itempath[MAX_OSPATH];
	findfile_t	*find;
	size_t		i;

	q_snprintf (modpath, sizeof (modpath), "%s/%s", base, modname);

	q_snprintf (itempath, sizeof (itempath), "%s/progs.dat", modpath);
	if (Sys_FileExists (itempath))
		return true;

	q_snprintf (itempath, sizeof (itempath), "%s/csprogs.dat", modpath);
	if (Sys_FileExists (itempath))
		return true;

	q_snprintf (itempath, sizeof (itempath), "%s/pak0.pak", modpath);
	if (Sys_FileExists (itempath))
		return true;

	for (i = 0; i < countof (assetdirs); i++)
	{
		q_snprintf (itempath, sizeof (itempath), "%s/%s", modpath, assetdirs[i]);
		for (find = Sys_FindFirst (itempath, NULL); find; find = Sys_FindNext (find))
		{
			if (!strcmp (find->name, ".") || !strcmp (find->name, ".."))
				continue;
			Sys_FindClose (find);
			return true;
		}
	}

	return false;
}

void Modlist_Init (void)
{
	const char	*basedirs[2];
	int			i, numbasedirs;
	findfile_t	*find;

	basedirs[0] = com_basedir;
	numbasedirs = 1;
	if (host_parms->userdir != host_parms->basedir)
		basedirs[numbasedirs++] = host_parms->userdir;

	for (i = 0; i < numbasedirs; i++)
	{
		for (find = Sys_FindFirst (basedirs[i], NULL); find; find = Sys_FindNext (find))
		{
			if (!(find->attribs & FA_DIRECTORY))
				continue;
			if (!strcmp (find->name, ".") || !strcmp (find->name, ".."))
				continue;
#ifndef _WIN32
			if (!q_strcasecmp (COM_FileGetExtension (find->name), "app")) // skip .app bundles on macOS
				continue;
#endif
			if (Modlist_Check (find->name, basedirs[i]))
				Modlist_Add (find->name);
		}
	}
}

//==============================================================================
//ericw -- demo list management
//==============================================================================

filelist_item_t	*demolist;

static void DemoList_Clear (void)
{
	FileList_Clear (&demolist);
}

void DemoList_Rebuild (void)
{
	DemoList_Clear ();
	DemoList_Init ();
}

// TODO: Factor out to a general-purpose file searching function
void DemoList_Init (void)
{
	char		demname[32];
	char		ignorepakdir[32];
	searchpath_t	*search;
	pack_t		*pak;
	int		i;

	// we don't want to list the demos in id1 pakfiles,
	// because these are not "add-on" demos
	q_snprintf (ignorepakdir, sizeof (ignorepakdir), "/%s/", GAMENAME);
	
	for (search = com_searchpaths; search; search = search->next)
	{
		if (*search->filename) //directory
		{
			findfile_t *find;
			for (find = Sys_FindFirst (search->filename, "dem"); find; find = Sys_FindNext (find))
			{
				if (find->attribs & FA_DIRECTORY)
					continue;
				COM_StripExtension (find->name, demname, sizeof (demname));
				FileList_Add (demname, &demolist);
			}
		}
		else //pakfile
		{
			if (!strstr(search->pack->filename, ignorepakdir))
			{ //don't list standard id demos
				for (i = 0, pak = search->pack; i < pak->numfiles; i++)
				{
					if (!strcmp (COM_FileGetExtension (pak->files[i].name), "dem"))
					{
						COM_StripExtension (pak->files[i].name, demname, sizeof (demname));
						FileList_Add (demname, &demolist);
					}
				}
			}
		}
	}
}

//==============================================================================
//save list management
//==============================================================================

filelist_item_t	*savelist;

static void SaveList_Clear (void)
{
	FileList_Clear (&savelist);
}

void SaveList_Rebuild (void)
{
	SaveList_Clear ();
	SaveList_Init ();
}

void SaveList_Init (void)
{
	char		savename[32];
	findfile_t *find;

	for (find = Sys_FindFirst (com_gamedir, "sav"); find; find = Sys_FindNext (find))
	{
		if (find->attribs & FA_DIRECTORY)
			continue;
		COM_StripExtension (find->name, savename, sizeof (savename));
		FileList_Add (savename, &savelist);
	}
}

//==============================================================================
//sky list management
//==============================================================================

filelist_item_t	*skylist;

static void SkyList_Clear (void)
{
	FileList_Clear (&skylist);
}

void SkyList_Rebuild (void)
{
	SkyList_Clear ();
	SkyList_Init ();
}

static qboolean SkyList_AddFile (const char *path)
{
	const char	prefix[] = "gfx/env/";
	const char	suffix[] = "up";
	const char	*ext;
	char		skyname[MAX_QPATH];
	size_t		len;

	ext = COM_FileGetExtension (path);
	if (q_strcasecmp (ext, "tga") != 0)
		return false;

	COM_StripExtension (path, skyname, sizeof (skyname));
	len = strlen (skyname);
	if (len < sizeof (suffix) - 1)
		return false;
	len -= sizeof (suffix) - 1;
	if (q_strcasecmp (skyname + len, suffix) != 0)
		return false;
	skyname[len] = '\0';

	SDL_assert (len > sizeof (prefix) - 1);
	SDL_assert (!q_strncasecmp (skyname, prefix, sizeof (prefix) - 1));
	if (len <= sizeof (prefix) - 1)
		return false;

	FileList_Add (skyname + (sizeof (prefix) - 1), &skylist);

	return true;
}

static void SkyList_AddDirRec (const char *root, const char *relpath)
{
	findfile_t	*find;
	char		child[MAX_OSPATH];
	char		fullpath[MAX_OSPATH];

	q_snprintf (fullpath, sizeof (fullpath), "%s/%s", root, relpath);
	for (find = Sys_FindFirst (fullpath, NULL); find; find = Sys_FindNext (find))
	{
		q_snprintf (child, sizeof (child), "%s/%s", relpath, find->name);
		if (find->attribs & FA_DIRECTORY)
		{
			if (find->name[0] == '.')
				continue;
			SkyList_AddDirRec (root, child);
			continue;
		}
		SkyList_AddFile (child);
	}
}

void SkyList_Init (void)
{
	searchpath_t	*search;
	pack_t			*pak;
	int				i;

	for (search = com_searchpaths; search; search = search->next)
	{
		if (*search->filename) //directory
			SkyList_AddDirRec (search->filename, "gfx/env");
		else //pakfile
			for (i = 0, pak = search->pack; i < pak->numfiles; i++)
				SkyList_AddFile (pak->files[i].name);
	}
}

/*
==================
Host_Skies_f

list all potential skies
==================
*/
static void Host_Skies_f (void)
{
	const char *types[] = {"sky", "skies"};
	FileList_Print (skylist, types, Cmd_Argc () >= 2 ? Cmd_Argv (1) : NULL);
}

/*
==================
Host_Mods_f -- johnfitz

list all potential mod directories (contain either a pak file or a progs.dat)
==================
*/
static void Host_Mods_f (void)
{
	const char *types[] = {"mod", "mods"};
	FileList_Print (modlist, types, Cmd_Argc () >= 2 ? Cmd_Argv (1) : NULL);
}

//==============================================================================

/*
=============
Host_Mapname_f -- johnfitz
=============
*/
static void Host_Mapname_f (void)
{
	if (sv.active)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", sv.name);
		return;
	}

	if (cls.state == ca_connected)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", cl.mapname);
		return;
	}

	Con_Printf ("no map loaded\n");
}

/*
==================
Host_Status_f
==================
*/
static void Host_Status_f (void)
{
	void	(*print_fn) (const char *fmt, ...)
				 FUNCP_PRINTF(1,2);
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hours = 0;
	int			j;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print_fn = Con_Printf;
	}
	else
		print_fn = SV_ClientPrintf;

	print_fn ("host:    %s\n", Cvar_VariableString ("hostname"));
	print_fn ("version: %4.2f\n", VERSION);
	if (tcpipAvailable)
		print_fn ("tcp/ip:  %s\n", my_tcpip_address);
	if (ipxAvailable)
		print_fn ("ipx:     %s\n", my_ipx_address);
	print_fn ("map:     %s\n", sv.name);
	print_fn ("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(net_time - NET_QSocketGetTime(client->netconnection));
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print_fn ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print_fn ("   %s\n", NET_QSocketGetAddressString(client->netconnection));
	}
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
static void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set god mode to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
		if (!((int)sv_player->v.flags & FL_GODMODE) )
			SV_ClientPrintf ("godmode OFF\n");
		else
			SV_ClientPrintf ("godmode ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_GODMODE;
			SV_ClientPrintf ("godmode ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_GODMODE;
			SV_ClientPrintf ("godmode OFF\n");
		}
		break;
	default:
		Con_Printf("god [value] : toggle god mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Notarget_f
==================
*/
static void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set notarget to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
		if (!((int)sv_player->v.flags & FL_NOTARGET) )
			SV_ClientPrintf ("notarget OFF\n");
		else
			SV_ClientPrintf ("notarget ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_NOTARGET;
			SV_ClientPrintf ("notarget ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_NOTARGET;
			SV_ClientPrintf ("notarget OFF\n");
		}
		break;
	default:
		Con_Printf("notarget [value] : toggle notarget mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

qboolean noclip_anglehack;

/*
==================
Host_Noclip_f
==================
*/
static void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_NOCLIP)
		{
			noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	default:
		Con_Printf("noclip [value] : toggle noclip mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
====================
Host_SetPos_f

adapted from fteqw, originally by Alex Shadowalker
====================
*/
static void Host_SetPos_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	if (Cmd_Argc() != 7 && Cmd_Argc() != 4)
	{
		SV_ClientPrintf("usage:\n");
		SV_ClientPrintf("   setpos <x> <y> <z>\n");
		SV_ClientPrintf("   setpos <x> <y> <z> <pitch> <yaw> <roll>\n");
		SV_ClientPrintf("current values:\n");
		SV_ClientPrintf("   %i %i %i %i %i %i\n",
			(int)sv_player->v.origin[0],
			(int)sv_player->v.origin[1],
			(int)sv_player->v.origin[2],
			(int)sv_player->v.v_angle[0],
			(int)sv_player->v.v_angle[1],
			(int)sv_player->v.v_angle[2]);
		return;
	}

	if (sv_player->v.movetype != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf ("noclip ON\n");
	}

	//make sure they're not going to whizz away from it
	sv_player->v.velocity[0] = 0;
	sv_player->v.velocity[1] = 0;
	sv_player->v.velocity[2] = 0;
	
	sv_player->v.origin[0] = atof(Cmd_Argv(1));
	sv_player->v.origin[1] = atof(Cmd_Argv(2));
	sv_player->v.origin[2] = atof(Cmd_Argv(3));
	
	if (Cmd_Argc() == 7)
	{
		sv_player->v.angles[0] = atof(Cmd_Argv(4));
		sv_player->v.angles[1] = atof(Cmd_Argv(5));
		sv_player->v.angles[2] = atof(Cmd_Argv(6));
		sv_player->v.fixangle = 1;
	}
	
	SV_LinkEdict (sv_player, false);
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
static void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	default:
		Con_Printf("fly [value] : toggle fly mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Ping_f

==================
*/
static void Host_Ping_f (void)
{
	int		i, j;
	float		total;
	client_t	*client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total+=client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int)(total*1000), client->name);
	}
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/

/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
static void Host_Map_f (void)
{
	int		i;
	char	name[MAX_QPATH], *p;

	if (Cmd_Argc() < 2)	//no map name given
	{
		if (cls.state == ca_dedicated)
		{
			if (sv.active)
				Con_Printf ("Current map: %s\n", sv.name);
			else
				Con_Printf ("Server not active\n");
		}
		else if (cls.state == ca_connected)
		{
			Con_Printf ("Current map: %s ( %s )\n", cl.levelname, cl.mapname);
		}
		else
		{
			Con_Printf ("map <levelname>: start a new server\n");
		}
		return;
	}

	if (cmd_source != src_command)
		return;

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer(false);

	if (cls.state != ca_dedicated)
		IN_Activate();
	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque ();

	svs.serverflags = 0;			// haven't completed an episode yet
	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	// remove (any) trailing ".bsp" from mapname -- S.A.
	p = strstr(name, ".bsp");
	if (p && p[4] == '\0')
		*p = '\0';
	PR_SwitchQCVM(&sv.qcvm);
	SV_SpawnServer (name);
	PR_SwitchQCVM(NULL);
	if (!sv.active)
		return;

	if (cls.state != ca_dedicated)
	{
		memset (cls.spawnparms, 0, MAX_MAPSTRING);
		for (i = 2; i < Cmd_Argc(); i++)
		{
			q_strlcat (cls.spawnparms, Cmd_Argv(i), MAX_MAPSTRING);
			q_strlcat (cls.spawnparms, " ", MAX_MAPSTRING);
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
======================
Host_Randmap_f

Loads a random map from the "maps" list.
======================
*/
static void Host_Randmap_f (void)
{
	int	i, randlevel, numlevels;
	filelist_item_t	*level;

	if (cmd_source != src_command)
		return;

	for (level = extralevels, numlevels = 0; level; level = level->next)
		numlevels++;

	if (numlevels == 0)
	{
		Con_Printf ("no maps\n");
		return;
	}

	randlevel = (rand() % numlevels);

	for (level = extralevels, i = 0; level; level = level->next, i++)
	{
		if (i == randlevel)
		{
			Con_Printf ("Starting map %s...\n", level->name);
			Cbuf_AddText (va("map %s\n", level->name));
			return;
		}
	}
}

/*
==================
Host_AutoLoad
==================
*/
static qboolean Host_AutoLoad (void)
{
	if (!sv_autoload.value || !sv.lastsave[0] || svs.maxclients != 1)
		return false;

	if (sv_autoload.value < 2.f &&
		!SCR_ModalMessage ("Load last save? (y/n)", 0.f))
	{
		sv.lastsave[0] = '\0';
		return false;
	}

	sv.autoloading = true;
	Con_Printf ("Autoloading...\n");
	Cbuf_AddText (va ("load %s\n", sv.lastsave));
	Cbuf_Execute ();

	if (sv.autoloading)
	{
		sv.autoloading = false;
		Con_Printf ("Autoload failed!\n");
		return false;
	}

	return true;
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
static void Host_Changelevel_f (void)
{
	char	level[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf ("Only the server may changelevel\n");
		return;
	}

	//johnfitz -- check for client having map before anything else
	q_snprintf (level, sizeof(level), "maps/%s.bsp", Cmd_Argv(1));
	if (!COM_FileExists(level, NULL))
		Host_Error ("cannot find map %s", level);
	//johnfitz

	q_strlcpy (level, Cmd_Argv(1), sizeof(level));
	if (!strcmp (sv.name, level) && Host_AutoLoad ())
		return;

	if (cls.state != ca_dedicated)
		IN_Activate();	// -- S.A.
	key_dest = key_game;	// remove console or menu
	PR_SwitchQCVM(&sv.qcvm);
	SV_SaveSpawnparms ();
	SV_SpawnServer (level);
	PR_SwitchQCVM(NULL);
	// also issue an error if spawn failed -- O.S.
	if (!sv.active)
		Host_Error ("cannot run map %s", level);
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
static void Host_Restart_f (void)
{
	char	mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;

	if (Host_AutoLoad ())
		return;

	q_strlcpy (mapname, sv.name, sizeof(mapname));	// mapname gets cleared in spawnserver
	PR_SwitchQCVM(&sv.qcvm);
	SV_SpawnServer (mapname);
	PR_SwitchQCVM(NULL);
	if (!sv.active)
		Host_Error ("cannot restart map %s", mapname);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
static void Host_Reconnect_f (void)
{
	if (cls.demoplayback)	// cross-map demo playback fix from Baker
		return;

	SCR_BeginLoadingPlaque ();
	CL_ClearSignons ();		// need new connection messages
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
static void Host_Connect_f (void)
{
	char	name[MAX_QPATH];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
static void Host_SavegameComment (char *text)
{
	int		i;
	char	*levelname;
	char	kills[20];
	char	*p1, *p2;

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		text[i] = ' ';

// Remove CR/LFs from level name to avoid broken saves, e.g. with autumn_sp map:
// https://celephais.net/board/view_thread.php?id=60452&start=3666
	levelname = cl.levelname[0] ? cl.levelname : cl.mapname;
	p1 = strchr(levelname, '\n');
	p2 = strchr(levelname, '\r');
	if (p1 != NULL) *p1 = 0;
	if (p2 != NULL) *p2 = 0;

	i = (int) strlen(levelname);
	if (i > 22) i = 22;
	memcpy (text, levelname, (size_t)i);
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
	}
	if (p1 != NULL) *p1 = '\n';
	if (p2 != NULL) *p2 = '\r';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}

static void Host_InvalidateSave (const char *relname)
{
	if (!strcmp (sv.lastsave, relname))
		sv.lastsave[0] = '\0';
}

/*
===============
Host_Savegame_f
===============
*/
static void Host_Savegame_f (void)
{
	char	relname[MAX_OSPATH];
	char	name[MAX_OSPATH];
	FILE	*f;
	int	i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (sv.nomonsters)
	{
		Con_Printf ("Can't save when using \"nomonsters\".\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	q_strlcpy (relname, Cmd_Argv(1), sizeof(relname));
	COM_AddExtension (relname, ".sav", sizeof(relname));
	Con_Printf ("Saving game to %s...\n", relname);

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, relname);

	f = Sys_fopen (name, "w");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	PR_SwitchQCVM(&sv.qcvm);

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%f\n",qcvm->time);

// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}

	ED_WriteGlobals (f);
	for (i = 0; i < qcvm->num_edicts; i++)
	{
		ED_Write (f, EDICT_NUM(i));
		fflush (f);
	}
	fclose (f);
	Con_Printf ("done.\n");
	PR_SwitchQCVM(NULL);

	SaveList_Rebuild ();

	q_strlcpy (sv.lastsave, relname, sizeof (sv.lastsave));
}

/*
===============
Host_Loadgame_f
===============
*/
static void Host_Loadgame_f (void)
{
	static char	*start;
	
	char	name[MAX_OSPATH];
	char	relname[MAX_OSPATH];
	char	mapname[MAX_QPATH];
	float	time, tfloat;
	const char	*data;
	int	i;
	edict_t	*ent;
	int	entnum;
	int	version;
	float	spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("load <savename> : load a game\n");
		return;
	}
	
	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (nomonsters.value)
	{
		Con_Warning ("\"%s\" disabled automatically.\n", nomonsters.name);
		Cvar_SetValueQuick (&nomonsters, 0.f);
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	q_strlcpy (relname, Cmd_Argv(1), sizeof(relname));
	COM_AddExtension (relname, ".sav", sizeof(relname));

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, relname);
	if (!Sys_FileExists (name))
	{
		Con_Printf ("ERROR: %s not found.\n", relname);
		Host_InvalidateSave (relname);
		return;
	}

	Con_Printf ("Loading game from %s...\n", relname);

	SCR_BeginLoadingPlaque ();

// avoid leaking if the previous Host_Loadgame_f failed with a Host_Error
	if (start != NULL)
		free (start);
	
	start = (char *) COM_LoadMallocFile_TextMode_OSPath(name, NULL);
	if (start == NULL)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		Host_InvalidateSave (relname);
		SCR_EndLoadingPlaque ();
		return;
	}

	data = start;
	data = COM_ParseIntNewline (data, &version);
	if (version != SAVEGAME_VERSION)
	{
		free (start);
		start = NULL;
		if (sv.autoloading)
			Con_Printf ("ERROR: Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		else
			Host_Error ("Savegame is version %i, not %i", version, SAVEGAME_VERSION);
		Host_InvalidateSave (relname);
		SCR_EndLoadingPlaque ();
		return;
	}
	data = COM_ParseStringNewline (data);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		data = COM_ParseFloatNewline (data, &spawn_parms[i]);
// this silliness is so we can load 1.06 save files, which have float skill values
	data = COM_ParseFloatNewline(data, &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue ("skill", (float)current_skill);

	data = COM_ParseStringNewline (data);
	q_strlcpy (mapname, com_token, sizeof(mapname));
	data = COM_ParseFloatNewline (data, &time);

	CL_Disconnect_f ();

	PR_SwitchQCVM(&sv.qcvm);
	SV_SpawnServer (mapname);

	if (!sv.active)
	{
		PR_SwitchQCVM(NULL);
		free (start);
		start = NULL;
		SCR_EndLoadingPlaque ();
		Con_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		data = COM_ParseStringNewline (data);
		sv.lightstyles[i] = (const char *)Hunk_Strdup (com_token, "lightstyles");
	}

// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (*data)
	{
		data = COM_Parse (data);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token,"{"))
		{
			Host_Error ("First token isn't a brace");
		}

		if (entnum == -1)
		{	// parse the global vars
			data = ED_ParseGlobals (data);
		}
		else
		{	// parse an edict
			ent = EDICT_NUM(entnum);
			if (entnum < qcvm->num_edicts)
			{
				ED_ClearEdict (ent);
			}
			else
			{
				memset (ent, 0, qcvm->edict_size);
				ent->baseline.scale = ENTSCALE_DEFAULT;
			}
			data = ED_ParseEdict (data, ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	qcvm->num_edicts = entnum;
	qcvm->time = time;

	free (start);
	start = NULL;

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	PR_SwitchQCVM(NULL);

	q_strlcpy (sv.lastsave, relname, sizeof (sv.lastsave));

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
static void Host_Name_f (void)
{
	char	newName[32];

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}
	if (Cmd_Argc () == 2)
		q_strlcpy(newName, Cmd_Argv(1), sizeof(newName));
	else
		q_strlcpy(newName, Cmd_Args(), sizeof(newName));
	newName[15] = 0;	// client_t structure actually says name[32].

	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newName) == 0)
			return;
		Cvar_Set ("_cl_name", newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected") )
	{
		if (Q_strcmp(host_client->name, newName) != 0)
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);
	}
	Q_strcpy (host_client->name, newName);
	host_client->edict->v.netname = PR_SetEngineString(host_client->name);

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

static void Host_Say(qboolean teamonly)
{
	int		j;
	client_t	*client;
	client_t	*save;
	const char	*p;
	char		text[MAXCMDLINE], *p2;
	qboolean	quoted;
	qboolean	fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state != ca_dedicated)
		{
			Cmd_ForwardToServer ();
			return;
		}
		fromServer = true;
		teamonly = false;
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = Cmd_Args();
// remove quotes if present
	quoted = false;
	if (*p == '\"')
	{
		p++;
		quoted = true;
	}
// turn on color set 1
	if (!fromServer)
		q_snprintf (text, sizeof(text), "\001%s: %s", save->name, p);
	else
		q_snprintf (text, sizeof(text), "\001<%s> %s", hostname.string, p);

// check length & truncate if necessary
	j = (int) strlen(text);
	if (j >= (int) sizeof(text) - 1)
	{
		text[sizeof(text) - 2] = '\n';
		text[sizeof(text) - 1] = '\0';
	}
	else
	{
		p2 = text + j;
		while ((const char *)p2 > (const char *)text &&
			(p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)) )
		{
			if (p2[-1] == '\"' && quoted)
				quoted = false;
			p2[-1] = '\0';
			p2--;
		}
		p2[0] = '\n';
		p2[1] = '\0';
	}

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
	}
	host_client = save;

	if (cls.state == ca_dedicated)
		Sys_Printf("%s", &text[1]);
}

static void Host_Say_f(void)
{
	Host_Say(false);
}

static void Host_Say_Team_f(void)
{
	Host_Say(true);
}

static void Host_Tell_f(void)
{
	int		j;
	client_t	*client;
	client_t	*save;
	const char	*p;
	char		text[MAXCMDLINE], *p2;
	qboolean	quoted;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	p = Cmd_Args();
// remove quotes if present
	quoted = false;
	if (*p == '\"')
	{
		p++;
		quoted = true;
	}
	q_snprintf (text, sizeof(text), "%s: %s", host_client->name, p);

// check length & truncate if necessary
	j = (int) strlen(text);
	if (j >= (int) sizeof(text) - 1)
	{
		text[sizeof(text) - 2] = '\n';
		text[sizeof(text) - 1] = '\0';
	}
	else
	{
		p2 = text + j;
		while ((const char *)p2 > (const char *)text &&
			(p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)) )
		{
			if (p2[-1] == '\"' && quoted)
				quoted = false;
			p2[-1] = '\0';
			p2--;
		}
		p2[0] = '\n';
		p2[1] = '\0';
	}

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned)
			continue;
		if (q_strcasecmp(client->name, Cmd_Argv(1)))
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
		break;
	}
	host_client = save;
}

/*
==================
Host_Color_f
==================
*/
static void Host_Color_f(void)
{
	int		top, bottom;
	int		playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_color", playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
static void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf ("Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = qcvm->time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}

/*
==================
Host_Pause_f
==================
*/
static void Host_Pause_f (void)
{
//ericw -- demo pause support (inspired by MarkV)
	if (cls.demoplayback)
	{
		cls.demopaused = !cls.demopaused;
		cl.paused = cls.demopaused;
		return;
	}

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (!pausable.value)
		SV_ClientPrintf ("Pause not allowed.\n");
	else
	{
		sv.paused ^= 1;

		if (sv.paused)
		{
			SV_BroadcastPrintf ("%s paused the game\n", PR_GetString(sv_player->v.netname));
		}
		else
		{
			SV_BroadcastPrintf ("%s unpaused the game\n",PR_GetString(sv_player->v.netname));
		}

	// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================

/*
==================
Host_PreSpawn_f
==================
*/
static void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("prespawn not valid -- already spawned\n");
		return;
	}

	host_client->sendsignon = PRESPAWN_SIGNONBUFS;
	host_client->signonidx = 0;
}

/*
==================
Host_Spawn_f
==================
*/
static void Host_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (cmd_source == src_command)
	{
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("Spawn not valid -- already spawned\n");
		return;
	}

// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.paused = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, qcvm->progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = PR_SetEngineString(host_client->name);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];
		// call the spawn function
		pr_global_struct->time = qcvm->time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - NET_QSocketGetTime(host_client->netconnection)) <= qcvm->time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer);
	}

// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, qcvm->time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char)i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

//
// send some stats
//
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);

//
// send a fixangle
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM( 1 + (host_client - svs.clients) );
	MSG_WriteByte (&host_client->message, svc_setangle);
	for (i = 0; i < 2; i++)
		if (sv.loadgame)
			MSG_WriteAngle (&host_client->message, ent->v.v_angle[i], sv.protocolflags );
		else
			MSG_WriteAngle (&host_client->message, ent->v.angles[i], sv.protocolflags );
	MSG_WriteAngle (&host_client->message, 0, sv.protocolflags );

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
static void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================

/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
static void Host_Kick_f (void)
{
	const char	*who;
	const char	*message = NULL;
	client_t	*save;
	int		i;
	qboolean	byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
	}
	else if (pr_global_struct->deathmatch)
		return;

	save = host_client;

	if (Cmd_Argc() > 2 && Q_strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = Q_atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (q_strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;			// skip the #
				while (*message == ' ')		// skip white space
					message++;
				message += strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
static void Host_Give_f (void)
{
	const char	*t;
	int	v;
	eval_t	*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (hipnotic)
		{
		    if (t[0] == '6')
		    {
			if (t[1] == 'a')
			    sv_player->v.items = (int)sv_player->v.items | HIT_PROXIMITY_GUN;
			else
			    sv_player->v.items = (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
		    }
		    else if (t[0] == '9')
			sv_player->v.items = (int)sv_player->v.items | HIT_LASER_CANNON;
		    else if (t[0] == '0')
			sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
		    else if (t[0] >= '2')
			sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
		    if (t[0] >= '2')
			sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_shells1");
		    if (val)
			val->_float = v;
		}
		sv_player->v.ammo_shells = v;
		break;

	case 'n':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_nails1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_nails = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_nails = v;
		}
		break;

	case 'l':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_lava_nails");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_nails = v;
		    }
		}
		break;

	case 'r':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_rockets1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_rockets = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_rockets = v;
		}
		break;

	case 'm':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_multi_rockets");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_rockets = v;
		    }
		}
		break;

	case 'h':
		sv_player->v.health = v;
		break;

	case 'c':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_cells1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_cells = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_cells = v;
		}
		break;

	case 'p':
		if (rogue)
		{
		    val = GetEdictFieldValueByName(sv_player, "ammo_plasma");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_cells = v;
		    }
		}
		break;

	//johnfitz -- give armour
	case 'a':
		if (v > 150)
		{
		    sv_player->v.armortype = 0.8;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR3;
		}
		else if (v > 100)
		{
		    sv_player->v.armortype = 0.6;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR2;
		}
		else if (v >= 0)
		{
		    sv_player->v.armortype = 0.3;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR1;
		}
		break;
		//johnfitz
	}

	//johnfitz -- update currentammo to match new ammo (so statusbar updates correctly)
	switch ((int)(sv_player->v.weapon))
	{
	case IT_SHOTGUN:
	case IT_SUPER_SHOTGUN:
		sv_player->v.currentammo = sv_player->v.ammo_shells;
		break;
	case IT_NAILGUN:
	case IT_SUPER_NAILGUN:
	case RIT_LAVA_SUPER_NAILGUN:
		sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case IT_GRENADE_LAUNCHER:
	case IT_ROCKET_LAUNCHER:
	case RIT_MULTI_GRENADE:
	case RIT_MULTI_ROCKET:
		sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	case IT_LIGHTNING:
	case HIT_LASER_CANNON:
	case HIT_MJOLNIR:
		sv_player->v.currentammo = sv_player->v.ammo_cells;
		break;
	case RIT_LAVA_NAILGUN: //same as IT_AXE
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case RIT_PLASMA_GUN: //same as HIT_PROXIMITY_GUN
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_cells;
		if (hipnotic)
			sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	}
	//johnfitz
}

static edict_t	*FindViewthing (void)
{
	int		i;
	edict_t	*e = NULL;

	PR_SwitchQCVM(&sv.qcvm);
	for (i=0 ; i<qcvm->num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if ( !strcmp (PR_GetString(e->v.classname), "viewthing") )
			break;
	}

	if (i == qcvm->num_edicts)
	{
		e = NULL;
		Con_Printf ("No viewthing on map\n");
	}

	PR_SwitchQCVM(NULL);
	return e;
}

/*
==================
Host_Viewmodel_f
==================
*/
static void Host_Viewmodel_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf ("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	PR_SwitchQCVM(&sv.qcvm);
	e->v.frame = 0;
	cl.model_precache[(int)e->v.modelindex] = m;
	PR_SwitchQCVM(NULL);
}

/*
==================
Host_Viewframe_f
==================
*/
static void Host_Viewframe_f (void)
{
	edict_t	*e;
	int		f;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes - 1;

	e->v.frame = f;
}

static void PrintFrameName (qmodel_t *m, int frame)
{
	aliashdr_t 			*hdr;
	maliasframedesc_t	*pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata (m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
static void Host_Viewnext_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName (m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
static void Host_Viewprev_f (void)
{
	edict_t	*e;
	qmodel_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName (m, e->v.frame);
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/

/*
==================
Host_Startdemos_f
==================
*/
static void Host_Startdemos_f (void)
{
	int		i, c;

	if (cls.state == ca_dedicated)
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		q_strlcpy (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0]));

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		Cbuf_InsertText ("menu_main\n");
		if (!fitzmode && !cl_startdemos.value)
		{  /* QuakeSpasm customization: */
			/* go straight to menu, no CL_NextDemo */
			cls.demonum = -1;
			return;
		}
		CL_NextDemo ();
	}
	else
	{
		cls.demonum = -1;
	}
}

/*
==================
Host_Demos_f

Return to looping demos
==================
*/
static void Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
static void Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (!cls.demoplayback)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}

/*
==================
Host_Resetdemos

Clear looping demo list (called on game change)
==================
*/
void Host_Resetdemos (void)
{
	memset (cls.demos, 0, sizeof (cls.demos));
	cls.demonum = 0;
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	Cmd_AddCommand ("maps", Host_Maps_f); //johnfitz
	Cmd_AddCommand ("mods", Host_Mods_f); //johnfitz
	Cmd_AddCommand ("games", Host_Mods_f); // as an alias to "mods" -- S.A. / QuakeSpasm
	Cmd_AddCommand ("skies", Host_Skies_f); //ericw
	Cmd_AddCommand ("mapname", Host_Mapname_f); //johnfitz
	Cmd_AddCommand ("randmap", Host_Randmap_f); //ericw

	Cmd_AddCommand_ClientCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand_ClientCommand ("god", Host_God_f);
	Cmd_AddCommand_ClientCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand_ClientCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand_Console ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand_ClientCommand ("name", Host_Name_f);
	Cmd_AddCommand_ClientCommand ("noclip", Host_Noclip_f);
	Cmd_AddCommand_ClientCommand ("setpos", Host_SetPos_f); //QuakeSpasm

	Cmd_AddCommand_ClientCommand ("say", Host_Say_f);
	Cmd_AddCommand_ClientCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand_ClientCommand ("tell", Host_Tell_f);
	Cmd_AddCommand_ClientCommand ("color", Host_Color_f);
	Cmd_AddCommand_ClientCommand ("kill", Host_Kill_f);
	Cmd_AddCommand_ClientCommand ("pause", Host_Pause_f);
	Cmd_AddCommand_ClientCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand_ClientCommand ("begin", Host_Begin_f);
	Cmd_AddCommand_ClientCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand_ClientCommand ("kick", Host_Kick_f);
	Cmd_AddCommand_ClientCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand_ClientCommand ("give", Host_Give_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);
}

