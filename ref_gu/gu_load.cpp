#include "gu_local.h"

static int memory = 0;
static int memor1 = 0;
static int memor2 = 0;

static int npot(int n) {
	if(n <=    0) return    0;
	if(n <=    1) return    1;
	if(n <=    2) return    2;
	if(n <=    4) return    4;
	if(n <=    8) return    8;
	if(n <=   16) return   16;
	if(n <=   32) return   32;
	if(n <=   64) return   64;
	if(n <=  128) return  128;
	if(n <=  256) return  256;
	if(n <=  512) return  512;
	if(n <= 1024) return 1024;
	return 0;
}

static int ppot(int n) {
	if(n <     1) return    0;
	if(n <     2) return    1;
	if(n <     4) return    3;
	if(n <     8) return    4;
	if(n <    16) return    8;
	if(n <    32) return   16;
	if(n <    64) return   32;
	if(n <   128) return   64;
	if(n <   256) return  128;
	if(n <   512) return  256;
	if(n <  1024) return  512;
	return 0;
}

static unsigned char* align16(unsigned char* ptr) {
	unsigned char* p = (unsigned char*)ptr;
	int off = (int)p & 0xf;
	if(off) {
		p += (~off & 0xf) + 1;
	}
	return p;
}

qboolean GU_LoadPalette(const char* name, unsigned int* palette) {
	unsigned char data[768];

	int fid = ri.FS_Fopen(name);
	if(!fid) {
		ri.Con_Printf(PRINT_ALL, "GU_LoadPalette(%s) file not found\n", name);
		return 0;
	}

	ri.FS_Fseek(fid, -sizeof(data), SEEK_END);
	if(!ri.FS_Fread(fid, data, sizeof(data))) {
		ri.Con_Printf(PRINT_ALL, "GU_LoadPalette(%s) file to short\n", name);
		ri.FS_Fclose(fid);
		return 0;
	}

	int i;
	for(i = 0; i < 255; i++) {
		palette[i] = (0xff << 24)
		| (data[i * 3 + 0] <<  0)
		| (data[i * 3 + 1] <<  8)
		| (data[i * 3 + 2] << 16);
	}
	palette[255] = 0x00ffffff;

	ri.FS_Fclose(fid);

	return 1;
}

image_t* GU_LoadPCX(const char* name, imagetype_t type) {
	if(!name) {
		return 0;
	}

	int fid = ri.FS_Fopen(name);
	if(!fid) {
		//ri.Con_Printf(PRINT_ALL, "GU_LoadPCX(%s) file not found\n", name);
		return 0;
	}

	pcx_t pcx;
	if(!ri.FS_Fread(fid, &pcx, sizeof(pcx))) {
		ri.Con_Printf(PRINT_ALL, "GU_LoadPCX(%s) file to short\n", name);
		ri.FS_Fclose(fid);
		return 0;
	}

	image_t* img = GU_FindUnusedImage();
	if(!img) {
		ri.Sys_Error(ERR_FATAL, "GU_LoadPCX(%s) all image slots filled\n", name);
		ri.FS_Fclose(fid);
		return 0;
	}

	memset(img, 0, sizeof(image_t));
	strncpy(img->name, name, MAX_QPATH);
	img->type = type;
	img->seq = gucontext.seq;

	img->width  = pcx.xmax + 1;
	img->height = pcx.ymax + 1;

	img->widthp2  = npot(img->width);
	img->heightp2 = npot(img->height);
	if(img->widthp2 < 16) {
		img->widthp2 = 16;
	}
	img->linep2   = img->widthp2;

	img->sl = 0.0f;
	img->tl = 0.0f;
	img->sh = 1.0f;
	img->th = 1.0f;

	img->wscale = (float)img->width  / (float)img->widthp2;
	img->hscale = (float)img->height / (float)img->heightp2;

	img->alloc = (unsigned char*)malloc(img->widthp2 * img->heightp2 + 16);
	img->data = (unsigned char*)align16(img->alloc);

	memset(img->data, 0x00, img->widthp2 * img->heightp2);

	unsigned char buffer[4096];
	unsigned char* data = buffer;
	unsigned char* buffer_end = buffer + sizeof(buffer);

	ri.FS_Fread(fid, buffer, sizeof(buffer));

	memory += img->widthp2 * img->heightp2;
	memor1 += img->width * img->height;
	memor2 += ppot(img->width) * ppot(img->height);
//	printf("Load: %s (%d %d) %d %d %d\n", img->name, img->width, img->height, memory, memor1, memor2);

	int x, y;
	for(y = 0; y < img->height; y++) {
		for(x = 0; x < img->width; ) {
			unsigned char rl;
			unsigned char index = *data++;
			if(data == buffer_end) {
				ri.FS_Fread(fid, buffer, sizeof(buffer));
				data = buffer;
			}
			if((index & 0xC0) == 0xC0) {
				rl = index & 0x3F;
				index = *data++;
				if(data == buffer_end) {
					ri.FS_Fread(fid, buffer, sizeof(buffer));
					data = buffer;
				}
			} else {
				rl = 1;
			}
			while(rl-- > 0) {
				img->data[y * img->widthp2 + x++] = index;
			}
		}
	}

	ri.FS_Fclose(fid);
	sceKernelDcacheWritebackAll();

	return img;
}

