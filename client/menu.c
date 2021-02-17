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

#include <psputility_netparam.h>
#include <pspwlan.h>

#include <ctype.h>
#include "client.h"
#include "../client/qmenu.h"

static int	m_main_cursor;

#define NUM_CURSOR_FRAMES 15

static const char* menu_in_sound	= "misc/menu1.wav";
static const char* menu_move_sound	= "misc/menu2.wav";
static const char* menu_out_sound	= "misc/menu3.wav";

static const char* menu_disabledenabled_names[] = {
	"disabled", "enabled", 0
};
static const char* menu_noyes_names[] = {
	"no", "yes", 0
};

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

static float ClampCvar( float min, float max, float value ) {
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

static void M_Banner( char *name )
{
	int w, h;

	re.DrawGetPicSize (&w, &h, name );
	re.DrawPic( 480 / 2 - w / 2, 272 / 2 - 110, name );
}

void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (Cvar_VariableValue ("maxclients") == 1 
		&& Com_ServerState ())
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i=0 ; i<m_menudepth ; i++)
		if (m_layers[i].draw == draw &&
			m_layers[i].key == key)
		{
			m_menudepth = i;
		}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

void M_ForceMenuOff(void)
{
	m_drawfunc = 0;
	m_keyfunc = 0;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates ();
	Cvar_Set ("paused", "0");
}

void M_PopMenu(void) {
	S_StartLocalSound(menu_out_sound);
	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");
	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
		M_ForceMenuOff ();
}

const char* Default_MenuKey(menuframework_s *m, int key) {
	const char *sound = NULL;
	menucommon_s *item;

	if(m) {
		if((item = Menu_ItemAtCursor(m)) != 0) {
			if (item->type == MTYPE_FIELD) {
				menufield_s* f = (menufield_s*)item;
#if (defined(PSP_PSPOSK) && PSP_PSPOSK > 0)
				if(key == K_LTRIGGER) {
					unsigned short desc[128];
					unsigned short intext[128];
					unsigned short outtext[128];
					
					memset(desc   , 0, sizeof(desc));
					memset(intext , 0, sizeof(intext));
					memset(outtext, 0, sizeof(outtext));
					
					int i;
					for(i = 0; i < f->cursor; i++) {
						intext[i] = f->buffer[i];
					}
					for(i = 0; f->generic.name[i]; i++) {
						desc[i] = f->generic.name[i];
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
					data.outtextlimit = f->length < 127 ? f->length : 127;
					data.outtext = outtext;
					
					osk.unk_48 = 1;
					osk.data = &data;
					
					int rc = sceUtilityOskInitStart(&osk);
					if(rc) {
						return 0;
					}
					
					int running = 1;
					while(running) {
						switch(sceUtilityOskGetStatus()) {
						case OSK_INIT :
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
						f->cursor = 0;
						for(i = 0; data.outtext[i]; i++) {
							unsigned char c = data.outtext[i];
							if(f->generic.flags & QMF_NUMBERSONLY) {
								if('0' <= c && c <= '9') {
									f->buffer[f->cursor] = (unsigned char)c;
									f->cursor++;
								}
							} else {
								if(32 <= c && c <= 127) {
									f->buffer[f->cursor] = (unsigned char)c;
									f->cursor++;
								}
							}
						}
					}
				} else
#endif
				if(f->generic.flags & QMF_ACTIVE) {
					Field_Key( f, key );
					return NULL;
				} else if(key == K_CIRCLE || key == K_CROSS) {
					f->generic.flags |= QMF_ACTIVE;
					f->cursor = strlen(f->buffer);
				}
			}
		}
	}

	switch ( key )
	{
	case K_START:
		M_PopMenu();
		return menu_out_sound;
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;
		
	case K_CROSS:
	case K_CIRCLE:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
	re.DrawChar ( cx + ((480 - 320)>>1), cy + ((272 - 240)>>1), num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawPic (int x, int y, char *pic)
{
	re.DrawPic (x + ((480 - 320)>>1), y + ((272 - 240)>>1), pic);
}


/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y.  The pic will extend to the left of x,
and both above and below y.
=============
*/
void M_DrawCursor( int x, int y, int f )
{
	char	cursorname[80];
	static qboolean cached;

	if ( !cached )
	{
		int i;

		for ( i = 0; i < NUM_CURSOR_FRAMES; i++ )
		{
			Com_sprintf( cursorname, sizeof( cursorname ), "m_cursor%d", i );

			re.RegisterPic( cursorname );
		}
		cached = true;
	}

	Com_sprintf( cursorname, sizeof(cursorname), "m_cursor%d", f );
	re.DrawPic( x, y, cursorname );
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 4);
	}
	M_DrawCharacter (cx, cy+8, 7);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx, cy, 5);
		}
		M_DrawCharacter (cx, cy+8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 6);
	}
	M_DrawCharacter (cx, cy+8, 9);
}

		
/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5


void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for ( i = 0; names[i] != 0; i++ )
	{
		re.DrawGetPicSize( &w, &h, names[i] );

		if ( w > widest )
			widest = w;
		totalheight += ( h + 12 );
	}

	ystart = ( 272 / 2 - 110 );
	xoffset = ( 480 - widest + 70 ) / 2;

	for ( i = 0; names[i] != 0; i++ )
	{
		if ( i != m_main_cursor ) {
			re.DrawPic( xoffset, ystart + i * 40 + 13, names[i] );
		}
	}
	strcpy( litname, names[m_main_cursor] );
	strcat( litname, "_sel" );
	re.DrawPic( xoffset, ystart + m_main_cursor * 40 + 13, litname );

	M_DrawCursor( xoffset - 25, ystart + m_main_cursor * 40 + 11, (int)(cls.realtime / 100)%NUM_CURSOR_FRAMES );

	re.DrawGetPicSize( &w, &h, "m_main_plaque" );
	re.DrawPic( xoffset - 30 - w, ystart, "m_main_plaque" );

	re.DrawPic( xoffset - 30 - w, ystart + h + 5, "m_main_logo" );
}


const char *M_Main_Key (int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_START:
		M_PopMenu ();
		break;

	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_CROSS:
	case K_CIRCLE:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_Game_f ();
			break;

		case 1:
			M_Menu_Multiplayer_f();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Video_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}

	return NULL;
}


void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/
static menuframework_s	s_multiplayer_menu;
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;

static void Multiplayer_MenuDraw(void) {
	M_Banner("m_banner_multiplayer");

	Menu_AdjustCursor(&s_multiplayer_menu, 1);
	Menu_Draw(&s_multiplayer_menu );
}

static void PlayerSetupFunc(void* unused) {
	M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc(void* unused) {
	M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc(void* unused) {
	M_Menu_StartServer_f();
}

void Multiplayer_MenuInit(void) {
	s_multiplayer_menu.x = 240 + 32;
	s_multiplayer_menu.y = 40;
	s_multiplayer_menu.nitems = 0;

	s_join_network_server_action.generic.type	  = MTYPE_ACTION;
	s_join_network_server_action.generic.flags    = 0;
	s_join_network_server_action.generic.x		  = 0;
	s_join_network_server_action.generic.y		  = 0;
	s_join_network_server_action.generic.name	  = "join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	   = MTYPE_ACTION;
	s_start_network_server_action.generic.flags    = 0;
	s_start_network_server_action.generic.x		   = 0;
	s_start_network_server_action.generic.y		   = 10;
	s_start_network_server_action.generic.name	   = "start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type	   = MTYPE_ACTION;
	s_player_setup_action.generic.flags    = 0;
	s_player_setup_action.generic.x		   = 0;
	s_player_setup_action.generic.y		   = 20;
	s_player_setup_action.generic.name	   = "player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

	Menu_AddItem(&s_multiplayer_menu, (void*)&s_join_network_server_action);
	Menu_AddItem(&s_multiplayer_menu, (void*)&s_start_network_server_action);
	Menu_AddItem(&s_multiplayer_menu, (void*)&s_player_setup_action);
	//Menu_AddItem(&s_multiplayer_menu, (void*)&s_wificonnection_list);

	//if(!sceWlanGetSwitchState()) {
	//	Menu_SetStatusBar(&s_multiplayer_menu, "Enable Wlan Switch for more connection selection");
	//} else {
		Menu_SetStatusBar(&s_multiplayer_menu, 0);
	//}

	Menu_Center(&s_multiplayer_menu);
}

const char* Multiplayer_MenuKey(int key) {
	return Default_MenuKey(&s_multiplayer_menu, key);
}

void M_Menu_Multiplayer_f(void) {
	Multiplayer_MenuInit();
	M_PushMenu(Multiplayer_MenuDraw, Multiplayer_MenuKey);
}

/*
=======================================================================

KEYS MENU

=======================================================================
*/
char *bindnames[][2] = {
{"+attack", 		"attack"},
{"weapprev",        "prev weapon"},
{"weapnext", 		"next weapon"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+moveup",			"up / jump"},
{"+movedown",		"down / crouch"},
{"inven",			"inventory"},
{"invuse",			"use item"},
{"invdrop",			"drop item"},
{"invprev",			"prev item"},
{"invnext",			"next item"},
{"cmd help", 		"help computer" }, 
{ 0, 0 }
};

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s		s_keys_attack_action;
static menuaction_s		s_keys_prev_weapon_action;
static menuaction_s		s_keys_next_weapon_action;
static menuaction_s		s_keys_walk_forward_action;
static menuaction_s		s_keys_backpedal_action;
static menuaction_s		s_keys_turn_left_action;
static menuaction_s		s_keys_turn_right_action;
static menuaction_s		s_keys_run_action;
static menuaction_s		s_keys_step_left_action;
static menuaction_s		s_keys_step_right_action;
static menuaction_s		s_keys_sidestep_action;
static menuaction_s		s_keys_look_up_action;
static menuaction_s		s_keys_look_down_action;
static menuaction_s		s_keys_center_view_action;
static menuaction_s		s_keys_move_up_action;
static menuaction_s		s_keys_move_down_action;
static menuaction_s		s_keys_inventory_action;
static menuaction_s		s_keys_inv_use_action;
static menuaction_s		s_keys_inv_drop_action;
static menuaction_s		s_keys_inv_prev_action;
static menuaction_s		s_keys_inv_next_action;

static menuaction_s		s_keys_help_computer_action;

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc( menuframework_s *menu )
{
	if ( bind_grab ) {
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, '=' );
	} else {
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, 12 + ( ( int ) ( Sys_Milliseconds() / 250 ) & 1 ) );
	}
}

static void DrawKeyBindingFunc( void *self )
{
	int keys[2];
	menuaction_s *a = ( menuaction_s * ) self;

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys);
		
	if (keys[0] == -1)
	{
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???" );
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (keys[0]);

		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name );

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString( a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, "or" );
			Menu_DrawString( a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, Key_KeynumToString (keys[1]) );
		}
	}
}

