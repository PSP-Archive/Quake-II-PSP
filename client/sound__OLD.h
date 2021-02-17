
#define MAX_PLAYSOUNDS 32

typedef struct sfx_s {
	char name[MAX_QPATH];
	unsigned short samplesPerSec;
	unsigned char channels;
	unsigned char bytesPerSample;
	unsigned int samples;
	char* data;
} sfx_t;

typedef struct playsound_s {
	struct sfx_s* sound;
	float volume;
	float attenuation;
	int entnum;
	int entchannel;
	vec3_t origin;
	int begin;
} playsound_t;

#pragma pack(1)
typedef struct sample_s {
	short left;
	short right;
} sample_t;
#pragma pack()

extern int s_inited;

//extern struct sfx_s s_slots[MAX_SOUNDS];
//extern struct sfx_s s_playslots[MAX_PLAYSOUNDS];

void S_Init(void);
void S_Shutdown(void);

void Sys_S_Init(void);
void Sys_S_Shutdown(void);

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound(vec3_t origin, int entnum, int entchannel, struct sfx_s* sfx, float fvol,  float attenuation, float timeofs);
void S_StartLocalSound(const char* s);

void S_RawSamples(int samples, int rate, int width, int channels, byte *data);

void S_StopAllSounds(void);
void S_Update(vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);

void S_Activate(qboolean active);

void S_BeginRegistration (void);
struct sfx_s* S_RegisterSound(const char* name);
void S_EndRegistration (void);

struct sfx_s* S_FindName(const char *name, qboolean create);

void S_FreeSound(sfx_t* sound);

//void CL_GetEntitySoundOrigin(int ent, vec3_t org);

void S_Render(struct sample_s* buffer, unsigned int size);