image_t* GU_LoadWal(const char* name, imagetype_t type) {
	if(!name) {
		return 0;
	}

	int fid = ri.FS_Fopen(name);
	if(!fid) {
		ri.Con_Printf(PRINT_ALL, "GU_LoadWal(%s) file not found\n", name);
		return 0;
	}

	miptex_t wal;
	if(!ri.FS_Fread(fid, &wal, sizeof(wal))) {
		ri.Con_Printf(PRINT_ALL, "GU_LoadWal(%s) file to short\n", name);
		ri.FS_Fclose(fid);
		return 0;
	}

	image_t* img = GU_FindUnusedImage();
	if(!img) {
		ri.Sys_Error(ERR_FATAL, "GU_LoadWal(%s) all image slots filled\n", name);
		ri.FS_Fclose(fid);
		return 0;
	}

	memset(img, 0, sizeof(image_t));
	strncpy(img->name, name, MAX_QPATH);
	img->type = type;
	img->seq = gucontext.seq;

	img->width    = wal.width;
	img->height   = wal.height;
	img->widthp2  = npot(img->width);
	img->heightp2 = npot(img->height);
	img->linep2   = img->widthp2;

	img->sl = 0.0f;
	img->tl = 0.0f;
	img->sh = 1.0f;
	img->th = 1.0f;

	img->wscale = (float)img->width  / (float)img->widthp2;
	img->hscale = (float)img->height / (float)img->heightp2;

#if 0
	img->alloc = (unsigned char*)malloc(img->widthp2 * img->heightp2 + 16);
	img->data  = align16(img->alloc);
#else
	img->alloc = (unsigned char*)malloc(img->widthp2 * img->heightp2);
	img->data  = img->alloc;
#endif

	ri.FS_Fseek(fid, wal.offsets[0], SEEK_SET);

	memory += img->widthp2 * img->heightp2;
	memor1 += img->width * img->height;
	memor2 += ppot(img->width) * ppot(img->height);
//	printf("Load: %s (%d %d) %d %d %d\n", img->name, img->width, img->height, memory, memor1, memor2);

	if(img->width == img->widthp2 && img->height == img->heightp2) {
		ri.FS_Fread(fid, img->data, img->width * img->height);
	} else {
		int y;
		for(y = 0; y < img->height; y++) {
			ri.FS_Fread(fid, img->data + img->widthp2 * y, img->width);
		}
	}

	ri.FS_Fclose(fid);

	sceKernelDcacheWritebackAll();
	return img;
}

image_t* GU_LoadRaw(const char* name, int w, int h, unsigned char* data, imagetype_t type) {
	image_t* img = GU_FindUnusedImage();
	if(!img) {
		return 0;
	}

	memset(img, 0, sizeof(image_t));
	strncpy(img->name, name, MAX_QPATH);
	img->type = type;
	img->seq = gucontext.seq;

	img->width    = w;
	img->height   = h;
	img->widthp2  = npot(img->width);
	img->heightp2 = npot(img->height);
	img->linep2   = img->widthp2;

	img->sl = 0.0f;
	img->tl = 0.0f;
	img->sh = 1.0f;
	img->th = 1.0f;

	img->wscale = (float)img->width  / (float)img->widthp2;
	img->hscale = (float)img->height / (float)img->heightp2;

	img->alloc = (unsigned char*)malloc(img->widthp2 * img->heightp2 + 16);
	img->data  = align16(img->alloc);

	memset(img->data, 0, img->widthp2 * img->heightp2);
	int x, y;
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			img->data[y * img->widthp2 + x] = data[y * w + x];
		}
	}

	sceKernelDcacheWritebackAll();

	return img;
}

#define X 0x0f,
#define o 0xff,

static unsigned char __attribute__((aligned(16))) tex_particle[256] = {
	o o o o o o o o o o o o o o o o
	o o o o o o X X X X o o o o o o
	o o o o X X X X X X X X o o o o
	o o o X X X X X X X X X X o o o
	o o X X X X X X X X X X X X o o
	o o X X X X X X X X X X X X o o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o o X X X X X X X X X X X X o o
	o o X X X X X X X X X X X X o o
	o o o X X X X X X X X X X o o o
	o o o o X X X X X X X X o o o o
	o o o o o o X X X X o o o o o o
	o o o o o o o o o o o o o o o o
};

#undef X
#undef o

image_t* GU_LoadParticle(void) {
	image_t* img = GU_FindUnusedImage();
	if(!img) {
		return 0;
	}
	
	memset(img, 0, sizeof(image_t));
	strncpy(img->name, "***particle***", MAX_QPATH);
	img->type = it_sprite;
	img->seq = gucontext.seq;

	img->width    = 16;
	img->height   = 16;
	img->widthp2  = 16;
	img->heightp2 = 16;
	img->linep2   = 16;

	img->sl = 0.0f;
	img->tl = 0.0f;
	img->sh = 1.0f;
	img->th = 1.0f;

	img->wscale = (float)img->width  / (float)img->widthp2;
	img->hscale = (float)img->height / (float)img->heightp2;

	img->alloc = 0;
	img->data  = tex_particle;

	return img;
}


static unsigned char __attribute__((aligned(16))) tex_notex[128] = {
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


image_t* GU_LoadNoTexture(void) {
	image_t* img = GU_FindUnusedImage();
	if(!img) {
		return 0;
	}
	
	memset(img, 0, sizeof(image_t));
	strncpy(img->name, "***particle***", MAX_QPATH);
	img->type = it_sprite;
	img->seq = gucontext.seq;

	img->width    = 8;
	img->height   = 8;
	img->widthp2  = npot(img->width);
	img->heightp2 = npot(img->height);
	if(img->widthp2 < 16) {
		img->widthp2 = 16;
	}
	img->linep2   = img->widthp2;

	img->sl = 0.0f;
	img->tl = 0.0f;
	img->sh = 1.0f;
	img->th = 1.0f;

	img->wscale = (float)img->width  / (float)img->widthp2;
	img->hscale = (float)img->height / (float)img->heightp2;

	img->alloc = 0;
	img->data  = tex_notex;

	return img;
}
