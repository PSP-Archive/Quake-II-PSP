#include "../client/client.h"

#include <pspctrl.h>
#include <psphprm.h>

cvar_t* in_speed;
cvar_t* in_tolerance;
cvar_t* in_acceleration;

cvar_t* in_anubfunc;
cvar_t* in_anubinvert;

static int keystate[K_MAX];
static int keytime = 0;

static int anubfunc = 0;
static int anubinvert = 0;

static float anubtable[256];

static void keymap(int key, qboolean down, int repeattime) {
	if(down) {
		if(keystate[key] == 0) {
			Key_Event(key, true, keytime);
			keystate[key] = keytime;
		} else if(repeattime > 0 && keystate[key] + repeattime < keytime) {
			Key_Event(key, true, keytime);
			keystate[key] += repeattime / 2;
		}
	} else {
		if(keystate[key] != 0) {
			Key_Event(key, false, keytime);
			keystate[key] = 0;
		}
	}
}

void IN_Init(void) {
	memset(keystate, 0, sizeof(keystate));

	in_tolerance    = Cvar_Get("in_tolerance",    "0.1", CVAR_ARCHIVE);
	in_speed        = Cvar_Get("in_speed",        "5.0", CVAR_ARCHIVE);
	in_acceleration = Cvar_Get("in_acceleration", "0.0", CVAR_ARCHIVE);

	in_anubfunc   = Cvar_Get("in_anubfunc",   "0", CVAR_ARCHIVE);
	in_anubinvert = Cvar_Get("in_anubinvert", "0", CVAR_ARCHIVE);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void IN_Shutdown(void) {
}

void IN_Frame(void) {
	if(in_tolerance->modified || in_speed->modified || in_acceleration->modified) {
		int i;
		for(i = 0; i < 256; i++) {
			float t = ((float)i / 128.0f) - 1.0f;
			if(t == 0.0f) {
				anubtable[i] = 0.0f;
				continue;
			}

			float a = fabs(t);
			if(a < in_tolerance->value) {
				anubtable[i] = 0.0f;
				continue;
			}

			a -= in_tolerance->value;

			a /= (1.0f - in_tolerance->value);
			a  = powf(a, in_acceleration->value);
			a *= in_speed->value;

			if(t < 0.0f) {
				t = -a;
			} else {
				t = a;
			}

			anubtable[i] = t;
		}

		in_acceleration->modified = false;
		in_speed->modified = false;
		in_tolerance->modified = false;
	}

	if(in_anubfunc->modified) {
		anubfunc = (int)in_anubfunc->value;
		in_anubfunc->modified = false;
	}

	if(in_anubinvert->modified) {
		anubinvert = (int)in_anubinvert->value;
		in_anubinvert->modified = false;
	}
	

	keytime = Sys_Milliseconds();

	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);
	keymap(K_UPARROW,			pad.Buttons & PSP_CTRL_UP,			250);
	keymap(K_DOWNARROW,			pad.Buttons & PSP_CTRL_DOWN,		250);
	keymap(K_LEFTARROW,			pad.Buttons & PSP_CTRL_LEFT,		250);
	keymap(K_RIGHTARROW,		pad.Buttons & PSP_CTRL_RIGHT,		250);
	keymap(K_CROSS,				pad.Buttons & PSP_CTRL_CROSS,		250);
	keymap(K_CIRCLE,			pad.Buttons & PSP_CTRL_CIRCLE,		250);
	keymap(K_SQUARE,			pad.Buttons & PSP_CTRL_SQUARE,		250);
	keymap(K_TRIANGLE,			pad.Buttons & PSP_CTRL_TRIANGLE,	250);
	keymap(K_LTRIGGER,			pad.Buttons & PSP_CTRL_LTRIGGER,	250);
	keymap(K_RTRIGGER,			pad.Buttons & PSP_CTRL_RTRIGGER,	250);
	keymap(K_START,				pad.Buttons & PSP_CTRL_START,		250);
	keymap(K_SELECT,			pad.Buttons & PSP_CTRL_SELECT,		250);
	keymap(K_HOLD,				pad.Buttons & PSP_CTRL_HOLD,		0);

	if(sceHprmIsRemoteExist()) {
		unsigned int epkeys;
		sceHprmPeekCurrentKey(&epkeys);
		keymap(K_EP_PLAYPAUSE,	epkeys & PSP_HPRM_PLAYPAUSE,		250);
		keymap(K_EP_FORWARD,	epkeys & PSP_HPRM_FORWARD,			250);
		keymap(K_EP_BACK,		epkeys & PSP_HPRM_BACK,				250);
		keymap(K_EP_VOLUP,		epkeys & PSP_HPRM_VOL_UP,			250);
		keymap(K_EP_VOLDOWN,	epkeys & PSP_HPRM_VOL_DOWN,			250);
		keymap(K_EP_HOLD,		epkeys & PSP_HPRM_HOLD,				0);
	}

	float ax = anubtable[pad.Lx];
	float ay = anubtable[pad.Ly];

	extern cvar_t* gu_flipscreen;
	if(gu_flipscreen && gu_flipscreen->value) {
		ax = -ax;
		ay = -ay;
	}

	switch(anubfunc) {
	case 1 :
		{
			char cmd[64];

			if(ay < -0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+forward %i %i\n", K_ANUB_UP, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-forward %i %i\n", K_ANUB_UP, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay > 0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+back %i %i\n", K_ANUB_DOWN, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-back %i %i\n", K_ANUB_DOWN, keytime);
			}
			Cbuf_AddText(cmd);

			cl.viewangles[YAW] -= ax;
		}
		break;
	case 2 :
		{
			char cmd[64];

			if(ax < -0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+moveleft %i %i\n", K_ANUB_LEFT, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-moveleft %i %i\n", K_ANUB_LEFT, keytime);
			}
			Cbuf_AddText(cmd);

			if(ax > 0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+moveright %i %i\n", K_ANUB_RIGHT, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-moveright %i %i\n", K_ANUB_RIGHT, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay < -0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+forward %i %i\n", K_ANUB_UP, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-forward %i %i\n", K_ANUB_UP, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay > 0.5f) {
				Com_sprintf(cmd, sizeof(cmd), "+back %i %i\n", K_ANUB_DOWN, keytime);
			} else {
				Com_sprintf(cmd, sizeof(cmd), "-back %i %i\n", K_ANUB_DOWN, keytime);
			}
			Cbuf_AddText(cmd);
		}
		break;
	default :
		cl.viewangles[YAW] -= ax;
		if(anubinvert) {
			cl.viewangles[PITCH] -= ay;
		} else {
			cl.viewangles[PITCH] += ay;
		}
		break;
	}

	int i;
	for(i = 0; i < 3; i++) {
		while(cl.viewangles[i] < -180.0f) {
			cl.viewangles[i] += 360.0f;
		}
		while(cl.viewangles[i] > 180.0f) {
			cl.viewangles[i] -= 360.0f;
		}
	}
}