static void KeyBindingFunc(void *self) {
	menuaction_s* a = (menuaction_s*)self;
	int keys[2];

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys);

	if (keys[1] != -1)
		M_UnbindCommand( bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar( &s_keys_menu, "press a key or button for this action" );
}

static void Keys_MenuInit( void ) {
	int y = 0;
	int i = 0;

	s_keys_menu.x = 480 * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	s_keys_attack_action.generic.type	= MTYPE_ACTION;
	s_keys_attack_action.generic.flags  = QMF_GRAYED;
	s_keys_attack_action.generic.x		= 0;
	s_keys_attack_action.generic.y		= y;
	s_keys_attack_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_attack_action.generic.localdata[0] = i;
	s_keys_attack_action.generic.name	= bindnames[s_keys_attack_action.generic.localdata[0]][1];

	s_keys_prev_weapon_action.generic.type	= MTYPE_ACTION;
	s_keys_prev_weapon_action.generic.flags  = QMF_GRAYED;
	s_keys_prev_weapon_action.generic.x		= 0;
	s_keys_prev_weapon_action.generic.y		= y += 9;
	s_keys_prev_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_prev_weapon_action.generic.localdata[0] = ++i;
	s_keys_prev_weapon_action.generic.name	= bindnames[s_keys_prev_weapon_action.generic.localdata[0]][1];

	s_keys_next_weapon_action.generic.type	= MTYPE_ACTION;
	s_keys_next_weapon_action.generic.flags  = QMF_GRAYED;
	s_keys_next_weapon_action.generic.x		= 0;
	s_keys_next_weapon_action.generic.y		= y += 9;
	s_keys_next_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_next_weapon_action.generic.localdata[0] = ++i;
	s_keys_next_weapon_action.generic.name	= bindnames[s_keys_next_weapon_action.generic.localdata[0]][1];

	s_keys_walk_forward_action.generic.type	= MTYPE_ACTION;
	s_keys_walk_forward_action.generic.flags  = QMF_GRAYED;
	s_keys_walk_forward_action.generic.x		= 0;
	s_keys_walk_forward_action.generic.y		= y += 9;
	s_keys_walk_forward_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_walk_forward_action.generic.localdata[0] = ++i;
	s_keys_walk_forward_action.generic.name	= bindnames[s_keys_walk_forward_action.generic.localdata[0]][1];

	s_keys_backpedal_action.generic.type	= MTYPE_ACTION;
	s_keys_backpedal_action.generic.flags  = QMF_GRAYED;
	s_keys_backpedal_action.generic.x		= 0;
	s_keys_backpedal_action.generic.y		= y += 9;
	s_keys_backpedal_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_backpedal_action.generic.localdata[0] = ++i;
	s_keys_backpedal_action.generic.name	= bindnames[s_keys_backpedal_action.generic.localdata[0]][1];

	s_keys_turn_left_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_left_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_left_action.generic.x		= 0;
	s_keys_turn_left_action.generic.y		= y += 9;
	s_keys_turn_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_left_action.generic.localdata[0] = ++i;
	s_keys_turn_left_action.generic.name	= bindnames[s_keys_turn_left_action.generic.localdata[0]][1];

	s_keys_turn_right_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_right_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_right_action.generic.x		= 0;
	s_keys_turn_right_action.generic.y		= y += 9;
	s_keys_turn_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_right_action.generic.localdata[0] = ++i;
	s_keys_turn_right_action.generic.name	= bindnames[s_keys_turn_right_action.generic.localdata[0]][1];

	s_keys_run_action.generic.type	= MTYPE_ACTION;
	s_keys_run_action.generic.flags  = QMF_GRAYED;
	s_keys_run_action.generic.x		= 0;
	s_keys_run_action.generic.y		= y += 9;
	s_keys_run_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_run_action.generic.localdata[0] = ++i;
	s_keys_run_action.generic.name	= bindnames[s_keys_run_action.generic.localdata[0]][1];

	s_keys_step_left_action.generic.type	= MTYPE_ACTION;
	s_keys_step_left_action.generic.flags  = QMF_GRAYED;
	s_keys_step_left_action.generic.x		= 0;
	s_keys_step_left_action.generic.y		= y += 9;
	s_keys_step_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_left_action.generic.localdata[0] = ++i;
	s_keys_step_left_action.generic.name	= bindnames[s_keys_step_left_action.generic.localdata[0]][1];

	s_keys_step_right_action.generic.type	= MTYPE_ACTION;
	s_keys_step_right_action.generic.flags  = QMF_GRAYED;
	s_keys_step_right_action.generic.x		= 0;
	s_keys_step_right_action.generic.y		= y += 9;
	s_keys_step_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_right_action.generic.localdata[0] = ++i;
	s_keys_step_right_action.generic.name	= bindnames[s_keys_step_right_action.generic.localdata[0]][1];

	s_keys_sidestep_action.generic.type	= MTYPE_ACTION;
	s_keys_sidestep_action.generic.flags  = QMF_GRAYED;
	s_keys_sidestep_action.generic.x		= 0;
	s_keys_sidestep_action.generic.y		= y += 9;
	s_keys_sidestep_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_sidestep_action.generic.localdata[0] = ++i;
	s_keys_sidestep_action.generic.name	= bindnames[s_keys_sidestep_action.generic.localdata[0]][1];

	s_keys_look_up_action.generic.type	= MTYPE_ACTION;
	s_keys_look_up_action.generic.flags  = QMF_GRAYED;
	s_keys_look_up_action.generic.x		= 0;
	s_keys_look_up_action.generic.y		= y += 9;
	s_keys_look_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_up_action.generic.localdata[0] = ++i;
	s_keys_look_up_action.generic.name	= bindnames[s_keys_look_up_action.generic.localdata[0]][1];

	s_keys_look_down_action.generic.type	= MTYPE_ACTION;
	s_keys_look_down_action.generic.flags  = QMF_GRAYED;
	s_keys_look_down_action.generic.x		= 0;
	s_keys_look_down_action.generic.y		= y += 9;
	s_keys_look_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_down_action.generic.localdata[0] = ++i;
	s_keys_look_down_action.generic.name	= bindnames[s_keys_look_down_action.generic.localdata[0]][1];

	s_keys_center_view_action.generic.type	= MTYPE_ACTION;
	s_keys_center_view_action.generic.flags  = QMF_GRAYED;
	s_keys_center_view_action.generic.x		= 0;
	s_keys_center_view_action.generic.y		= y += 9;
	s_keys_center_view_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_center_view_action.generic.localdata[0] = ++i;
	s_keys_center_view_action.generic.name	= bindnames[s_keys_center_view_action.generic.localdata[0]][1];

	s_keys_move_up_action.generic.type	= MTYPE_ACTION;
	s_keys_move_up_action.generic.flags  = QMF_GRAYED;
	s_keys_move_up_action.generic.x		= 0;
	s_keys_move_up_action.generic.y		= y += 9;
	s_keys_move_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_up_action.generic.localdata[0] = ++i;
	s_keys_move_up_action.generic.name	= bindnames[s_keys_move_up_action.generic.localdata[0]][1];

	s_keys_move_down_action.generic.type	= MTYPE_ACTION;
	s_keys_move_down_action.generic.flags  = QMF_GRAYED;
	s_keys_move_down_action.generic.x		= 0;
	s_keys_move_down_action.generic.y		= y += 9;
	s_keys_move_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_down_action.generic.localdata[0] = ++i;
	s_keys_move_down_action.generic.name	= bindnames[s_keys_move_down_action.generic.localdata[0]][1];

	s_keys_inventory_action.generic.type	= MTYPE_ACTION;
	s_keys_inventory_action.generic.flags  = QMF_GRAYED;
	s_keys_inventory_action.generic.x		= 0;
	s_keys_inventory_action.generic.y		= y += 9;
	s_keys_inventory_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inventory_action.generic.localdata[0] = ++i;
	s_keys_inventory_action.generic.name	= bindnames[s_keys_inventory_action.generic.localdata[0]][1];

	s_keys_inv_use_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_use_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_use_action.generic.x		= 0;
	s_keys_inv_use_action.generic.y		= y += 9;
	s_keys_inv_use_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_use_action.generic.localdata[0] = ++i;
	s_keys_inv_use_action.generic.name	= bindnames[s_keys_inv_use_action.generic.localdata[0]][1];

	s_keys_inv_drop_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_drop_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_drop_action.generic.x		= 0;
	s_keys_inv_drop_action.generic.y		= y += 9;
	s_keys_inv_drop_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_drop_action.generic.localdata[0] = ++i;
	s_keys_inv_drop_action.generic.name	= bindnames[s_keys_inv_drop_action.generic.localdata[0]][1];

	s_keys_inv_prev_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_prev_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_prev_action.generic.x		= 0;
	s_keys_inv_prev_action.generic.y		= y += 9;
	s_keys_inv_prev_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_prev_action.generic.localdata[0] = ++i;
	s_keys_inv_prev_action.generic.name	= bindnames[s_keys_inv_prev_action.generic.localdata[0]][1];

	s_keys_inv_next_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_next_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_next_action.generic.x		= 0;
	s_keys_inv_next_action.generic.y		= y += 9;
	s_keys_inv_next_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_next_action.generic.localdata[0] = ++i;
	s_keys_inv_next_action.generic.name	= bindnames[s_keys_inv_next_action.generic.localdata[0]][1];

	s_keys_help_computer_action.generic.type	= MTYPE_ACTION;
	s_keys_help_computer_action.generic.flags  = QMF_GRAYED;
	s_keys_help_computer_action.generic.x		= 0;
	s_keys_help_computer_action.generic.y		= y += 9;
	s_keys_help_computer_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_help_computer_action.generic.localdata[0] = ++i;
	s_keys_help_computer_action.generic.name	= bindnames[s_keys_help_computer_action.generic.localdata[0]][1];

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_attack_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_prev_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_next_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_walk_forward_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_backpedal_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_run_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_sidestep_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_center_view_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_down_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inventory_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_use_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_drop_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_prev_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_next_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_help_computer_action );
	
	Menu_SetStatusBar( &s_keys_menu, "cross or circle to change, square or triangle to clear" );
	Menu_Center( &s_keys_menu );
}

static void Keys_MenuDraw (void) {
	Menu_AdjustCursor( &s_keys_menu, 1 );
	Menu_Draw( &s_keys_menu );
}

