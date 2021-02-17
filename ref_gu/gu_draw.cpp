#include "gu_local.h"

/*
=============
Draw_FindPic
=============
*/
image_t* Draw_FindPic(const char *name) {
	if(name[0] != '/' && name[0] != '\\') {
		char fullname[MAX_QPATH];
		ri.Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		return GU_FindImage(fullname, it_pic);
	} else {
		return GU_FindImage(name+1, it_pic);
	}
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize(int* w, int* h, const char* name) {
	image_t* img = Draw_FindPic(name);
	if(!img) {
		*w = *h = 0;
		return;
	}

	*w = img->width;
	*h = img->height;
}

/*
=============
Draw_Image
=============
*/
void Draw_Image(int x, int y, const image_t* img) {
	GU_Bind(img);

#pragma pack(1)
	typedef struct {
		short s, t;
		short x, y, z;
	} VERT;
#pragma pack()

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = 0;
	v[0].t = 0;
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;
	v[1].s = img->width;
	v[1].t = img->height;
	v[1].x = x + img->width;
	v[1].y = y + img->height;
	v[1].z = 0;

	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic(int x, int y, const char* name) {
	image_t* img = Draw_FindPic(name);
	if(!img) {
		return;
	}

	Draw_Image(x, y, img);
}

/*
=============
Draw_StrechImage
=============
*/
void Draw_StretchImage(int x, int y, int w, int h, image_t* img) {
	GU_Bind(img);

#pragma pack(1)
	typedef struct {
		short s, t;
		short x, y, z;
	} VERT;
#pragma pack()

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = 0;
	v[0].t = 0;
	v[0].x = x;
	v[0].y = x;
	v[0].z = 0;
	v[1].s = img->width;
	v[1].t = img->height;
	v[1].x = x + w;
	v[1].y = y + h;
	v[1].z = 0;
	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}

/*
=============
Draw_StrechPic
=============
*/
void Draw_StretchPic(int x, int y, int w, int h, const char* name) {
	image_t* img = Draw_FindPic(name);
	if(!img) {
		return;
	}

	Draw_StretchImage(x, y, w, h, img);
}

/*
=============
Draw_Raw
=============
*/
void Draw_Raw(int x, int y, int w, int h, int cols, int rows, byte* data) {
	image_t* img = GU_LoadRaw("***cinematic***", cols, rows, data, it_pic);

	if(!gu_cinscale->value) {
		Draw_StretchImage(x + (480 - cols) / 2, y + (272 - rows) / 2, cols, rows, img);
	} else {
		Draw_StretchImage(0, 0, 480, 272, img);
	}

	free(img->alloc);
	memset(img, 0, sizeof(image_t));
}

/*
=============
Draw_Fade
=============
*/
void Draw_FadeScreen(void) {
	sceGuEnable(GU_BLEND);
	sceGuDisable(GU_TEXTURE_2D);

#pragma pack(1)
	typedef struct VERT {
		unsigned short color;
		short x, y, z;
	};
#pragma pack()

	VERT __attribute__((aligned(16))) v[2] = {
		{ 0xC000,   0,   0, 0 },
		{ 0xC000, 480, 272, 0 }
	};

	sceGumDrawArray(GU_SPRITES, GU_COLOR_4444 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);

	sceGuEnable(GU_TEXTURE_2D);
	sceGuDisable(GU_BLEND);
}

/*
=============
Draw_Fill
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c) {
	c &= 0xFF;

	sceGuDisable(GU_TEXTURE_2D);
	sceGuColor(gucontext.stdpalette[c]);

#pragma pack(1)
	typedef struct {
		short x, y, z;
	} VERT;
#pragma pack()

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;
	v[1].x = x + w;
	v[1].y = y + h;
	v[1].z = 0;
	sceGumDrawArray(GU_SPRITES, GU_VERTEX_16BIT, 2, 0, v);

	sceGuEnable(GU_TEXTURE_2D);
}

/*
=============
Draw_Char
=============
*/
void Draw_Char(int x, int y, int num) {
	image_t* img = gucontext.charset;
	if(!img) {
		return;
	}

	num &= 255;
	if((num & 127) == 32) {
		return;
	}
	if(y <= -8) {
		return;
	}

	GU_Bind(img);

#pragma pack(1)
	typedef struct {
		short s, t;
		short x, y, z;
	} VERT;
#pragma pack()

	short row = (num >> 4) << 3;
	short col = (num & 15) << 3;

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);
	v[0].s = col,
	v[0].t = row;
	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;
	v[1].s = col + 8;
	v[1].t = row + 8;
	v[1].x = x + 8;
	v[1].y = y + 8;
	v[1].z = 0;
	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
}


void Draw_Text(int x, int y, const char* text) {
#if 0
	const unsigned char* data = (const unsigned char*)text;
#endif
}
