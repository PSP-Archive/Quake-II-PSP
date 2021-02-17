#include "../client/client.h"

#include <pspiofilemgr.h>

#define MAX_TRACKS 24

//static int num_tracks = 0;
//static int cur_track = 0;
//static char tracks[MAX_TRACKS][MAX_QPATH];

cvar_t* music_enabled;
cvar_t* music_volume;

//static void AudioPlayer_PlayPause_f(void) {
//}

//static void AudioPlayer_Next_f(void) {
//}

//static void AudioPlayer_Prev_f(void) {
//}

void AudioPlayer_Init(void) {
#if 0
	music_enabled = Cvar_Get("music_enabled", "0", CVAR_ARCHIVE);
	music_volume  = Cvar_Get("music_volume", "1", CVAR_ARCHIVE);
	if(!music_enabled->value) {
		return;
	}

	num_tracks = 0;
	cur_track = 0;

	char dirname[MAX_QPATH];
	Com_sprintf(dirname, sizeof(dirname), "%s/music/", FS_Gamedir());

	SceUID dir = sceIoDopen(dirname);
	if(dir < 0) {
		return;
	}

	SceIoDirent dirent;

	while(sceIoDread(dir, &dirent) > 0) {
		if(dirent.d_name[0] == '.') {
			continue;
		}

		if(num_tracks == MAX_TRACKS) {
			break;
		}

		Com_Printf("music track: %s\n", dirent.d_name);
	}

	sceIoDclose(dir);
#endif

//	Cmd_AddCommand("musicplaypause", AudioPlayer_PlayPause_f);
//	Cmd_AddCommand("musicnext", AudioPlayer_Next_f);
//	Cmd_AddCommand("musicprev", AudioPlayer_Prev_f);
}

void AudioPlayer_Shutdown(void) {
//	Cmd_RemoveCommand("musicplaypause");
//	Cmd_RemoveCommand("musicnext");
//	Cmd_RemoveCommand("msuicprev");

#if 0
	memset(tracks, 0, sizeof(tracks));

	num_tracks = 0;
#endif
}

void AudioPlayer_Play(void) {
}

void AudioPlayer_Stop(void) {
}

void AudioPlayer_Resume(void) {
}