static const char *Keys_MenuKey( int key ) {
	menuaction_s *item = ( menuaction_s * ) Menu_ItemAtCursor( &s_keys_menu );

	if(bind_grab) {	
		if(key != K_START && key != K_SELECT) {
			char cmd[1024];
			Com_sprintf (cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
			Cbuf_Execute();
		}
		
		Menu_SetStatusBar( &s_keys_menu, "cross or circle to change, square or triangle to clear" );
		bind_grab = false;
		return menu_out_sound;
	}

	switch(key) {
		case K_CIRCLE :
		case K_CROSS :
			KeyBindingFunc( item );
			return menu_in_sound;
			
		case K_SQUARE :
		case K_TRIANGLE :
			M_UnbindCommand( bindnames[item->generic.localdata[0]][0] );
			return menu_out_sound;
	}
	return Default_MenuKey( &s_keys_menu, key );
}

void M_Menu_Keys_f(void) {
	Keys_MenuInit();
	M_PushMenu( Keys_MenuDraw, Keys_MenuKey );
}


/*
=======================================================================

CONTROLS MENU

=======================================================================
*/

static menuframework_s	s_options_menu;
static menulist_s		s_options_alwaysrun;
static menulist_s		s_options_crosshair;
static menuslider_s		s_options_speed;
static menuslider_s		s_options_tolerance;
static menuslider_s		s_options_acceleration;
static menuslider_s		s_options_anubacc;
static menulist_s		s_options_anubfunc;
static menulist_s		s_options_anubinvert;
#if 0
static menuslider_s		s_options_soundvolume;
static menuslider_s		s_options_musicvolume;
#endif
static menulist_s		s_options_cpufreq;
static menuaction_s		s_options_defaults;
static menuaction_s		s_options_controls;
#if 0
static menuaction_s		s_options_console;
#endif

static void Options_SetValues(void) {
	Cvar_SetValue("cl_run", ClampCvar(0.0f, 1.0f, cl_run->value));
	s_options_alwaysrun.curvalue = cl_run->value;

	Cvar_SetValue("crosshair", ClampCvar(0.0f, 3.0f, crosshair->value));
	s_options_crosshair.curvalue = crosshair->value;

	extern cvar_t* in_speed;
	Cvar_SetValue("in_speed", ClampCvar(1.0f, 9.0f, in_speed->value));
	s_options_speed.curvalue = in_speed->value * 2.0f;

	extern cvar_t* in_tolerance;
	Cvar_SetValue("in_tolerance", ClampCvar(0.0f, 0.2f, in_tolerance->value));
	s_options_tolerance.curvalue = in_tolerance->value * 20.0f;

	extern cvar_t* in_acceleration;
	Cvar_SetValue("in_acceleration", ClampCvar(0.0f, 4.0f, in_acceleration->value));
	s_options_acceleration.curvalue = in_acceleration->value;

	extern cvar_t* in_anubfunc;
	Cvar_SetValue("in_anubfunc", ClampCvar(0.0f, 2.0f, in_anubfunc->value));
	s_options_anubfunc.curvalue = in_anubfunc->value;

	extern cvar_t* in_anubinvert;
	Cvar_SetValue("in_anubinvert", ClampCvar(0.0f, 1.0f, in_anubinvert->value));
	s_options_anubinvert.curvalue = in_anubinvert->value;

#if 0
	extern cvar_t* s_volume;
	Cvar_SetValue("s_volume", ClampCvar(0, 1, s_volume->value));
	s_options_soundvolume.curvalue = s_volume->value * 10.0f;

	extern cvar_t* music_volume;
	Cvar_SetValue("music_volume", ClampCvar(0, 1, music_volume->value));
	s_options_musicvolume.curvalue = music_volume->value * 10.0f;
#endif

	extern cvar_t* sys_cpufreq;
	if(sys_cpufreq->value == 333.0f) {
		s_options_cpufreq.curvalue = 1.0f;
	} else {
		s_options_cpufreq.curvalue = 0.0f;
	}
}

static void f_options_alwaysrun(void* unused) {
	Cvar_SetValue("cl_run", s_options_alwaysrun.curvalue);
}

static void f_options_crosshair(void *unused) {
	Cvar_SetValue("crosshair", s_options_crosshair.curvalue);
}

static void f_options_speed(void* unused) {
	Cvar_SetValue("in_speed", s_options_speed.curvalue / 2.0f);
}

static void f_options_tolerance(void* unused) {
	Cvar_SetValue("in_tolerance", s_options_tolerance.curvalue / 20.0f);
}

static void f_options_acceleration(void* unused) {
	Cvar_SetValue("in_acceleration", s_options_acceleration.curvalue);
}

static void f_options_anubfunc(void* unused) {
	Cvar_SetValue("in_anubfunc", s_options_anubfunc.curvalue);
}

static void f_options_anubinvert(void *unused) {
	Cvar_SetValue("in_anubinvert", s_options_anubinvert.curvalue);
}

#if 0
static void f_options_soundvolume(void *unused) {
	Cvar_SetValue("s_volume", s_options_soundvolume.curvalue / 10.0f);
}

static void f_options_musicvolume(void* ununsed) {
	Cvar_SetValue("music_volume", s_options_musicvolume.curvalue / 10.0f);
}
#endif

static void f_options_cpufreq(void* unsused) {
	if(s_options_cpufreq.curvalue == 1.0f) {
		Cvar_SetValue("psp_cpufreq", 333.0f);
	} else {
		Cvar_SetValue("psp_cpufreq", 222.0f);
	}
}

static void f_options_controls(void *unused) {
	M_Menu_Keys_f();
}

static void f_options_default( void *unused ) {
	Cbuf_AddText("exec default.cfg\n");
	Cbuf_Execute();

	void Options_SetValues(void);
	Options_SetValues();
}

#if 0
static void f_options_console(void *unused) {
	extern void Key_ClearTyping(void);
	if(cl.attractloop) {
		Cbuf_AddText("killserver\n");
		return;
	}
	Key_ClearTyping();
	Con_ClearNotify();
	M_ForceMenuOff();
	cls.key_dest = key_console;
}
#endif

void Options_MenuInit(void) {
	static const char* crosshair_names[] = {
		"none", "cross", "dot", "angle", 0
	};

	static const char* cpuspeed_names[] = {
		"222 mhz", "333 mhz", 0
	};

	static const char* anubfunc_names[] = {
		"look", "move", "strafe", 0
	};

	int y = 0;

	s_options_menu.x = 480 / 2;
	s_options_menu.y = 272 / 2 - 58;
	s_options_menu.nitems = 0;

	s_options_alwaysrun.generic.type        = MTYPE_SPINCONTROL;
	s_options_alwaysrun.generic.x           = 0;
	s_options_alwaysrun.generic.y           = y += 10;
	s_options_alwaysrun.generic.name        = "always run";
	s_options_alwaysrun.generic.callback    = f_options_alwaysrun;
	s_options_alwaysrun.itemnames           = menu_noyes_names;
	Menu_AddItem(&s_options_menu, (void*)&s_options_alwaysrun);

	s_options_crosshair.generic.type		= MTYPE_SPINCONTROL;
	s_options_crosshair.generic.x			= 0;
	s_options_crosshair.generic.y			= y += 10;
	s_options_crosshair.generic.name		= "crosshair";
	s_options_crosshair.generic.callback	= f_options_crosshair;
	s_options_crosshair.itemnames			= crosshair_names;
	Menu_AddItem(&s_options_menu, (void*)&s_options_crosshair);

	y += 10;

	s_options_speed.generic.type			= MTYPE_SLIDER;
	s_options_speed.generic.x				= 0;
	s_options_speed.generic.y				= y += 10;
	s_options_speed.generic.name			= "speed";
	s_options_speed.generic.callback		= f_options_speed;
	s_options_speed.minvalue				= 1.0f;
	s_options_speed.maxvalue				= 17.0f;
	Menu_AddItem(&s_options_menu, (void*)&s_options_speed);

	s_options_tolerance.generic.type		= MTYPE_SLIDER;
	s_options_tolerance.generic.x			= 0;
	s_options_tolerance.generic.y			= y += 10;
	s_options_tolerance.generic.name		= "tolerance";
	s_options_tolerance.generic.callback	= f_options_tolerance;
	s_options_tolerance.minvalue			= 0.0f;
	s_options_tolerance.maxvalue			= 8.0f;
	Menu_AddItem(&s_options_menu, (void*)&s_options_tolerance);

	s_options_acceleration.generic.type		= MTYPE_SLIDER;
	s_options_acceleration.generic.x		= 0;
	s_options_acceleration.generic.y		= y += 10;
	s_options_acceleration.generic.name		= "acceleration";
	s_options_acceleration.generic.callback	= f_options_acceleration;
	s_options_acceleration.minvalue			= 0.0f;
	s_options_acceleration.maxvalue			= 4.0f;
	Menu_AddItem(&s_options_menu, (void*)&s_options_acceleration);

	s_options_anubfunc.generic.type			= MTYPE_SPINCONTROL;
	s_options_anubfunc.generic.x			= 0;
	s_options_anubfunc.generic.y			= y += 10;
	s_options_anubfunc.generic.name			= "anub function";
	s_options_anubfunc.generic.callback		= f_options_anubfunc;
	s_options_anubfunc.itemnames			= anubfunc_names;
	Menu_AddItem(&s_options_menu, (void*)&s_options_anubfunc);

	s_options_anubinvert.generic.type		= MTYPE_SPINCONTROL;
	s_options_anubinvert.generic.x          = 0;
	s_options_anubinvert.generic.y			= y += 10;
	s_options_anubinvert.generic.name		= "anub invert";
	s_options_anubinvert.generic.callback   = f_options_anubinvert;
	s_options_anubinvert.itemnames			= menu_noyes_names;
	Menu_AddItem(&s_options_menu, (void*)&s_options_anubinvert);

	y += 10;

#if 0
	s_options_soundvolume.generic.type		= MTYPE_SLIDER;
	s_options_soundvolume.generic.x			= 0;
	s_options_soundvolume.generic.y			= y += 10;
	s_options_soundvolume.generic.name		= "sound volume";
	s_options_soundvolume.generic.callback	= f_options_soundvolume;
	s_options_soundvolume.minvalue			= 0.0f;
	s_options_soundvolume.maxvalue			= 10.0f;
	Menu_AddItem(&s_options_menu, (void*)&s_options_soundvolume);

	s_options_musicvolume.generic.type		= MTYPE_SLIDER;
	s_options_musicvolume.generic.x			= 0;
	s_options_musicvolume.generic.y			= y += 10;
	s_options_musicvolume.generic.name		= "music volume";
	s_options_musicvolume.generic.callback	= f_options_musicvolume;
	s_options_musicvolume.minvalue			= 0.0f;
	s_options_musicvolume.maxvalue			= 10.0f;
	Menu_AddItem(&s_options_menu, (void*)&s_options_musicvolume);
#endif

	s_options_cpufreq.generic.type			= MTYPE_SPINCONTROL;
	s_options_cpufreq.generic.x				= 0;
	s_options_cpufreq.generic.y				= y += 10;
	s_options_cpufreq.generic.name			= "cpu frequency";
	s_options_cpufreq.generic.callback		= f_options_cpufreq;
	s_options_cpufreq.itemnames				= cpuspeed_names;
	Menu_AddItem(&s_options_menu, (void*)&s_options_cpufreq);

	y += 10;

	s_options_defaults.generic.type			= MTYPE_ACTION;
	s_options_defaults.generic.x			= 0;
	s_options_defaults.generic.y			= y += 10;
	s_options_defaults.generic.name			= "reset defaults";
	s_options_defaults.generic.callback		= f_options_default;
	Menu_AddItem(&s_options_menu, (void*)&s_options_defaults);

	s_options_controls.generic.type			= MTYPE_ACTION;
	s_options_controls.generic.x			= 0;
	s_options_controls.generic.y			= y += 10;
	s_options_controls.generic.name			= "edit controls";
	s_options_controls.generic.callback		= f_options_controls;
	Menu_AddItem(&s_options_menu, (void*)&s_options_controls);

#if 0
	s_options_console.generic.type			= MTYPE_ACTION;
	s_options_console.generic.x				= 0;
	s_options_console.generic.y				= y += 10;
	s_options_console.generic.name			= "go to console";
	s_options_console.generic.callback		= f_options_console;
	Menu_AddItem(&s_options_menu, (void*)&s_options_console);
#endif

	Options_SetValues();
}

void Options_MenuDraw(void) {
	M_Banner("m_banner_options");
	Menu_AdjustCursor( &s_options_menu, 1 );
	Menu_Draw(&s_options_menu);
}

const char* Options_MenuKey(int key) {
	return Default_MenuKey(&s_options_menu, key);
}

void M_Menu_Options_f(void) {
	Options_MenuInit();
	M_PushMenu(Options_MenuDraw, Options_MenuKey);
}

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

extern cvar_t* scr_drawfps;
extern cvar_t* scr_drawtime;

static menuframework_s	s_video_menu;
static menulist_s		s_video_vsync;
static menulist_s		s_video_texfilter;
static menulist_s		s_video_particletype;
static menulist_s		s_video_drawfps;
static menulist_s		s_video_drawtime;
static menuaction_s		s_video_defaults;

static void Video_SetValues(void) {
	cvar_t* gu_vsync         = Cvar_Get("gu_vsync", "1", 0);
	cvar_t* gu_texfilter     = Cvar_Get("gu_texfilter", "0", 0);
	cvar_t* gu_particletype  = Cvar_Get("gu_particletype", "0", 0);

	gu_vsync->value = ClampCvar(0, 1, gu_vsync->value);
	s_video_vsync.curvalue = gu_vsync->value;

	gu_texfilter->value = ClampCvar(0, 1, gu_texfilter->value);
	s_video_texfilter.curvalue = gu_texfilter->value;

	gu_particletype->value = ClampCvar(0, 2, gu_particletype->value);
	s_video_particletype.curvalue = gu_particletype->value;

	scr_drawfps->value = ClampCvar(0, 1, scr_drawfps->value);
	s_video_drawfps.curvalue = scr_drawfps->value;

	scr_drawtime->value = ClampCvar(0, 1, scr_drawtime->value);
	s_video_drawfps.curvalue = scr_drawfps->value;
}

static void f_video_vsync(void* unused) {
	Cvar_SetValue("gu_vsync", s_video_vsync.curvalue);
}

static void f_video_texfilter(void* unused) {
	Cvar_SetValue("gu_texfilter", s_video_texfilter.curvalue);
}

static void f_video_particletype(void* unused) {
	Cvar_SetValue("gu_particletype", s_video_particletype.curvalue);
}

static void f_video_drawfps(void* unused) {
	Cvar_SetValue("scr_drawfps", s_video_drawfps.curvalue);
}

static void f_video_drawtime(void* unused) {
	Cvar_SetValue("scr_drawtime", s_video_drawtime.curvalue);
}

static void f_video_defaults(void* unused) {
	Video_SetValues();
}

/*
** VID_MenuInit
*/
void VID_MenuInit(void) {
	const static char* menu_nearestlinear_names[] = {
		"nearest", "linear", 0
	};
	const static char* menu_particletype_names[] = {
		"none", "points", "sprites", 0
	};

	s_video_menu.x = 480 / 2;
	s_video_menu.y = 272 / 2 - 58;
	s_video_menu.nitems = 0;

	s_video_vsync.generic.type				= MTYPE_SPINCONTROL;
	s_video_vsync.generic.x					= 0;
	s_video_vsync.generic.y					= 0;
	s_video_vsync.generic.name				= "vertical sync";
	s_video_vsync.generic.callback			= f_video_vsync;
	s_video_vsync.itemnames					= menu_disabledenabled_names;

	s_video_texfilter.generic.type			= MTYPE_SPINCONTROL;
	s_video_texfilter.generic.x				= 0;
	s_video_texfilter.generic.y				= 10;
	s_video_texfilter.generic.name			= "texture filter";
	s_video_texfilter.generic.callback		= f_video_texfilter;
	s_video_texfilter.itemnames				= menu_nearestlinear_names;

	s_video_particletype.generic.type		= MTYPE_SPINCONTROL;
	s_video_particletype.generic.x			= 0;
	s_video_particletype.generic.y			= 20;
	s_video_particletype.generic.name		= "particle type";
	s_video_particletype.generic.callback	= f_video_particletype;
	s_video_particletype.itemnames			= menu_particletype_names;

	s_video_drawfps.generic.type			= MTYPE_SPINCONTROL;
	s_video_drawfps.generic.x				= 0;
	s_video_drawfps.generic.y				= 40;
	s_video_drawfps.generic.name			= "draw fps";
	s_video_drawfps.generic.callback		= f_video_drawfps;
	s_video_drawfps.itemnames				= menu_noyes_names;

	s_video_drawtime.generic.type			= MTYPE_SPINCONTROL;
	s_video_drawtime.generic.x				= 0;
	s_video_drawtime.generic.y				= 50;
	s_video_drawtime.generic.name			= "draw time";
	s_video_drawtime.generic.callback		= f_video_drawtime;
	s_video_drawtime.itemnames				= menu_noyes_names;
	
	s_video_defaults.generic.type			= MTYPE_ACTION;
	s_video_defaults.generic.x				= 0;
	s_video_defaults.generic.y				= 70;
	s_video_defaults.generic.name			= "reset defaults";
	s_video_defaults.generic.callback		= f_video_defaults;

	Video_SetValues();

	Menu_AddItem(&s_video_menu, (void*)&s_video_vsync);
	Menu_AddItem(&s_video_menu, (void*)&s_video_texfilter);
	Menu_AddItem(&s_video_menu, (void*)&s_video_particletype);
	Menu_AddItem(&s_video_menu, (void*)&s_video_drawfps);
	Menu_AddItem(&s_video_menu, (void*)&s_video_drawtime);
	Menu_AddItem(&s_video_menu, (void*)&s_video_defaults);
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw(void) {
	M_Banner("m_banner_video");
	Menu_AdjustCursor(&s_video_menu, 1);
	Menu_Draw(&s_video_menu);
}

/*
================
VID_MenuKey
================
*/
const char* VID_MenuKey(int key) {
	return Default_MenuKey(&s_video_menu, key);
}


void M_Menu_Video_f (void) {
	VID_MenuInit();
	M_PushMenu(VID_MenuDraw, VID_MenuKey);
}

/*
=============================================================================

END GAME MENU

=============================================================================
*/
static int credits_tick_time;
static int credits_run_time;
static qboolean credits_running;

static const char** credits;
static const char* idcredits[] = {
	"+Quake II by Id Software",
	"",
	"",
	"",
	"+EMERGENCY EXIT",
	"www.teamemergencyexit.com",
	"",
	"+TITAN",
	"www.titandemo.org",
	"",
	"+Programming",
	"McZonk",
	"",
	"+Beta Testing Support",
	"Rehbock",
	"",
	"+Additional Support",
	"Tyranid, Chip",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ID SOFTWARE",
	"www.idsoftware.com",
	"",
	"+Programming",
	"John Carmack, John Cash, Brian Hook",
	"",
	"+Art",
	"Adrian Carmack, Kevin Cloud, Paul Steed",
	"",
	"+Level Design",
	"Tim Willits, American McGee, Christian Antkow",
	"Paul Jaquays, Brandon James",
	"",
	"+Biz",
	"Todd Hollenshead, Barrett Alexander, Donna Jackson",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

void M_Credits_MenuDraw( void ) {
	int i, y;

	if(credits_running) {
		credits_run_time += cls.realtime - credits_tick_time;
	}
	credits_tick_time = cls.realtime;

	/*
	** draw the credits
	*/
	for ( i = 0, y = 272 - (credits_run_time / 40.0F ); credits[i] && y < 272; y += 10, i++ )
	{
		int j, stringoffset = 0;
		int bold = false;

		if ( y <= -8 )
			continue;

		if ( credits[i][0] == '+' )
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for ( j = 0; credits[i][j+stringoffset]; j++ )
		{
			int x;

			x = ( 480 - strlen( credits[i] ) * 8 - stringoffset * 8 ) / 2 + ( j + stringoffset ) * 8;

			if ( bold ) {
				re.DrawChar( x, y, credits[i][j+stringoffset] + 128 );
			} else {
				re.DrawChar( x, y, credits[i][j+stringoffset] );
			}
		}
	}

	if(y < 0) {
		credits_run_time = 0;
	}
}

const char* M_Credits_Key( int key )
{
	switch (key)
	{
	case K_CROSS:
	case K_CIRCLE :
		credits_running ^= 1;
		break;

	case K_START:
		M_PopMenu ();
		break;
	}

	return menu_out_sound;
}

void M_Menu_Credits_f( void ) {
	credits = idcredits;

	credits_tick_time = cls.realtime;
	credits_run_time = 0;
	credits_running = true;

	M_PushMenu( M_Credits_MenuDraw, M_Credits_Key);
}

/*
=============================================================================

GAME MENU

=============================================================================
*/

static int		m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;

static void StartGame(void) {
	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff ();
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );

	Cvar_SetValue( "gamerules", 0 );		//PGM

	//Cbuf_AddText ("loading ; killserver ; wait ; newgame\n"); // PSP_FIXME
	Cbuf_AddText("killserver; wait; newgame\n");
	cls.key_dest = key_game;
}

static void EasyGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "0" );
	StartGame();
}

