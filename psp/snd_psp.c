#include "../client/client.h"
#include "../client/snd_loc.h"

#include <pspkernel.h>
#include <pspaudio.h>

#define AUDIO_SAMPLES 1024

static int audioThreadHandle = -1;
static int audioChannel = 0;

typedef struct sample_s {
	short left;
	short right;
} sample_t;

#define AUDIO_DMA_SAMPLES 16384

static struct sample_s audioBuffer[2][AUDIO_SAMPLES];
static struct sample_s audioDMABuffer[AUDIO_DMA_SAMPLES];

static volatile int audioSamplesRead = 0;

static void audioCopy(const sample_t* first, const sample_t* last, sample_t* dst) {
	const sample_t* src;
	for(src = first; src != last; ++src) {
   		const sample_t sample = *src;
		*dst++ = sample;
    	*dst++ = sample;
	}
}

static int audioThread(int args, void* argp) {
	volatile int index = 0;

	while(1) {
		sample_t* buffer = audioBuffer[index];

		const unsigned int read = AUDIO_SAMPLES / 2;
		const unsigned int rest = AUDIO_DMA_SAMPLES - audioSamplesRead;

		const sample_t* dmabuffer = &audioDMABuffer[audioSamplesRead];

		if(read > rest) {
			audioCopy(dmabuffer, dmabuffer + rest, &buffer[0]);
			audioCopy(&audioDMABuffer[0], &audioDMABuffer[0] + (read - rest), &buffer[rest * 2]);
		} else {
			audioCopy(dmabuffer, dmabuffer + read, &buffer[0]);
		}

		audioSamplesRead = (audioSamplesRead + read) % AUDIO_DMA_SAMPLES;

		sceAudioOutputPannedBlocking(audioChannel, 0x8000, 0x8000, buffer);

		index = (index ? 0 : 1);
	}

	sceKernelExitThread(0);
	return 0;
}

qboolean SNDDMA_Init(void) {
	return 0;

	if(audioThreadHandle != -1) {
		return 1;
	}

	dma.buffer     = (byte*)audioDMABuffer;
	dma.channels   = 2;
	dma.samplebits = 16;
	dma.samplepos  = 0;
	dma.speed      = 22050;
	dma.samples    = AUDIO_DMA_SAMPLES * 2;
	dma.submission_chunk = 1;

	audioChannel = sceAudioChReserve(-1, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_STEREO);

	audioThreadHandle = sceKernelCreateThread("quake2audio", (void*)&audioThread, 0x12, 0x10000, 0, 0);
	if(audioThreadHandle < 0) {
		audioThreadHandle = -1;
		Com_Printf("PSP Audio Thread init failed\n");
		return 0;
	}
	int rc = sceKernelStartThread(audioThreadHandle, 0, 0);
	if(rc != 0) {
		sceKernelDeleteThread(audioThreadHandle);
		audioThreadHandle = -1;
		Com_Printf("PSP Audio Thread start failed\n");
		return 0;
	}

	audioSamplesRead = 0;

	return 1;
}

void SNDDMA_Shutdown(void) {
	if(audioThreadHandle == -1) {
		return;
	}

	memset(audioDMABuffer, 0, sizeof(audioDMABuffer));

	sceKernelDeleteThread(audioThreadHandle);
	audioThreadHandle = -1;

	sceKernelDelayThread(100000);

	sceAudioChRelease(audioChannel);
}

int SNDDMA_GetDMAPos() {
	return audioSamplesRead * 2;
}

void SNDDMA_BeginPainting(void) {
}

void SNDDMA_Submit(void) {
}
