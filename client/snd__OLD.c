#include "client.h"

int s_inited = 0;

static struct sfx_s s_slots[MAX_SOUNDS];

static struct playsound_s  s_playslots[MAX_PLAYSOUNDS];

static playsound_t* AllocPlaySound() {
	int i;
	for(i = 0; i < MAX_PLAYSOUNDS; i++) {
		if(!s_playslots[i].sound) {
			return &s_playslots[i];
		}
	}
	return 0;
}

void S_Init(void) {
	if(s_inited) {
		return;
	}

	s_inited = 1;

	int i;
	for(i = 0; i < MAX_SOUNDS; i++) {
		S_FreeSound(&s_slots[i]);
	}

	memset(s_playslots, 0, sizeof(s_playslots));

	Sys_S_Init();
}

void S_Shutdown(void) {
	if(!s_inited) {
		return;
	}

	Sys_S_Shutdown();

	s_inited = 0;
}

void S_StartSound(vec3_t origin, int entnum, int entchannel, struct sfx_s* sound, float vol, float attenuation, float timeofs) {
	if(!s_inited || !sound) {
		return;
	}

	if(sound->name[0] == '*') {
		// PSP_FIXME play sexed sound
		return;
	}

	playsound_t* ps = AllocPlaySound();
	if(!ps) {
		return;
	}

	if(origin) {
		VectorCopy(origin, ps->origin);
	}
	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sound = sound;

	ps->begin = cl.frame.servertime;

	printf("Sound: %s\n", sound->name);
}

void S_StartLocalSound(const char* name) {
	if(!s_inited) {
		return;
	}

	struct sfx_s* sound = S_FindName(name, 1);
	if(!sound) {
		return;
	}

	S_StartSound(0, cl.playernum+1, 0, sound, 1.0f, 1.0f, 0.0f);
}

void S_RawSamples(int samples, int rate, int width, int channels, byte *data) {
}

void S_StopAllSounds(void) {
	
}

void S_Update(vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up) {
	if(!s_inited) {
		return;
	}

	if(cls.disable_screen) {
		S_StopAllSounds();
		return;
	}
}

void S_Activate(qboolean active) {
}

void S_BeginRegistration(void) {
}

struct sfx_s* S_RegisterSound(const char* name) {
	return 0;
}

void S_EndRegistration(void) {
}

#pragma pack(1)
typedef struct WaveHeader_s {
	unsigned int   riff;
	unsigned int   filesize;
	unsigned int   wave;

	unsigned int   fmt_;
	unsigned int   _unused0;
	unsigned short _unused2;
	unsigned short channels;
	unsigned int   samples;
	unsigned int   _unused3;
	unsigned short bytesPerSample;
	unsigned short bitsPerSample;
	unsigned int   data;
	unsigned int   length;
} WaveHeader_t;
#pragma pack()

struct sfx_s* S_FindName(const char* name, qboolean create) {
//	printf("TRACE: %s\n", __FUNCTION__);

	if(!name || !name[0] || strlen(name) >= MAX_QPATH) {
		Com_Error(ERR_FATAL, "S_FindName\n");
	}

	struct sfx_s* sound = 0;
	int i;
	for(i = 0; i < MAX_SOUNDS; i++) {
		if(!strcmp(s_slots[i].name, name)) {
			return &s_slots[i];
		} else if(!sound && !s_slots[i].name[0]) {
			sound = &s_slots[i];
		}
	}

	if(!create) {
		return 0;
	}

	if(!sound) {
		Com_Printf("S_FindName: No free slot\n");
		return 0;
	}

	strcpy(sound->name, name);

	char fullname[MAX_QPATH];
	Com_sprintf(fullname, sizeof(fullname), "sound/%s", name);

	int fid = FS_Fopen(fullname);
	if(!fid) {
		Com_Printf("S_FindName: File %s not found\n", fullname);
	}

	struct WaveHeader_s wave;
	FS_Fread(fid, &wave, sizeof(struct WaveHeader_s));


	sound->channels       = wave.channels;
	sound->samplesPerSec  = wave.samples;
	sound->bytesPerSample = wave.bytesPerSample;

	sound->samples = wave.length / (wave.channels * wave.bytesPerSample);
	sound->data = malloc(wave.length);
	FS_Fread(fid, sound->data, wave.length);

	FS_Fclose(fid);

	return sound;
}

void S_FreeSound(struct sfx_s* sound) {
	sound->name[0] = 0;
	if(sound->data) {
		free(sound->data);
		sound->data = 0;
	}
}

void S_Render(sample_t* buffer, unsigned int size) {
#if 0
	int i;
	sample_t* b;
	for(i = 0, b = buffer; i < size; i++, b++) {
		b->left  = 0;
		b->right = 0;
	}
#else
	int i;
	sample_t* b;

	if(!s_inited) {
		for(i = 0, b = buffer; i < size; i++, b++) {
			b->left = 0;
			b->right = 0;
		}
		return;
	}

	for(i = 0, b = buffer; i < size; i++, b++) {
		if(!s_inited) {
			b->left = 0;
			b->right = 0;
		}

		int s;
		for(s = 0; s < MAX_PLAYSOUNDS; s++) {
			playsound_t* ps = &s_playslots[s];

			if(ps->begin + (ps->sound->samples / ps->sound->samplesPerSec) < cl.frame.servertime) {
				ps->sound = 0;
				continue;
			}
		}

		b->left = 0;
		b->right = 0;
	}
#endif
}