static void MediumGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "1" );
	StartGame();
}

static void HardGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "2" );
	StartGame();
}

static void LoadGameFunc( void *unused )
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc( void *unused )
{
	M_Menu_SaveGame_f();
}

static void CreditsFunc( void *unused )
{
	M_Menu_Credits_f();
}

void Game_MenuInit( void ) {

	s_game_menu.x = 480 * 0.50;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= 0;
	s_easy_game_action.generic.y		= 0;
	s_easy_game_action.generic.name	= "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= 0;
	s_medium_game_action.generic.y		= 10;
	s_medium_game_action.generic.name	= "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= 0;
	s_hard_game_action.generic.y		= 20;
	s_hard_game_action.generic.name	= "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= 0;
	s_load_game_action.generic.y		= 40;
	s_load_game_action.generic.name	= "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= 0;
	s_save_game_action.generic.y		= 50;
	s_save_game_action.generic.name	= "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x		= 0;
	s_credits_action.generic.y		= 60;
	s_credits_action.generic.name	= "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem( &s_game_menu, ( void * ) &s_easy_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_medium_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_hard_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_load_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_save_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_credits_action );

	Menu_Center( &s_game_menu );
}

void Game_MenuDraw( void )
{
	M_Banner( "m_banner_game" );
	Menu_AdjustCursor( &s_game_menu, 1 );
	Menu_Draw( &s_game_menu );
}

