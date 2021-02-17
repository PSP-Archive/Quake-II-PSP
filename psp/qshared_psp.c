#include "../qcommon/qcommon.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspsdk.h>

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

byte *membase;
int maxhunksize;
int curhunksize;

//#define DEBUG_HUNK

void* Hunk_Begin(int maxsize) {
	maxhunksize = maxsize;
	curhunksize = 0;
	membase = malloc(maxhunksize);
	if(membase == NULL) {
		Sys_Error(ERR_FATAL, "Hunk_Begin: unable to allocate %d bytes", maxsize);
		return 0;
	}

#ifdef DEBUG_HUNK
	printf("Hunk_Begin addr:0x%08x size:%d\n", (unsigned int)membase, maxsize);
#endif

	return membase;
}

void* Hunk_Alloc(int size) {
	byte *buf;
	size = (size+31)&~31;
	if(curhunksize + size > maxhunksize) {
		Sys_Error(ERR_FATAL, "Hunk_Alloc: overflow");
		return 0;
	}
	buf = membase + curhunksize;
	curhunksize += size;

#ifdef DEBUG_HUNK
	printf("Hunk_Alloc addr:0x%08x size:%d\n", (unsigned int)buf, size);
#endif

	return buf;
}

int Hunk_End(void) {
	byte* n = realloc(membase, curhunksize);
	if(n != membase) {
		Sys_Error(ERR_FATAL, "Hunk_End: Could not remap block");
		return 0;
	}

#ifdef DEBUG_HUNK
	printf("Hunk_End   addr:0x%08x size:%d\n", (unsigned int)membase, curhunksize);
#endif

	return curhunksize;
}

void Hunk_Free(void* base) {
	if(base) {
		free(base);
	}
}

/*

*/

cvar_t* sys_cpufreq;

unsigned int sys_frame_time;

void Sys_SendKeyEvents (void) {
	sys_frame_time = Sys_Milliseconds();

	void IN_Frame(void);
	IN_Frame();
}

void Sys_Init(void) {
	sys_cpufreq = Cvar_Get("psp_cpufreq", "222", CVAR_ARCHIVE);
}

void Sys_Quit(void) {
	CL_Shutdown();
	Qcommon_Shutdown();
    sceKernelExitGame();
}

void Sys_Frame(void) {
	if(sys_cpufreq->modified) {
		sys_cpufreq->modified = false;

		if(sys_cpufreq->value <= 111.0f) {
			sys_cpufreq->value = 111.0f;
			scePowerSetClockFrequency(111, 111,  56);
		} else if(sys_cpufreq->value >= 266.0f) {
			sys_cpufreq->value = 333.0f;
			scePowerSetClockFrequency(333, 333, 166);
		} else {
			sys_cpufreq->value = 222.0f;
			scePowerSetClockFrequency(222, 222, 111);
		}
		
		Com_Printf("PSP Cpu Speed : %d MHz   Bus Speed : %d MHz\n",
			scePowerGetCpuClockFrequency(),
			scePowerGetBusClockFrequency()
		);
	}
}

// LIB Loading

static SceUID game_lib = 0;
static SceUID ref_lib  = 0;

void* Sys_GetGameAPI(void* params) {
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	static const char* game_name = "gamepsp.prx";

	if(game_lib) {
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadGame");
	}

	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "%s/%s", FS_Gamedir(), game_name);

	game_lib = sceKernelLoadModule(name, 0, 0);
	if(!game_lib) {
		Com_Error(ERR_FATAL, "Unabled to load %s", name);
	}

	sceKernelStartModule(game_lib, 0, 0, 0, 0);

#else
	game_lib = 1;
#endif

	void* GetGameAPI(void*);

	return GetGameAPI(params);
}

void Sys_UnloadGame(void) {
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	if(game_lib) {
		sceKernelStopModule(game_lib, 0, 0, 0, 0);
		sceKernelUnloadModule(game_lib);
	}
#endif
	game_lib = 0;
}

void* Sys_GetRefAPI(void* params) {
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	static const char* ref_name  = "refpspgu.prx";

	if(ref_lib) {
		Com_Error(ERR_FATAL, "Sys_GetRefAPI without Sys_UnloadGame");
	}

	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "%s", ref_name);

	Com_Printf("REF_LIB: %d\n", ref_lib);

	ref_lib = sceKernelLoadModule(name, 0, 0);
	if(ref_lib <= 0) {
		Com_Printf("REF_LIB: %s %d 0x%08X\n", ref_name, ref_lib, ref_lib);
		Sys_Error("Unabled to load %s", name);
	}
	
	Com_Printf("REF_LIB: %d\n", ref_lib);

	int ref = sceKernelStartModule(ref_lib, 0, 0, 0, 0);
	Com_Printf("RETURN: %d\n", ref);
#else
	ref_lib = 1;
#endif

	void* GetRefAPI(void*);

	return GetRefAPI(params);
}

void Sys_UnloadRef(void) {
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	if(ref_lib) {
		sceKernelStopModule(ref_lib, 0, 0, 0, 0);
		sceKernelUnloadModule(ref_lib);
	}
#endif
	ref_lib = 0;
}
