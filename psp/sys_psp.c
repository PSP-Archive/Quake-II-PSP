#include "../qcommon/qcommon.h"

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <psppower.h>

#include <time.h>

void Sys_Error(char* error, ...) {
	va_list argptr;
    char string[1024];

	va_start(argptr,error);
    vsprintf(string,error,argptr);
    va_end(argptr);

	Sys_ConsoleOutput(string);

	CL_Shutdown();
	Qcommon_Shutdown();

	sceKernelExitGame();
}

void Sys_ConsoleOutput(char* string) {
	printf("%s", string);
}

int curtime;
int Sys_Milliseconds() {
	 return (curtime = ((unsigned int)clock()) / 1000);
}

static int running = 0;
static int exit_callback(int arg1, int arg2, void *common) {
	running = 0;
	sceKernelDelayThread(3000000);
	sceKernelExitGame();
	return 0;
}

static int CallbackThread(SceSize args, void *argp) {
	int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

static int quake2(SceSize args, void* argp) {
	pspSdkInetInit();

	int   cl_argc = 1;
	char* cl_argv[2] = { (char*)argp, 0 };

	// Main Quake Init Call
	Qcommon_Init(cl_argc, cl_argv);

	int cur_time = 0;
	int old_time = 0;
	int new_time = 0;
	
	running = 1;

	old_time = Sys_Milliseconds();
	while(running) {
		new_time = Sys_Milliseconds();
		cur_time = new_time - old_time;
		while(cur_time <= 0) {
			new_time = Sys_Milliseconds();
			cur_time = new_time - old_time;
			sceKernelDelayThread(500);
		}

		old_time = new_time;

		{
			// Main Quake Frame Call
			Qcommon_Frame(cur_time);
		}
	}

	CL_Shutdown();
	Qcommon_Shutdown();

	sceKernelExitGame();

	return 0;
}

PSP_MODULE_INFO("quake2psp", 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

int main(int argc, char** argv) {
	pspSdkLoadInetModules();

	SceUID mdid;

	SceUID thid;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	
	thid = sceKernelCreateThread("quake2", quake2, 0x18, 0x100000, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, 0);
	if(thid >= 0) {
		sceKernelStartThread(thid, strlen(argv[0])+1, argv[0]);
	}

	sceKernelExitDeleteThread(0);

	return 0;
}
