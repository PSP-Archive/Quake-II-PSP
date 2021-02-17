/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "qcommon.h"

// define this to dissalow any data but the demo pak file
//#define	NO_ADDONS

// if a packfile directory differs from this, it is assumed to be hacked
// Full version
#define	PAK0_CHECKSUM	0x40e614e0
// Demo
//#define	PAK0_CHECKSUM	0xb2c6d7ea
// OEM
//#define	PAK0_CHECKSUM	0x78e135c

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/


//
// in memory
//

typedef struct {
	const char* name;
	int filepos, filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	int numfiles;
	packfile_t* files;
	char* string;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
//cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;

typedef struct filelink_s
{
	struct filelink_s	*next;
	char	*from;
	int		fromlength;
	char	*to;
} filelink_t;

filelink_t	*fs_links;

typedef struct searchpath_s {
	char filename[MAX_OSPATH];
	pack_t* pack;
	struct searchpath_s *next;
} searchpath_t;

searchpath_t* fs_searchpaths;
searchpath_t* fs_base_searchpaths;	// without gamedirs


/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/


/*
================
FS_filelength
================
*/
int FS_filelength(FILE *f) {
	int pos = ftell(f);
	fseek(f, 0, SEEK_END);
	int end = ftell(f);
	fseek(f, pos, SEEK_SET);
	return end;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath(char* path) {
//printf("TRACE: %s(%s)\n", __FUNCTION__, path);

	unsigned int off = 1;

	if(strlen(path) >= 5
	&& path[0] == 'm'
	&& path[1] == 's'
	&& path[2] == '0'
	&& path[3] == ':'
	&& path[4] == '/') {
		off = 5;
	}

	char* pos;
	for(pos = path+off; *pos; pos++) {
		if(*pos == '/') {
			*pos = '\0';
			Sys_Mkdir(path);
			*pos = '/';
		}
	}
}

/*
==============
FS_FCloseFile

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile(FILE *f) {
	fclose(f);
}

// RAFAEL
/*
	Developer_searchpath
*/
int	Developer_searchpath(int who) {
	int		ch;
	// PMM - warning removal
//	char	*start;
	searchpath_t	*search;
	
	if (who == 1) // xatrix
		ch = 'x';
	else if (who == 2)
		ch = 'r';

	for (search = fs_searchpaths ; search ; search = search->next) {
		if (strstr (search->filename, "xatrix"))
			return 1;
		if (strstr (search->filename, "rogue"))
			return 2;
	}
	return (0);

}

/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;

int FS_FOpenFile(char* filename, FILE** file) {
	file_from_pak = 0;

	searchpath_t* search;
	for(search = fs_searchpaths; search; search = search->next) {
		char netpath[MAX_OSPATH];			
		Com_sprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);
		*file = fopen(netpath, "rb");
		if(*file) {
			return FS_filelength(*file);
		}
	}

	for(search = fs_searchpaths; search; search = search->next) {
		if(search->pack) {
			pack_t* pak = search->pack;
			int i;
			for(i = 0; i < pak->numfiles; i++) {
				if(!Q_strcasecmp (pak->files[i].name, filename)) {
					file_from_pak = 1;
	
					*file = fopen(pak->filename, "rb");
					if(!*file) {
						Com_Error(ERR_FATAL, "Couldn't reopen %s", pak->filename);	
					}

					fseek(*file, pak->files[i].filepos, SEEK_SET);
					return pak->files[i].filelen;
				}
			}
		}	
	}
	
	*file = 0;
	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
#define	MAX_READ	0x10000		// read in blocks of 64k

void FS_Read(void *buffer, int len, FILE *f) {
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;
	tries = 0;
	while(remaining) {
		block = remaining;
		if(block > MAX_READ) {
			block = MAX_READ;
		}
		read = fread(buf, 1, block, f);
		if(read == 0) {
			Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
		}

		if(read == -1) {
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
		}

		remaining -= read;
		buf += read;
	}
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (char *path, void **buffer)
{
	FILE	*h;
	byte	*buf;
	int		len;

	buf = NULL;	// quiet compiler warning

// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &h);
	if (!h)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}
	
	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = Z_Malloc(len);
	*buffer = buf;

	FS_Read (buf, len, h);

	fclose (h);

	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile(void *buffer) {
	Z_Free (buffer);
}

/*
========================================
New File Stuff for PSP
========================================
*/
typedef struct fs_fdesc_s {
	FILE* file;
	packfile_t* pakfile;
} fs_fdesc_t;

#define MAX_OPEN_FILES 8

static fs_fdesc_t fs_openfiles[MAX_OPEN_FILES];

int FS_Fopen(char* name) {
	int fileid;
	for(fileid = 0; fileid < MAX_OPEN_FILES; fileid++) {
		if(!fs_openfiles[fileid].file) {
			break;
		}
	}
	if(fileid == MAX_OPEN_FILES) {
		Sys_Error("Maximum limit of open files hit\n");
		return 0;
	}

	fs_fdesc_t* fdesc = &fs_openfiles[fileid];
	memset(fdesc, 0, sizeof(fs_fdesc_t));
	fileid++;

	searchpath_t* search;
	for(search = fs_searchpaths; search; search = search->next) {
		char fullpath[MAX_OSPATH];			
		Com_sprintf(fullpath, sizeof(fullpath), "%s/%s", search->filename, name);
		fdesc->file = fopen(fullpath, "r");
		if(fdesc->file) {
			fdesc->pakfile = 0;
			return fileid;
		}
	}

	for(search = fs_searchpaths; search; search = search->next) {
		if(search->pack) {
			pack_t* pak = search->pack;
			int i;
			for(i = 0; i < pak->numfiles; i++) {
				if(!Q_strcasecmp(pak->files[i].name, name)) {
					fdesc->file = fopen(pak->filename, "r");
					if(!fdesc->file) {
						Com_Error(ERR_FATAL, "Pak file not found %s", pak->filename);
						return 0;
					}
					fdesc->pakfile = &pak->files[i];
					fseek(fdesc->file, fdesc->pakfile->filepos, SEEK_SET);
					return fileid;
				}
			}
		}	
	}

	return 0;
}

void FS_Fclose(int fileid) {
	fileid--;
	if(fileid < 0 && fileid >= MAX_OPEN_FILES) {
		Sys_Error("Wrong fileid\n");
		return;
	}
	fs_fdesc_t* fdesc = &fs_openfiles[fileid];
	if(!fdesc->file) {
		Sys_Error("No file\n");
		return;
	}

	if(fdesc->file) {
		fclose(fdesc->file);
	}
	memset(fdesc, 0, sizeof(fs_fdesc_t));
}

int FS_Fseek(int fileid, int pos, int func) {
	fileid--;
	if(fileid < 0 && fileid >= MAX_OPEN_FILES) {
		Sys_Error("Wrong fileid\n");
		return 0;
	}
	fs_fdesc_t* fdesc = &fs_openfiles[fileid];
	if(!fdesc->file) {
		Sys_Error("No file\n");
		return 0;
	}

	if(!fdesc->pakfile) {
		return fseek(fdesc->file, pos, func);
	} else {
		if(func == SEEK_CUR) {
			return fseek(fdesc->file, pos, SEEK_CUR);
		}
		if(func == SEEK_SET) {
			return fseek(fdesc->file, fdesc->pakfile->filepos + pos, SEEK_SET);
		}
		if(func == SEEK_END) {
			return fseek(fdesc->file, fdesc->pakfile->filepos + fdesc->pakfile->filelen + pos, SEEK_SET);
		}
		Sys_Error("Unknown seek\n");
		return 0;
	}
}

int FS_Ftell(int fileid) {
	fileid--;
	if(fileid < 0 && fileid >= MAX_OPEN_FILES) {
		Sys_Error("Wrong fileid\n");
		return 0;
	}
	fs_fdesc_t* fdesc = &fs_openfiles[fileid];
	if(!fdesc->file) {
		Sys_Error("No file\n");
		return 0;
	}

	if(!fdesc->pakfile) {
		return ftell(fdesc->file);
	} else {
		return ftell(fdesc->file) - fdesc->pakfile->filepos;
	}
}

int FS_Fread(int fileid, void* buffer, int length) {
	fileid--;
	if(fileid < 0 && fileid >= MAX_OPEN_FILES) {
		Sys_Error("Wrong fileid\n");
		return 0;
	}
	fs_fdesc_t* fdesc = &fs_openfiles[fileid];
	if(!fdesc->file) {
		Sys_Error("No file\n");
		return 0;
	}

	if(fdesc->pakfile) {
		int pos = ftell(fdesc->file);
		if(pos - fdesc->pakfile->filepos + length > fdesc->pakfile->filelen) {
			length = fdesc->pakfile->filelen - pos + fdesc->pakfile->filepos;
		}
	}
	return fread(buffer, 1, length, fdesc->file);
}


/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t* FS_LoadPackFile(char* packfile) {
#if 1
	FILE* fh = fopen(packfile, "rb");
	if(!fh) {
		return 0;
	}

	dpackheader_t header;
	fread(&header, 1, sizeof(header), fh);
	if(header.ident != IDPAKHEADER) {
		Com_Printf("%s has bad magic\n", packfile);
		fclose(fh);
		return 0;
	}

	int filecount = header.dirlen / sizeof(dpackfile_t);
	if(filecount > MAX_FILES_IN_PACK) {
		Com_Printf("%s contains too many files\n", packfile);
		fclose(fh);
		return 0;
	}

	fseek(fh, header.dirofs, SEEK_SET);

	pack_t* pack = (pack_t*)malloc(sizeof(pack_t));
	strncpy(pack->filename, packfile, sizeof(pack->filename));
	pack->numfiles = filecount;
	pack->files    = malloc(filecount * sizeof(packfile_t));
	pack->string   = malloc(0x38000);

	int soff = 0;

	int i;
	for(i = 0; i < filecount; i++) {
		typedef struct {
			char name[56];
			int off;
			int len;
		} filedata_t;

		filedata_t file;
		fread(&file, sizeof(file), 1, fh);

		pack->files[i].name = pack->string + soff;
		pack->files[i].filepos = file.off;
		pack->files[i].filelen = file.len;

		int len = (int)strlen(file.name);
		memcpy(pack->string + soff, file.name, len);
		pack->string[soff + len] = '\0';
		soff += len + 1;
	}

	void* string = realloc(pack->string, soff);
	if(string != pack->string) {
		Com_Error(ERR_FATAL, "realloc error in FS_LoadPackFile\n");
		return 0;
	}

	fclose(fh);

	return pack;

#else
	dpackheader_t	header;
	//int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	//dpackfile_t		info[MAX_FILES_IN_PACK];

	FILE* packhandle = fopen(packfile, "r");
	if(!packhandle) {
		return 0;
	}

	fread(&header, 1, sizeof(header), packhandle);
	if(LittleLong(header.ident) != IDPAKHEADER) {
		Com_Error(ERR_FATAL, "%s is not a packfile", packfile);
	}
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if(numpackfiles > MAX_FILES_IN_PACK) {
		Com_Error(ERR_FATAL, "%s has %i files", packfile, numpackfiles);
	}

	newfiles = Z_Malloc(numpackfiles * sizeof(packfile_t));

	fseek(packhandle, header.dirofs, SEEK_SET);
	fread(newfiles, 1, header.dirlen, packhandle);

	pack = Z_Malloc(sizeof(pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = 0; // PSP_CHECK don't keep pak file open
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	fclose(packhandle);
	
	Com_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
#endif
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void FS_AddGameDirectory(char *dir) {
	strcpy(fs_gamedir, dir);

	searchpath_t* search = Z_Malloc(sizeof(searchpath_t));
	strcpy(search->filename, dir);
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	pack_t		*pak;

	int i;
	for(i = 0; i < 10; i++) {
		char pakfile[MAX_OSPATH];
		Com_sprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile(pakfile);
		if(!pak) {
			continue;
		}
		search = Z_Malloc(sizeof(searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;		
	}
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char* FS_Gamedir(void) {
	if(*fs_gamedir) {
		return fs_gamedir;
	} else {
		return BASEDIRNAME;
	}
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec(void) {
#if 0
	char name [MAX_QPATH];

	char* dir = Cvar_VariableString("gamedir");
	if(*dir) {
		Com_sprintf(name, sizeof(name), "%s/%s/pspautoexec.cfg", fs_basedir->string, dir); 
	} else {
		Com_sprintf(name, sizeof(name), "%s/%s/pspautoexec.cfg", fs_basedir->string, BASEDIRNAME); 
	}
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM)) {
		printf("Autoexec\n");
		Cbuf_AddText("exec pspautoexec.cfg\n");
	}
	Sys_FindClose();
#else
	Cbuf_AddText("exec pspautoexec.cfg\n");
#endif
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while(fs_searchpaths != fs_base_searchpaths) {
		if(fs_searchpaths->pack) {
			free(fs_searchpaths->pack->files);
			free(fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free(fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText ("vid_restart\nsnd_restart\n");

	Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir,BASEDIRNAME) || (*dir == 0))
	{
		Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
	}
}

#if 0 // PSP_REMOVE
/*
================
FS_Link_f

Creates a filelink_t
================
*/
void FS_Link_f (void)
{
	filelink_t	*l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("USAGE: link <from> <to>\n");
		return;
	}

	// see if the link already exists
	prev = &fs_links;
	for (l=fs_links ; l ; l=l->next)
	{
		if (!strcmp (l->from, Cmd_Argv(1)))
		{
			Z_Free (l->to);
			if (!strlen(Cmd_Argv(2)))
			{	// delete it
				*prev = l->next;
				Z_Free (l->from);
				Z_Free (l);
				return;
			}
			l->to = CopyString (Cmd_Argv(2));
			return;
		}
		prev = &l->next;
	}

	// create a new link
	l = Z_Malloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->fromlength = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}
#endif

/*
** FS_ListFiles
*/
char **FS_ListFiles( char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc( sizeof( char * ) * nfiles );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		strcpy( wildcard, Cmd_Argv( 1 ) );
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n" );
	};
}

/*
============
FS_Path_f

============
*/
void FS_Path_f (void)
{
	searchpath_t	*s;
	filelink_t		*l;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}

	Com_Printf ("\nLinks:\n");
	for (l=fs_links ; l ; l=l->next)
		Com_Printf ("%s : %s\n", l->from, l->to);
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char* FS_NextPath(char *prevpath) {
	char* prev;

	if(!prevpath) {
		return fs_gamedir;
	}

	prev = fs_gamedir;

	searchpath_t* s;
	for(s = fs_searchpaths; s; s = s->next) {
		if(s->pack) {
			continue;
		}
		if(prevpath == prev) {
			return s->filename;
		}
		prev = s->filename;
	}

	return NULL;
}


/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem(void) {
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("dir", FS_Dir_f );

	memset(fs_openfiles, 0, sizeof(fs_openfiles));

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);

	//
	// start up with baseq2 by default
	//
	FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_basedir->string) );

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	if(fs_gamedirvar->string[0]) {
		FS_SetGamedir(fs_gamedirvar->string);
		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, fs_gamedirvar->string));
	}
}
