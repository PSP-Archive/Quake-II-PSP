#include "gu_local.h"

cvar_t* gu_texfilter;

void GU_DefaultState(void) {
	sceGuClearColor(0xff000000);
	sceGuFrontFace(GU_CCW);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuDisable(GU_BLEND);

	sceGuEnable(GU_COLOR_TEST);
	sceGuColorFunc(GU_NOTEQUAL, 0xffffff, 0xffffff);

	sceGuColor(0xffffff);

	sceGuAmbientColor(0xffffffff);
	
	sceGuShadeModel(GU_FLAT);

	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

void GU_Bind(const image_t* img) {
	sceGuTexMode(GU_PSM_T8, 0, 0, 0);
	sceGuTexImage(0, img->widthp2, img->heightp2, img->linep2, img->data);
	sceGuTexEnvColor(0xffffffff);
	sceGuTexScale(img->wscale, img->hscale);
	sceGuTexOffset(0.0f, 0.0f);
	if(img->type == it_wall) {
		sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	} else {
		sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	}
	if(img->type == it_sprite) {
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	} else {
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	}

	if(img->type != it_pic && gu_texfilter->value) {
		sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	} else {
		sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	}
}

void GU_BindPalette(const unsigned char* palette) {
	if(palette) {
		memcpy(gucontext.cinpalette, palette, sizeof(unsigned int) * 256);
		sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
		sceGuClutLoad((256 / 8), gucontext.cinpalette);
	} else {
		sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
		sceGuClutLoad((256 / 8), gucontext.stdpalette);
	}
}

image_t* GU_FindUnusedImage(void) {
	int i;
	for(i = 0; i < MAX_TEXTURES; i++) {
		if(textures[i].seq == 0) {
			return &textures[i];
		}
	}
	return 0;
}

#pragma pack(1)
typedef struct tga_s {
	unsigned char id;
	unsigned char cmaptype;
	unsigned char imagetype;
	
	unsigned short cmapstart;
	unsigned short cmaplength;
	unsigned char  cmapbits;
	
	unsigned short xstart;
	unsigned short ystart;
	unsigned short width;
	unsigned short height;
	unsigned char  bits;
	unsigned char  desc;
} tga_t;
#pragma pack()

#pragma pack(1)
typedef struct bmp_s {
	unsigned short id;
	unsigned int   filesize;
	unsigned int   reserved0;
	unsigned int   offset;

	unsigned int   infosize;
	int			   width;
	int            height;
	unsigned short planes;
	unsigned short bpp;

	unsigned int   compression;
	unsigned int   imagesize;
	int            xres;
	int            yres;
	unsigned int   cltUsed;
	unsigned int   cltImportant;
} bmp_t;
#pragma pack()

void GU_Screenshot(const char* filename) {
#if 0
	FILE* fh = fopen(filename, "w+");
	if(!fh) {
		return;
	}
	
	static const tga_t tgaheader = {
		0, 0, 2,
		0, 0, 0,
		0, 0, 480, 272, 24, 32
	};
	fwrite(&tgaheader, 1, sizeof(tgaheader), fh);
	
	unsigned char buffer[480 * 3];
	unsigned int* vram = (unsigned int*)gucontext.primbuffer;
	
	for(int y = 0; y < 272; y++) {
		for(int x = 0; x < 480; x++) {
			unsigned int color = vram[y * 512 + x];
			buffer[x * 3 + 0] = (color & 0xFF0000) >> 16;
			buffer[x * 3 + 1] = (color & 0x00FF00) >>  8;
			buffer[x * 3 + 2] = (color & 0x0000FF) >>  0;
		}
		fwrite(buffer, 1, sizeof(buffer), fh);
	}
	
	fclose(fh);
#else
	FILE* fh = fopen(filename, "w+");
	if(!fh) {
		return;
	}

	static const bmp_t bmpheader = {
		19778, 54 + (480 * 272 * 3), 0, 54,
		40, 480, 272, 1, 24,
		0, (480 * 272 * 3), 2834, 2834, 0, 0
	};
	fwrite(&bmpheader, 1, sizeof(bmpheader), fh);

	unsigned char buffer[480*3];
	unsigned int* vram = (unsigned int*)gucontext.primbuffer;

	for(int y = 271; y >= 0; y--) {
		for(int x = 0; x < 480; x++) {
			unsigned int color = vram[y * 512 + x];
			buffer[x * 3 + 0] = (color & 0xFF0000) >> 16;
			buffer[x * 3 + 1] = (color & 0x00FF00) >>  8;
			buffer[x * 3 + 2] = (color & 0x0000FF) >>  0;
		}
		fwrite(buffer, 1, sizeof(buffer), fh);
	}

	fclose(fh);
#endif

	//ri.Con_Printf(PRINT_ALL, "Taken screenshot %s\n", filename);
}

void GU_RotateForEntity(entity_t* e) {
	ScePspFVector3 trn = {
		e->origin[0], 
		e->origin[1],
		e->origin[2]
	};
	ScePspFVector3 rot = {
		-D2R(e->angles[2]),
		-D2R(e->angles[0]),
		 D2R(e->angles[1])
	};
	sceGumTranslate(&trn);
	sceGumRotateZYX(&rot);
}
