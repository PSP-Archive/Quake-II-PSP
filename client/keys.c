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
#include "client.h"

#define		MAXCMDLINE	256
char	key_lines[32][MAXCMDLINE];
int		key_linepos;
int		shift_down=false;
int	anykeydown;

int		edit_line=0;
int		history_line=0;

int		key_waiting;
char	*keybindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];	// if true, can't be rebound while in menu
int		keyshift[256];		// key to map to if shift held down in console
int		key_repeats[256];	// if > 1, it is autorepeating
qboolean	keydown[256];

typedef struct {
	char* name;
	int keynum;
} keyname_t;

keyname_t keynames[] = {
	{"LEFTARROW",    K_LEFTARROW},
	{"RIGHTARROW",   K_RIGHTARROW},
	{"DOWNARROW",    K_DOWNARROW},
	{"UPARROW",      K_UPARROW},
	{"SQUARE",       K_SQUARE},
	{"CIRCLE",       K_CIRCLE},
	{"CROSS",        K_CROSS},
	{"TRIANGLE",     K_TRIANGLE},
	{"LTRIGGER",     K_LTRIGGER},
	{"RTRIGGER",     K_RTRIGGER},
	{"START",        K_START},
	{"SELECT",       K_SELECT},
	{"NOTE",         K_NOTE},
	{"HOLD",         K_HOLD},
	{"EP_PLAYPAUSE", K_EP_PLAYPAUSE},
	{"EP_FORWARD",   K_EP_FORWARD},
	{"EP_BACK",      K_EP_BACK},
	{"EP_VOLUP",     K_EP_VOLUP},
	{"EP_VOLDOWN",   K_EP_VOLDOWN},
	{"EP_HOLD",      K_EP_HOLD},
	{NULL,           0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void CompleteCommand(void) {
	char* s = key_lines[edit_line]+1;
	if(*s == '\\' || *s == '/') {
		s++;
	}

	// PSP_FIXME check for multiple

	char* cmd = Cmd_CompleteCommand(s);
	if(!cmd) {
		cmd = Cvar_CompleteVariable(s);
	}

	if(cmd) {
		key_lines[edit_line][1] = '/';
		strcpy(key_lines[edit_line]+2, cmd);
		key_linepos = strlen(cmd)+2;
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
int con_inputmode = 0;

void Key_Console(int key) {
	if(con_inputmode) {
		if(key == K_CROSS || key == K_CIRCLE) {
			// backslash text are commands, else chat
			if(key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/') {
				Cbuf_AddText (key_lines[edit_line]+2);	// skip the >
			} else {
				Cbuf_AddText (key_lines[edit_line]+1);	// valid command
			}
			Cbuf_AddText("\n");
			Cbuf_Execute();
			Com_Printf ("%s\n",key_lines[edit_line]);
			edit_line = (edit_line + 1) & 31;
			history_line = edit_line;
			key_lines[edit_line][0] = '>';
			key_linepos = 1;
			if (cls.state == ca_disconnected) {
				SCR_UpdateScreen();
			}
			con_inputmode = 0;
			return;
		}

		if(key == K_LTRIGGER) {
#if (defined(PSP_PSPOSK) && PSP_PSPOSK > 0)
			const char desc_name[] = "console input";

			unsigned short desc[128];
			unsigned short intext[128];
			unsigned short outtext[128];
			
			memset(desc   , 0, sizeof(desc));
			memset(intext , 0, sizeof(intext));
			memset(outtext, 0, sizeof(outtext));
			
			int i;
			for(i = 0; key_lines[edit_line][i+1]; i++) {
				intext[i] = key_lines[edit_line][i+1];
			}
			for(i = 0; desc_name[i]; i++) {
				desc[i] = desc_name[i];
			}
			
			PspOsk osk;
			memset(&osk, 0, sizeof(osk));
			osk.size = sizeof(osk);
			osk.language = 2;
			osk.buttonswap = 0;
			osk.unk_12 = 17;
			osk.unk_16 = 19;
			osk.unk_20 = 18;
			osk.unk_24 = 16;
			
			PspOskData data;
			memset(&data, 0, sizeof(data));
			data.language = 2;
			data.lines = 1;
			data.unk_24 = 1;
			data.desc = desc;
			data.intext = intext;
			data.outtextlength = 128;
			data.outtextlimit = 127;
			data.outtext = outtext;
			
			osk.unk_48 = 1;
			osk.data = &data;
			
			int rc = sceUtilityOskInitStart(&osk);
			if(rc) {
				return;
			}
			
			int running = 1;
			while(running) {
				switch(sceUtilityOskGetStatus()) {
				case OSK_INIT :
					break;
				case OSK_VISIBLE :
					break;
				case OSK_QUIT :
					sceUtilityOskShutdownStart();
					break;
				default :
					running = 0;
					break;
				}
				SCR_UpdateScreen();
			}
			
			if(data.rc == 2) {
				key_linepos = 1;
				for(i = 0; data.outtext[i]; i++) {
					unsigned short c = data.outtext[i];
					if(32 <= c && c < 128) {
						key_lines[edit_line][key_linepos] = (unsigned char)c;
						key_linepos++;
					}
				}
			}
#else
			key_linepos = 1;
			key_lines[edit_line][key_linepos] = 0;
#endif
			return;
		}

		if(key == K_RTRIGGER) {
			if(key_linepos > 1) {
				CompleteCommand();
			}
			return;
		}

		if(key == K_RIGHTARROW) {
			key_lines[edit_line][key_linepos] = 'a';
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
			return;
		}
	
		if(key == K_LEFTARROW) {
			if(key_linepos > 1) {
				key_linepos--;
			}
			return;
		}

		if(key == K_DOWNARROW) {
			if(key_linepos > 1) {
				key_lines[edit_line][key_linepos-1]--;
				if(key_lines[edit_line][key_linepos-1] < 32) {
					key_lines[edit_line][key_linepos-1] = 127;
				}	
			}
			return;
		}

		if(key == K_UPARROW) {
			if(key_linepos > 1) {
				key_lines[edit_line][key_linepos-1]++;
				if((unsigned char)key_lines[edit_line][key_linepos-1] > 127) {
					key_lines[edit_line][key_linepos-1] = 32;
				}
			}
			return;
		}
	
		if(key == K_SQUARE) {
			if(key_linepos > 1) {
				if('a' <= key_lines[edit_line][key_linepos-1] && key_lines[edit_line][key_linepos-1] <= 'z') {
					key_lines[edit_line][key_linepos-1] += 'A' - 'a';
				} else if('A' <= key_lines[edit_line][key_linepos-1] && key_lines[edit_line][key_linepos-1] <= 'Z') {
					key_lines[edit_line][key_linepos-1] += 'a' - 'A';
				} else {
					key_lines[edit_line][key_linepos-1] = 'a';
				}
			}
			return;
		}
	
		if(key == K_TRIANGLE) {
			if(key_linepos > 1) {
				if('0' <= key_lines[edit_line][key_linepos-1] && key_lines[edit_line][key_linepos-1] <= '9') {
					key_lines[edit_line][key_linepos-1] = ' ';
				} else {
					key_lines[edit_line][key_linepos-1] = '0';
				}
			}
			return;
		}
		
	} else {
	
		if(key == K_UPARROW) {
			do {
				history_line -=  1;
				history_line &= 31;
			} while(history_line != edit_line && !key_lines[history_line][1]);
			
			if(history_line == edit_line) {
				history_line = (edit_line + 1) & 31;
			}
			strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = strlen(key_lines[edit_line]);
			return;
		}
		
		if(key == K_DOWNARROW) {
			if(history_line == edit_line) {
				return;
			}
			do {
				history_line +=  1;
				history_line &= 31;
			} while(history_line != edit_line && !key_lines[history_line][1]);
			
			if(history_line == edit_line) {
				key_lines[edit_line][0] = '>';
				key_linepos = 1;
			} else {
				strcpy(key_lines[edit_line], key_lines[history_line]);
				key_linepos = strlen(key_lines[edit_line]);
			}
			return;
		}
		
		if(key == K_RTRIGGER) {
			con.display -= 4;
			return;
		}
		
		if(key == K_LTRIGGER) {
			con.display += 4;
			if(con.display > con.current) {
				con.display = con.current;
			}
			return;
		}
		
		if(0) { // No Key for Home
			con.display = con.current - con.totallines + 10;
			return;
		}
		
		if(0) { // No key for End
			con.display = con.current;
			return;
		}
		
		if(key == K_CROSS || key == K_CIRCLE) {
			con_inputmode = 1;
			return;
		}
	}
}

//============================================================================

qboolean	chat_team;
char		chat_buffer[MAXCMDLINE];
int			chat_bufferlen = 0;

void Key_Message (int key)
{
#if 0 // PSP_REMOVE
	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		cls.key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == sizeof(chat_buffer)-1)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
#endif
}

//============================================================================


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum(char* str) {
	keyname_t* kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char* Key_KeynumToString(int keynum) {
	keyname_t* kn;
	
	if(keynum == -1) {
		return "<KEY NOT FOUND>";
	}

	for(kn = keynames; kn->name; kn++) {
		if(keynum == kn->keynum) {
			return kn->name;
		}
	}

	return "<UNKNOWN KEYNUM>";
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, char *binding)
{
	char	*new;
	int		l;
			
	if (keynum == -1)
		return;

// free old bindings
	if(keybindings[keynum]) {
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
			
// allocate memory for new binding
	l = strlen (binding);	
	new = Z_Malloc (l+1);
	strcpy (new, binding);
	new[l] = 0;
	keybindings[keynum] = new;	
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}
	
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, "");
}

void Key_Unbindall_f (void)
{
	int		i;
	
	for (i=0 ; i<256 ; i++)
		if (keybindings[i])
			Key_SetBinding (i, "");
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b;
	char		cmd[1024];
	
	c = Cmd_Argc();

	if (c < 2)
	{
		Com_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Com_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b] );
		else
			Com_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}
	
// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != (c-1))
			strcat (cmd, " ");
	}

	Key_SetBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		if (keybindings[i] && keybindings[i][0])
			fprintf (f, "bind %s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}


/*
============
Key_Bindlist_f

============
*/
void Key_Bindlist_f (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		if (keybindings[i] && keybindings[i][0])
			Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}

/*
===================
Key_Init
===================
*/
void Key_Init(void) {
	int i;
	for(i = 0; i < 32; i++) {
		key_lines[i][0] = '>';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	Cmd_AddCommand("bind",Key_Bind_f);
	Cmd_AddCommand("unbind",Key_Unbind_f);
	Cmd_AddCommand("unbindall",Key_Unbindall_f);
	Cmd_AddCommand("bindlist",Key_Bindlist_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event(int key, qboolean down, unsigned time) {
	// console key is hardcoded, so the user can never unbind it
	if(key == K_SELECT) {
		if(!down) {
			return;
		}
		Con_ToggleConsole_f();
		return;
	}

	// menu key is hardcoded, so the user can never unbind it
	if(key == K_START) {
		if(!down) {
			return;
		}

		// put away windows
		if(cl.frame.playerstate.stats[STAT_LAYOUTS] && cls.key_dest == key_game) {
			Cbuf_AddText("cmd putaway");
			return;
		}

		switch(cls.key_dest) {
			case key_message :
				Key_Message(key);
				break;

			case key_menu :
				M_Keydown(key);
				break;

			case key_game :
			case key_console :
				M_Menu_Main_f();
				break;

			default :
				Com_Error(ERR_FATAL, "Bad cls.key_dest");
		}
		return;
	}

	if(cls.key_dest == key_game) {
		if(keybindings[key]) {
			char* ke = keybindings[key];
			if(ke) {
				if(ke[0] == '+') {
					char cmd[64];
					Com_sprintf(cmd, sizeof(cmd), "%s %i %i\n", ke, key, time);
					if(!down) {
						cmd[0] = '-';
					}
					Cbuf_AddText(cmd);
				} else {
					if(down) {
						Cbuf_AddText(ke);
						Cbuf_AddText("\n");
					}
				}
			}
		}
		return;
	}

	if(!down) {
		return;
	}

	extern cvar_t* gu_flipscreen;
	if(gu_flipscreen && gu_flipscreen->value) {
		switch(key) {
		case K_LEFTARROW :
			key = K_RIGHTARROW;
			break;
		case K_RIGHTARROW :
			key = K_LEFTARROW;
			break;
		case K_UPARROW :
			key = K_DOWNARROW;
			break;
		case K_DOWNARROW :
			key = K_UPARROW;
			break;
		default :
			break;
		}
	}

	switch(cls.key_dest) {
		case key_message:
			Key_Message(key);
			break;

		case key_menu:
			M_Keydown(key);
			break;

		case key_console:
			Key_Console(key);
			break;

		default:
			break;
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	anykeydown = false;

	for (i=0 ; i<256 ; i++)
	{
		if ( keydown[i] || key_repeats[i] )
			Key_Event( i, false, 0 );
		keydown[i] = 0;
		key_repeats[i] = 0;
	}
}


/*
===================
Key_GetKey
===================
*/
int Key_GetKey(void) {
	key_waiting = -1;

	while(key_waiting == -1)
		Sys_SendKeyEvents();

	return key_waiting;
}