const char *Game_MenuKey( int key )
{
	return Default_MenuKey( &s_game_menu, key );
}

void M_Menu_Game_f (void)
{
	Game_MenuInit();
	M_PushMenu( Game_MenuDraw, Game_MenuKey );
	m_game_cursor = 1;
}

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

#define	MAX_SAVEGAMES	15

static menuframework_s	s_savegame_menu;

static menuframework_s	s_loadgame_menu;
static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];

char		m_savestrings[MAX_SAVEGAMES][32];
qboolean	m_savevalid[MAX_SAVEGAMES];

void Create_Savestrings (void)
{
	int		i;
	FILE	*f;
	char	name[MAX_OSPATH];

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		Com_sprintf (name, sizeof(name), "%s/save/save%i/server.ssv", FS_Gamedir(), i);
		f = fopen (name, "rb");
		if (!f)
		{
			strcpy (m_savestrings[i], "<EMPTY>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read (m_savestrings[i], sizeof(m_savestrings[i]), f);
			fclose (f);
			m_savevalid[i] = true;
		}
	}
}

void LoadGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	if ( m_savevalid[ a->generic.localdata[0] ] )
		Cbuf_AddText (va("load save%i\n",  a->generic.localdata[0] ) );
	M_ForceMenuOff ();
}

void LoadGame_MenuInit( void )
{
	int i;

	s_loadgame_menu.x = 480 / 2 - 120;
	s_loadgame_menu.y = 272 / 2 - 58;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings();

	for ( i = 0; i < MAX_SAVEGAMES; i++ )
	{
		s_loadgame_actions[i].generic.name			= m_savestrings[i];
		s_loadgame_actions[i].generic.flags			= QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0]	= i;
		s_loadgame_actions[i].generic.callback		= LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = ( i ) * 10;
		if(i>0)	// separate from autosave
			s_loadgame_actions[i].generic.y += 10;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_loadgame_menu, &s_loadgame_actions[i] );
	}
}

void LoadGame_MenuDraw( void )
{
	M_Banner( "m_banner_load_game" );
//	Menu_AdjustCursor( &s_loadgame_menu, 1 );
	Menu_Draw( &s_loadgame_menu );
}

const char *LoadGame_MenuKey( int key )
{
	if ( key == K_CIRCLE || key == K_CROSS )
	{
		s_savegame_menu.cursor = s_loadgame_menu.cursor - 1;
		if ( s_savegame_menu.cursor < 0 )
			s_savegame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_loadgame_menu, key );
}

void M_Menu_LoadGame_f (void)
{
	LoadGame_MenuInit();
	M_PushMenu( LoadGame_MenuDraw, LoadGame_MenuKey );
}


/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/
static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];

void SaveGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	Cbuf_AddText (va("save save%i\n", a->generic.localdata[0] ));
	M_ForceMenuOff ();
}

void SaveGame_MenuDraw( void )
{
	M_Banner( "m_banner_save_game" );
	Menu_AdjustCursor( &s_savegame_menu, 1 );
	Menu_Draw( &s_savegame_menu );
}

void SaveGame_MenuInit( void )
{
	int i;

	s_savegame_menu.x = 480 / 2 - 120;
	s_savegame_menu.y = 272 / 2 - 58;
	s_savegame_menu.nitems = 0;

	Create_Savestrings();

	// don't include the autosave slot
	for ( i = 0; i < MAX_SAVEGAMES-1; i++ )
	{
		s_savegame_actions[i].generic.name = m_savestrings[i+1];
		s_savegame_actions[i].generic.localdata[0] = i+1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = ( i ) * 10;

		s_savegame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_savegame_menu, &s_savegame_actions[i] );
	}
}

const char *SaveGame_MenuKey( int key )
{
	if ( key == K_CROSS || key == K_CIRCLE )
	{
		s_loadgame_menu.cursor = s_savegame_menu.cursor - 1;
		if ( s_loadgame_menu.cursor < 0 )
			s_loadgame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_savegame_menu, key );
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState())
		return;		// not playing a game

	SaveGame_MenuInit();
	M_PushMenu( SaveGame_MenuDraw, SaveGame_MenuKey );
	Create_Savestrings ();
}


/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
#define MAX_LOCAL_SERVERS 8

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int		m_num_servers;
#define	NO_SERVER_STRING	"<no server>"

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

void M_AddToServerList (netadr_t adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;
	while ( *info == ' ' )
		info++;

	// ignore if duplicated
	for (i=0 ; i<m_num_servers ; i++)
		if (!strcmp(info, local_server_names[i]))
			return;

	local_server_netadr[m_num_servers] = adr;
	strncpy (local_server_names[m_num_servers], info, sizeof(local_server_names[0])-1);
	m_num_servers++;
}


void JoinServerFunc( void *self )
{
	char	buffer[128];
	int		index;

	index = ( menuaction_s * ) self - s_joinserver_server_actions;

	if ( Q_stricmp( local_server_names[index], NO_SERVER_STRING ) == 0 )
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof(buffer), "connect %s\n", NET_AdrToString (local_server_netadr[index]));
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

void AddressBookFunc( void *self )
{
	M_Menu_AddressBook_f();
}

void NullCursorDraw( void *self )
{
}

void SearchLocalGames(void) {
	m_num_servers = 0;

	int	i;
	for(i = 0; i<MAX_LOCAL_SERVERS ; i++) {
		strcpy(local_server_names[i], NO_SERVER_STRING);
	}

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Searching for local servers, this" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	// send out info packets
	CL_PingServers_f();
}

void SearchLocalGamesFunc(void *self) {
	SearchLocalGames();
}

void JoinServer_MenuInit( void )
{
	int i;

	s_joinserver_menu.x = 480 * 0.50 - 120;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type	= MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name	= "address book";
	s_joinserver_address_book_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x		= 0;
	s_joinserver_address_book_action.generic.y		= 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= "refresh server list";
	s_joinserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x	= 0;
	s_joinserver_search_action.generic.y	= 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x    = 80;
	s_joinserver_server_title.generic.y	   = 30;

	for ( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		strcpy(local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x		= 0;
		s_joinserver_server_actions[i].generic.y		= 40 + i*10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = "press X or O to connect";
	}

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_address_book_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_title );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_action );

	for(i = 0; i < 8; i++) {
		Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_actions[i] );
	}

	Menu_Center( &s_joinserver_menu );

	SearchLocalGames();
}

void JoinServer_MenuDraw(void)
{
	M_Banner( "m_banner_join_server" );
	Menu_Draw( &s_joinserver_menu );
}


const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( JoinServer_MenuDraw, JoinServer_MenuKey );
}


/*
=============================================================================

START SERVER MENU

=============================================================================
*/
extern int Developer_searchpath (int who);

static menuframework_s s_startserver_menu;
static char* maplist[256];
static char mapnames[4096];

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menuaction_s s_startserver_botoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc(void* self) {
	if(s_rules_box.curvalue == 1) {
		return;
	}
	M_Menu_DMOptions_f();
}

void BotOptionsFunc(void* self) {
}

void RulesChangeFunc ( void *self )
{
	// DM
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if(s_rules_box.curvalue == 1)		// coop				// PGM
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy( s_maxclients_field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
}

void StartServerActionFunc(void *self) {
	char	startmap[128];
	int		timelimit;
	int		fraglimit;
	int		maxclients;
//	char*	spot;

	strncpy(startmap, strchr(maplist[s_startmap_list.curvalue], '\n') + 1, sizeof(startmap)-1);

	maxclients  = atoi(s_maxclients_field.buffer);
	timelimit	= atoi(s_timelimit_field.buffer);
	fraglimit	= atoi(s_fraglimit_field.buffer);

	Cvar_SetValue( "maxclients", ClampCvar( 0, maxclients, maxclients ) );
	Cvar_SetValue ("timelimit", ClampCvar( 0, timelimit, timelimit ) );
	Cvar_SetValue ("fraglimit", ClampCvar( 0, fraglimit, fraglimit ) );
	Cvar_Set("hostname", s_hostname_field.buffer );
//	Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
//	Cvar_SetValue ("coop", s_rules_box.curvalue );

#if 0 // PSP_FIXME
//PGM
	if((s_rules_box.curvalue < 2) || (Developer_searchpath(2) != 2))
	{
		Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
		Cvar_SetValue ("coop", s_rules_box.curvalue );
		Cvar_SetValue ("gamerules", 0 );
	}
	else
	{
		Cvar_SetValue ("deathmatch", 1 );	// deathmatch is always true for rogue games, right?
		Cvar_SetValue ("coop", 0 );			// FIXME - this might need to depend on which game we're running
		Cvar_SetValue ("gamerules", s_rules_box.curvalue );
	}
//PGM
#else
	Cvar_SetValue("deathmatch", 1);
	Cvar_SetValue("coop", 0);
	Cvar_SetValue("gamerules", 0);
#endif

#if 0 // PSP_FIXME
	spot = NULL;
	if (s_rules_box.curvalue == 1)		// PGM
	{
 		if(Q_stricmp(startmap, "bunk1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "mintro") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "fact1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "power1") == 0)
  			spot = "pstart";
 		else if(Q_stricmp(startmap, "biggun") == 0)
  			spot = "bstart";
 		else if(Q_stricmp(startmap, "hangar1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "city1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va("map %s\n", startmap));
	}
#else
	Cbuf_AddText(va("map %s\n", startmap));
#endif

	M_ForceMenuOff ();
}

void StartServer_MenuInit(void) {
	static const char *dm_coop_names[] = {
		"deathmatch",
		"cooperative",
		0
	};
//=======
//PGM
	static const char *dm_coop_names_rogue[] = {
		"deathmatch",
		"cooperative",
		"tag",
//		"deathball",
		0
	};
//PGM
//=======

#if 0
	char *buffer;
	char  mapsname[1024];
	char *s;
	int length;
	int i;
	FILE *fp;

	/*
	** load the list of map names
	*/
	Com_sprintf( mapsname, sizeof( mapsname ), "%s/maps.lst", FS_Gamedir() );
	if(!(fp = fopen( mapsname, "rb"))) {
		Com_Error(ERR_FATAL, "couldn't find maps.lst\n");
		return;
	}

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = malloc( length );
	fread( buffer, length, 1, fp );

	s = buffer;

	i = 0;
	while ( i < length )
	{
		if ( s[i] == '\r' )
			nummaps++;
		i++;
	}

	if ( nummaps == 0 )
		Com_Error( ERR_DROP, "no maps in maps.lst\n" );

	mapnames = malloc( sizeof( char * ) * ( nummaps + 1 ) );
	memset( mapnames, 0, sizeof( char * ) * ( nummaps + 1 ) );

	s = buffer;

	for ( i = 0; i < nummaps; i++ )
	{
    char  shortname[MAX_TOKEN_CHARS];
    char  longname[MAX_TOKEN_CHARS];
		char  scratch[200];
		int		j, l;

		strcpy( shortname, COM_Parse( &s ) );
		l = strlen(shortname);
		for (j=0 ; j<l ; j++)
			shortname[j] = toupper(shortname[j]);
		strcpy( longname, COM_Parse( &s ) );
		Com_sprintf( scratch, sizeof( scratch ), "%s\n%s", longname, shortname );

		mapnames[i] = malloc( strlen( scratch ) + 1 );
		strcpy( mapnames[i], scratch );
	}
	mapnames[nummaps] = 0;

	if(fp != 0) {
		fclose(fp);
		free( buffer );
	}
#else

	char mapfile[MAX_TOKEN_CHARS];
	Com_sprintf(mapfile, sizeof(mapfile), "%s/maps.lst", FS_Gamedir());
	
	FILE* fh = fopen(mapfile, "r");
	if(!fh) {
		Com_Error(ERR_FATAL, "Unable to open: %s\n", mapfile);
		return;
	}

	maplist[0] = 0;
	int offset = 0;
	int number = 0;

	char mapstring[128];
	while(fgets(mapstring, sizeof(mapstring), fh)) {
		char shortname[MAX_TOKEN_CHARS];
		char longname[MAX_TOKEN_CHARS];
		char fullname[MAX_TOKEN_CHARS];

		char* mapstring2 = mapstring;
		strncpy(shortname, COM_Parse(&mapstring2), sizeof(shortname)-1);
		strncpy(longname, COM_Parse(&mapstring2), sizeof(longname)-1);

		{
			int len = strlen(shortname);
			int i;
			for(i = 0; i < len; i++) {
				shortname[i] = toupper(shortname[i]);
			}
		}

		Com_sprintf(fullname, sizeof(fullname), "%s\n%s", longname, shortname);
		
		if(number + 1 >= (sizeof(maplist) / sizeof(char*))) {
			Com_Printf("Map list full\n");
			break;
		}

		if(strlen(fullname) + offset + 1 >= sizeof(mapnames)) {
			Com_Printf("Map list full\n");
			break;
		}

		strncpy(mapnames + offset, fullname, sizeof(mapnames) - (offset+1));
		maplist[number]   = mapnames + offset;
		maplist[number+1] = 0;
		offset += strlen(fullname) + 1;
		number += 1;
	}

	fclose(fh);

	if(!maplist[0]) {
		Com_Error(ERR_FATAL, "Unable to read: %s\n", mapfile);
		return;
	}
#endif

	/*
	** initialize the menu stuff
	*/
	s_startserver_menu.x = 480 * 0.50;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type	= MTYPE_SPINCONTROL;
	s_startmap_list.generic.x		= 0;
	s_startmap_list.generic.y		= 0;
	s_startmap_list.generic.name	= "initial map";
	s_startmap_list.itemnames		= (const char**)maplist;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	 = 0;
	s_rules_box.generic.y	 = 20;
	s_rules_box.generic.name = "rules";
	
//PGM - rogue games only available with rogue DLL.
	if(Developer_searchpath(2) == 2)
		s_rules_box.itemnames = dm_coop_names_rogue;
	else
		s_rules_box.itemnames = dm_coop_names;
//PGM

	if (Cvar_VariableValue("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy( s_timelimit_field.buffer, Cvar_VariableString("timelimit") );
	s_timelimit_field.cursor = strlen(s_timelimit_field.buffer);

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy( s_fraglimit_field.buffer, Cvar_VariableString("fraglimit") );
	s_fraglimit_field.cursor = strlen(s_fraglimit_field.buffer);

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is. 
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= 0;
	s_maxclients_field.generic.y	= 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if ( Cvar_VariableValue( "maxclients" ) == 1 )
		strcpy( s_maxclients_field.buffer, "8" );
	else 
		strcpy( s_maxclients_field.buffer, Cvar_VariableString("maxclients") );
	s_maxclients_field.cursor = strlen(s_maxclients_field.buffer);

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= 0;
	s_hostname_field.generic.y	= 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy( s_hostname_field.buffer, Cvar_VariableString("hostname") );
	s_hostname_field.cursor = strlen(s_hostname_field.buffer);

	s_startserver_dmoptions_action.generic.type      = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	     = " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags     = QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	     = 24;
	s_startserver_dmoptions_action.generic.y	     = 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback  = DMOptionsFunc;

	s_startserver_botoptions_action.generic.type      = MTYPE_ACTION;
	s_startserver_botoptions_action.generic.name	  = " bot settings";
	s_startserver_botoptions_action.generic.flags     = QMF_LEFT_JUSTIFY;
	s_startserver_botoptions_action.generic.x	      = 24;
	s_startserver_botoptions_action.generic.y	      = 118;
	s_startserver_botoptions_action.generic.statusbar = NULL;
	s_startserver_botoptions_action.generic.callback  = BotOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= " begin";
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= 24;
	s_startserver_start_action.generic.y	= 130;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_botoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc(NULL);
}

void StartServer_MenuDraw(void)
{
	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key ) {
	if(key == K_START)
	{
		/*
		if ( mapnames )
		{
			int i;

			for ( i = 0; i < nummaps; i++ )
				free( mapnames[i] );
			free( mapnames );
		}
		mapnames = 0;
		nummaps = 0;
		*/
	}

	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f(void) {
	StartServer_MenuInit();
	M_PushMenu(StartServer_MenuDraw, StartServer_MenuKey);
}

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/
static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

//ROGUE
static menulist_s	s_no_mines_box;
static menulist_s	s_no_nukes_box;
static menulist_s	s_stack_double_box;
static menulist_s	s_no_spheres_box;
//ROGUE

static void DMFlagCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue( "dmflags" );

	if ( f == &s_friendlyfire_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if ( f == &s_falls_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if ( f == &s_weapons_stay_box ) 
	{
		bit = DF_WEAPONS_STAY;
	}
	else if ( f == &s_instant_powerups_box )
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if ( f == &s_allow_exit_box )
	{
		bit = DF_ALLOW_EXIT;
	}
	else if ( f == &s_powerups_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if ( f == &s_health_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if ( f == &s_spawn_farthest_box )
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if ( f == &s_teamplay_box )
	{
		if ( f->curvalue == 1 )
		{
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if ( f->curvalue == 2 )
		{
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~( DF_MODELTEAMS | DF_SKINTEAMS );
		}

		goto setvalue;
	}
	else if ( f == &s_samelevel_box )
	{
		bit = DF_SAME_LEVEL;
	}
	else if ( f == &s_force_respawn_box )
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if ( f == &s_armor_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if ( f == &s_infinite_ammo_box )
	{
		bit = DF_INFINITE_AMMO;
	}
	else if ( f == &s_fixed_fov_box )
	{
		bit = DF_FIXED_FOV;
	}
	else if ( f == &s_quad_drop_box )
	{
		bit = DF_QUAD_DROP;
	}

//=======
//ROGUE
	else if (Developer_searchpath(2) == 2)
	{
		if ( f == &s_no_mines_box)
		{
			bit = DF_NO_MINES;
		}
		else if ( f == &s_no_nukes_box)
		{
			bit = DF_NO_NUKES;
		}
		else if ( f == &s_stack_double_box)
		{
			bit = DF_NO_STACK_DOUBLE;
		}
		else if ( f == &s_no_spheres_box)
		{
			bit = DF_NO_SPHERES;
		}
	}
//ROGUE
//=======

	if ( f )
	{
		if ( f->curvalue == 0 )
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", flags);

	Com_sprintf( dmoptions_statusbar, sizeof( dmoptions_statusbar ), "dmflags = %d", flags );

}

void DMOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] = 
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue( "dmflags" );
	int y = 0;

	s_dmoptions_menu.x = 480 * 0.50;
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x	= 0;
	s_falls_box.generic.y	= y;
	s_falls_box.generic.name	= "falling damage";
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = ( dmflags & DF_NO_FALLING ) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x	= 0;
	s_weapons_stay_box.generic.y	= y += 10;
	s_weapons_stay_box.generic.name	= "weapons stay";
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = ( dmflags & DF_WEAPONS_STAY ) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x	= 0;
	s_instant_powerups_box.generic.y	= y += 10;
	s_instant_powerups_box.generic.name	= "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = ( dmflags & DF_INSTANT_ITEMS ) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x	= 0;
	s_powerups_box.generic.y	= y += 10;
	s_powerups_box.generic.name	= "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = ( dmflags & DF_NO_ITEMS ) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x	= 0;
	s_health_box.generic.y	= y += 10;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name	= "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = ( dmflags & DF_NO_HEALTH ) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x	= 0;
	s_armor_box.generic.y	= y += 10;
	s_armor_box.generic.name	= "allow armor";
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = ( dmflags & DF_NO_ARMOR ) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x	= 0;
	s_spawn_farthest_box.generic.y	= y += 10;
	s_spawn_farthest_box.generic.name	= "spawn farthest";
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = ( dmflags & DF_SPAWN_FARTHEST ) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x	= 0;
	s_samelevel_box.generic.y	= y += 10;
	s_samelevel_box.generic.name	= "same map";
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = ( dmflags & DF_SAME_LEVEL ) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x	= 0;
	s_force_respawn_box.generic.y	= y += 10;
	s_force_respawn_box.generic.name	= "force respawn";
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = ( dmflags & DF_FORCE_RESPAWN ) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x	= 0;
	s_teamplay_box.generic.y	= y += 10;
	s_teamplay_box.generic.name	= "teamplay";
	s_teamplay_box.generic.callback = DMFlagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x	= 0;
	s_allow_exit_box.generic.y	= y += 10;
	s_allow_exit_box.generic.name	= "allow exit";
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = ( dmflags & DF_ALLOW_EXIT ) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x	= 0;
	s_infinite_ammo_box.generic.y	= y += 10;
	s_infinite_ammo_box.generic.name	= "infinite ammo";
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = ( dmflags & DF_INFINITE_AMMO ) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x	= 0;
	s_fixed_fov_box.generic.y	= y += 10;
	s_fixed_fov_box.generic.name	= "fixed FOV";
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = ( dmflags & DF_FIXED_FOV ) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x	= 0;
	s_quad_drop_box.generic.y	= y += 10;
	s_quad_drop_box.generic.name	= "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = ( dmflags & DF_QUAD_DROP ) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x	= 0;
	s_friendlyfire_box.generic.y	= y += 10;
	s_friendlyfire_box.generic.name	= "friendly fire";
	s_friendlyfire_box.generic.callback = DMFlagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = ( dmflags & DF_NO_FRIENDLY_FIRE ) == 0;

//============
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x	= 0;
		s_no_mines_box.generic.y	= y += 10;
		s_no_mines_box.generic.name	= "remove mines";
		s_no_mines_box.generic.callback = DMFlagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = ( dmflags & DF_NO_MINES ) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x	= 0;
		s_no_nukes_box.generic.y	= y += 10;
		s_no_nukes_box.generic.name	= "remove nukes";
		s_no_nukes_box.generic.callback = DMFlagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = ( dmflags & DF_NO_NUKES ) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x	= 0;
		s_stack_double_box.generic.y	= y += 10;
		s_stack_double_box.generic.name	= "2x/4x stacking off";
		s_stack_double_box.generic.callback = DMFlagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = ( dmflags & DF_NO_STACK_DOUBLE ) != 0;

		s_no_spheres_box.generic.type = MTYPE_SPINCONTROL;
		s_no_spheres_box.generic.x	= 0;
		s_no_spheres_box.generic.y	= y += 10;
		s_no_spheres_box.generic.name	= "remove spheres";
		s_no_spheres_box.generic.callback = DMFlagCallback;
		s_no_spheres_box.itemnames = yes_no_names;
		s_no_spheres_box.curvalue = ( dmflags & DF_NO_SPHERES ) != 0;

	}
//ROGUE
//============

	Menu_AddItem( &s_dmoptions_menu, &s_falls_box );
	Menu_AddItem( &s_dmoptions_menu, &s_weapons_stay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_instant_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_health_box );
	Menu_AddItem( &s_dmoptions_menu, &s_armor_box );
	Menu_AddItem( &s_dmoptions_menu, &s_spawn_farthest_box );
	Menu_AddItem( &s_dmoptions_menu, &s_samelevel_box );
	Menu_AddItem( &s_dmoptions_menu, &s_force_respawn_box );
	Menu_AddItem( &s_dmoptions_menu, &s_teamplay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_allow_exit_box );
	Menu_AddItem( &s_dmoptions_menu, &s_infinite_ammo_box );
	Menu_AddItem( &s_dmoptions_menu, &s_fixed_fov_box );
	Menu_AddItem( &s_dmoptions_menu, &s_quad_drop_box );
	Menu_AddItem( &s_dmoptions_menu, &s_friendlyfire_box );

//=======
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		Menu_AddItem( &s_dmoptions_menu, &s_no_mines_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_nukes_box );
		Menu_AddItem( &s_dmoptions_menu, &s_stack_double_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_spheres_box );
	}
//ROGUE
//=======

	Menu_Center( &s_dmoptions_menu );

	// set the original dmflags statusbar
	DMFlagCallback( 0 );
	Menu_SetStatusBar( &s_dmoptions_menu, dmoptions_statusbar );
}

void DMOptions_MenuDraw(void)
{
	Menu_Draw( &s_dmoptions_menu );
}

const char *DMOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_dmoptions_menu, key );
}

void M_Menu_DMOptions_f (void)
{
	DMOptions_MenuInit();
	M_PushMenu( DMOptions_MenuDraw, DMOptions_MenuKey );
}

/*
=============================================================================

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/
static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static void DownloadCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", f->curvalue);
	}

	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", f->curvalue);
	}

	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", f->curvalue);
	}

	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", f->curvalue);
	}

	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", f->curvalue);
	}
}

void DownloadOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	int y = 0;

	s_downloadoptions_menu.x = 480 * 0.50;
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x    = 48;
	s_download_title.generic.y	 = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x	= 0;
	s_allow_download_box.generic.y	= y += 20;
	s_allow_download_box.generic.name	= "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x	= 0;
	s_allow_download_maps_box.generic.y	= y += 20;
	s_allow_download_maps_box.generic.name	= "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x	= 0;
	s_allow_download_players_box.generic.y	= y += 10;
	s_allow_download_players_box.generic.name	= "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x	= 0;
	s_allow_download_models_box.generic.y	= y += 10;
	s_allow_download_models_box.generic.name	= "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x	= 0;
	s_allow_download_sounds_box.generic.y	= y += 10;
	s_allow_download_sounds_box.generic.name	= "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem( &s_downloadoptions_menu, &s_download_title );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_maps_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_players_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_models_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_sounds_box );

	Menu_Center( &s_downloadoptions_menu );

	// skip over title
	if (s_downloadoptions_menu.cursor == 0)
		s_downloadoptions_menu.cursor = 1;
}

void DownloadOptions_MenuDraw(void)
{
	Menu_Draw( &s_downloadoptions_menu );
}

const char *DownloadOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_downloadoptions_menu, key );
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit();
	M_PushMenu( DownloadOptions_MenuDraw, DownloadOptions_MenuKey );
}
/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/
#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];
static menuaction_s     s_addressbook_back;

void AddressBook_Back(void* unused) {
	M_Menu_JoinServer_f();	
}

void AddressBook_MenuInit(void) {
	s_addressbook_menu.x = 480 / 2 - 142;
	s_addressbook_menu.y = 272 / 2 - 58;
	s_addressbook_menu.nitems = 0;

	int i;
	for (i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++) {
		char buffer[20];
		Com_sprintf(buffer, sizeof(buffer), "adr%d", i);

		cvar_t* adr = Cvar_Get(buffer, "", CVAR_ARCHIVE);

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x		= 0;
		s_addressbook_fields[i].generic.y		= i * 18 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor			= 0;
		s_addressbook_fields[i].length			= 60;
		s_addressbook_fields[i].visible_length	= 30;

		strcpy(s_addressbook_fields[i].buffer, adr->string);

		Menu_AddItem(&s_addressbook_menu, &s_addressbook_fields[i]);
	}

	s_addressbook_back.generic.type = MTYPE_ACTION;
	s_addressbook_back.generic.name = " back";
	s_addressbook_back.generic.callback = AddressBook_Back;
	s_addressbook_back.generic.x = 0;
	s_addressbook_back.generic.y = 180;
	s_addressbook_back.generic.flags = QMF_LEFT_JUSTIFY;
	Menu_AddItem(&s_addressbook_menu, &s_addressbook_back);
}

const char *AddressBook_MenuKey( int key )
{
	if ( key == K_START )
	{
		int index;
		char buffer[20];

		for ( index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++ )
		{
			Com_sprintf( buffer, sizeof( buffer ), "adr%d", index );
			Cvar_Set( buffer, s_addressbook_fields[index].buffer );
		}
	}
	return Default_MenuKey( &s_addressbook_menu, key );
}

void AddressBook_MenuDraw(void)
{
	M_Banner( "m_banner_addressbook" );
	Menu_Draw( &s_addressbook_menu );
}

void M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	M_PushMenu( AddressBook_MenuDraw, AddressBook_MenuKey );
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s       s_player_wifi_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_wifi_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 128

static const char* wificonnection_items[12] = { "none", 0 };
static char wificonnections[10][64] = {};

static void UpdateWifiConnections() {
	int i;
	for(i = 1; i <= 10; i++) {
		if(sceUtilityCheckNetParam(i)) {
			break;
		}
		sceUtilityGetNetParam(i, PSP_NETPARAM_NAME, (void*)wificonnections[i-1]);
		wificonnection_items[i]   = wificonnections[i-1];
		wificonnection_items[i+1] = 0;
	}
}

static void SelectWifiConnection(void* unused) {
	Cvar_SetValue("net_wificonnection", ClampCvar(0, 10, s_player_wifi_box.curvalue));
}

typedef struct
{
	int		nskins;
	char**	skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

void DownloadOptionsFunc(void *self) {
	M_Menu_DownloadOptions_f();
}

static void HandednessCallback(void *unused) {
	Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
	s_player_skin_box.itemnames = (const char**)s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

#if 0 // PSP_REMOVE
static void FreeFileList( char **list, int n )
{
	int i;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] )
		{
			free( list[i] );
			list[i] = 0;
		}
	}
	free( list );
}
#endif

#if 0 // PSP_REMOVE
static qboolean IconOfSkinExists( char *skin, char **pcxfiles, int npcxfiles )
{
	int i;
	char scratch[1024];

	strcpy( scratch, skin );
	*strrchr( scratch, '.' ) = 0;
	strcat( scratch, "_i.pcx" );

	for ( i = 0; i < npcxfiles; i++ )
	{
		if ( strcmp( pcxfiles[i], scratch ) == 0 )
			return true;
	}

	return false;
}
#endif

static qboolean PlayerConfig_ScanDirectories(void) {
#if 0 // PSP_FIXME hardcoded model selection
	char findname[1024];
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames;
	char *path = NULL;
	int i;

	extern char **FS_ListFiles( char *, int *, unsigned, unsigned );

	s_numplayermodels = 0;

	/*
	** get a list of directories
	*/
	do 
	{
		path = FS_NextPath( path );
		Com_sprintf( findname, sizeof(findname), "%s/players/*.*", path );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, SFF_SUBDIR, 0 ) ) != 0 )
			break;
	} while ( path );

	if ( !dirnames )
		return false;

	/*
	** go through the subdirectories
	*/
	npms = ndirs;
	if ( npms > MAX_PLAYERMODELS )
		npms = MAX_PLAYERMODELS;

	for ( i = 0; i < npms; i++ )
	{
		int k, s;
		char *a, *b, *c;
		char **pcxnames;
		char **skinnames;
		int npcxfiles;
		int nskins = 0;

		if ( dirnames[i] == 0 )
			continue;

		// verify the existence of tris.md2
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/tris.md2" );
		if ( !Sys_FindFirst( scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			Sys_FindClose();
			continue;
		}
		Sys_FindClose();

		// verify the existence of at least one pcx skin
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/*.pcx" );
		pcxnames = FS_ListFiles( scratch, &npcxfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );

		if ( !pcxnames )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			continue;
		}

		// count valid skins, which consist of a skin with a matching "_i" icon
		for ( k = 0; k < npcxfiles-1; k++ )
		{
			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					nskins++;
				}
			}
		}
		if ( !nskins )
			continue;

		skinnames = malloc( sizeof( char * ) * ( nskins + 1 ) );
		memset( skinnames, 0, sizeof( char * ) * ( nskins + 1 ) );

		// copy the valid skins
		for ( s = 0, k = 0; k < npcxfiles-1; k++ )
		{
			char *a, *b, *c;

			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					a = strrchr( pcxnames[k], '/' );
					b = strrchr( pcxnames[k], '\\' );

					if ( a > b )
						c = a;
					else
						c = b;

					strcpy( scratch, c + 1 );

					if ( strrchr( scratch, '.' ) )
						*strrchr( scratch, '.' ) = 0;

					skinnames[s] = strdup( scratch );
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		a = strrchr( dirnames[i], '/' );
		b = strrchr( dirnames[i], '\\' );

		if ( a > b )
			c = a;
		else
			c = b;

		strncpy( s_pmi[s_numplayermodels].displayname, c + 1, MAX_DISPLAYNAME-1 );
		strcpy( s_pmi[s_numplayermodels].directory, c + 1 );

		FreeFileList( pcxnames, npcxfiles );

		s_numplayermodels++;
	}
	if ( dirnames )
		FreeFileList( dirnames, ndirs );
#else
	
	s_numplayermodels = 2;
	
	strcpy(s_pmi[0].directory, "male");
	strcpy(s_pmi[0].displayname, "male");
	s_pmi[0].nskins = 3;
	s_pmi[0].skindisplaynames = malloc(sizeof(char*) * (s_pmi[0].nskins + 1));
	memset(s_pmi[0].skindisplaynames, 0, sizeof(char*) * (s_pmi[0].nskins + 1));
	s_pmi[0].skindisplaynames[0] = strdup("grunt");
	s_pmi[0].skindisplaynames[1] = strdup("major");
	s_pmi[0].skindisplaynames[2] = strdup("scout");

	strcpy(s_pmi[1].directory, "female");
	strcpy(s_pmi[1].displayname, "female");
	s_pmi[1].nskins = 3;
	s_pmi[1].skindisplaynames = malloc(sizeof(char*) * (s_pmi[1].nskins + 1));
	memset(s_pmi[1].skindisplaynames, 0, sizeof(char*) * (s_pmi[1].nskins + 1));
	s_pmi[1].skindisplaynames[0] = strdup("athena");;
	s_pmi[1].skindisplaynames[1] = strdup("lotus");
	s_pmi[1].skindisplaynames[2] = strdup("venus");
	
	return true;
#endif
}

#if 0 // PSP_REMOVE
static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
}
#endif

qboolean PlayerConfig_MenuInit( void ) {
	extern cvar_t *name;
	//extern cvar_t *team;
	//extern cvar_t *skin;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	static const char *handedness[] = { "right", "left", "center", 0 };

	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );

	if ( hand->value < 0 || hand->value > 2 )
		Cvar_SetValue( "hand", 0 );

	PlayerConfig_ScanDirectories();

	UpdateWifiConnections();
#if 0

	if(!PlayerConfig_ScanDirectories()) {
		return false;
	}

	if (s_numplayermodels == 0)
		return false;

	strcpy( currentdirectory, skin->string );

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
#endif
	{
		strcpy( currentdirectory, "male" );
		strcpy( currentskin, "grunt" );
	}

	//qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 )
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	s_player_config_menu.x = 480 / 2 - 95; 
	s_player_config_menu.y = 272 / 2 - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy( s_player_name_field.buffer, name->string );
	s_player_name_field.cursor = strlen( name->string );

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x    = -8;
	s_player_model_title.generic.y	 = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = (const char**)s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x    = -16;
	s_player_skin_title.generic.y	 = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -56;
	s_player_skin_box.generic.y	= 94;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = (const char**)s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x    = 32;
	s_player_hand_title.generic.y	 = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -56;
	s_player_handedness_box.generic.y	= 118;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames = handedness;

	s_player_wifi_title.generic.type = MTYPE_SEPARATOR;
	s_player_wifi_title.generic.x = 72;
	s_player_wifi_title.generic.y = 132;
	s_player_wifi_title.generic.name = "wifi connection";

	extern cvar_t* net_wificonnection;
	s_player_wifi_box.generic.type = MTYPE_SPINCONTROL;
	s_player_wifi_box.generic.x    = -56;
	s_player_wifi_box.generic.y	   = 142;
	s_player_wifi_box.generic.name = 0;
	s_player_wifi_box.generic.cursor_offset = -48;
	s_player_wifi_box.generic.callback = SelectWifiConnection;
	s_player_wifi_box.curvalue  = net_wificonnection->value;
	s_player_wifi_box.itemnames = wificonnection_items;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (Cvar_VariableValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x    = 56;
	s_player_rate_title.generic.y	 = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -56;
	s_player_rate_box.generic.y	= 166;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= "download options";
	s_player_download_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x	= -24;
	s_player_download_action.generic.y	= 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	//if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_wifi_title );
	Menu_AddItem( &s_player_config_menu, &s_player_wifi_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_download_action );

	return true;
}

void PlayerConfig_MenuDraw(void) {
	extern float CalcFov( float fov_x, float w, float h );
	refdef_t refdef;
	char scratch[MAX_QPATH];

	memset(&refdef, 0, sizeof(refdef));

	refdef.x = 480 / 2 + 24;
	refdef.y = 272 / 2 - 100;
	refdef.width = 144;
	refdef.height = 168;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
	{
		entity_t entity;

		memset(&entity, 0, sizeof(entity));

		Com_sprintf(scratch, sizeof(scratch), "players/%s/tris.md2",
			s_pmi[s_player_model_box.curvalue].directory
		);
		entity.model = re.RegisterModel(scratch);
		Com_sprintf(scratch, sizeof( scratch ), "players/%s/%s.pcx",
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]
		);
		entity.skin = re.RegisterSkin(scratch);
		entity.flags = RF_FULLBRIGHT;
		entity.origin[0] = 80;
		entity.origin[1] = 0;
		entity.origin[2] = 0;
		VectorCopy( entity.origin, entity.oldorigin );
		entity.frame = 0;
		entity.oldframe = 0;
		entity.backlerp = 0.0;

#if 0
		static int i = 0;
		if(++yaw > 180) yaw -= 360;
#else
		entity.angles[1] = cl.viewangles[YAW] - 90.0f;
		//entity.angles[2] = cl.viewangles[PITCH];
#endif		
		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;
		Menu_Draw( &s_player_config_menu );
		M_DrawTextBox( ( refdef.x ) * ( 320.0f / 480 ), ( 272 / 2 ) * ( 240.0f / 272) - 77, refdef.width / 8, refdef.height / 8 );
		refdef.height += 4;

		Com_sprintf(scratch, sizeof(scratch), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]
		);
		re.DrawPic( s_player_config_menu.x - 40, 272 / 2 - 72, scratch );

		re.RenderFrame(&refdef);
	}
}

const char* PlayerConfig_MenuKey(int key) {
	if(key == K_CIRCLE || key == K_CROSS) {
		char scratch[1024];
		Cvar_Set("name", s_player_name_field.buffer );

		Com_sprintf(scratch, sizeof(scratch), "%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]
		);

		Cvar_Set("skin", scratch);

#if 0 // PSP_REMOVE add dynamic player models
		int i, j;
		for(i = 0; i < s_numplayermodels; i++) {
			for(j = 0; j < s_pmi[i].nskins; j++) {
				if(s_pmi[i].skindisplaynames[j]) {
					free(s_pmi[i].skindisplaynames[j]);
				}
				s_pmi[i].skindisplaynames[j] = 0;
			}
			free(s_pmi[i].skindisplaynames);
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
#endif
	}
	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if(!PlayerConfig_MenuInit()) {
		Menu_SetStatusBar( &s_multiplayer_menu, "No valid player models found" );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	M_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}

/*
=======================================================================

QUIT MENU

=======================================================================
*/

const char *M_Quit_Key (int key) {
	switch (key) {
	case K_CROSS :
	case K_CIRCLE :
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		M_PopMenu ();
		break;
	}

	return NULL;
}


void M_Quit_Draw (void) {
	int		w, h;
	re.DrawGetPicSize (&w, &h, "quit");
	re.DrawPic ( (480-w)/2, (272-h)/2, "quit");
}


void M_Menu_Quit_f (void) {
	M_PushMenu (M_Quit_Draw, M_Quit_Key);
}



//=============================================================================
/* Menu Subsystem */

/*
=================
M_Init
=================
*/
void M_Init(void) {
	Cmd_AddCommand("menu_main", M_Menu_Main_f);
	Cmd_AddCommand("menu_game", M_Menu_Game_f);
		Cmd_AddCommand("menu_loadgame", M_Menu_LoadGame_f);
		Cmd_AddCommand("menu_savegame", M_Menu_SaveGame_f);
	Cmd_AddCommand("menu_multiplayer", M_Menu_Multiplayer_f);
		Cmd_AddCommand("menu_joinserver", M_Menu_JoinServer_f);
			Cmd_AddCommand("menu_addressbook", M_Menu_AddressBook_f);
		Cmd_AddCommand("menu_startserver", M_Menu_StartServer_f);
			Cmd_AddCommand("menu_dmoptions", M_Menu_DMOptions_f);
		Cmd_AddCommand("menu_playerconfig", M_Menu_PlayerConfig_f);
			Cmd_AddCommand("menu_downloadoptions", M_Menu_DownloadOptions_f);
		Cmd_AddCommand("menu_credits", M_Menu_Credits_f );
	Cmd_AddCommand("menu_video", M_Menu_Video_f);
	Cmd_AddCommand("menu_options", M_Menu_Options_f);
		Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
}

/*
=================
M_Draw
=================
*/
void M_Draw(void) {
	if (cls.key_dest != key_menu)
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cinematictime > 0) {
		re.DrawFill (0,0,480, 272, 0);
	} else {
		re.DrawFadeScreen();
	}

	m_drawfunc ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if(m_entersound) {
		S_StartLocalSound(menu_in_sound);
		m_entersound = false;
	}
}


/*
=================
M_Keydown
=================
*/
void M_Keydown (int key) {
	if(m_keyfunc) {
		const char* s;
		if((s = m_keyfunc(key)) != 0) {
			S_StartLocalSound( ( char * ) s );
		}
	}
}
