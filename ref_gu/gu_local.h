#include <stdio.h>
#include <math.h>

#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>

#include "../client/ref.h"

typedef union {
	float a[2];
	struct { float x, y; };
	ScePspFVector2 psp;
} GUVec2;

typedef union {
	float a[3];
	struct { float x, y, z; };
	ScePspFVector3 psp;
} GUVec3;

typedef union {
	float a[4];
	struct { float x, y, z, w; };
	ScePspFVector4 psp;
} GUVec4;

typedef union {
	struct {
		GUVec3 nrm;
		float dis;
	};
	GUVec4 vec4;
} GUPlane;

typedef union {
	struct {
		GUVec3 pos;
		float rad;
	};
	GUVec4 vec4;
} GUSphere;

typedef struct {
	GUVec3 org;
	GUVec3 dir;
} GURay;

#define D2R(x) ((x) * (M_PI / 180.0f))
#define R2D(x) ((x) * (180.0f / M_PI))

typedef enum imagetype_e {
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s {
	char name[MAX_QPATH];
	imagetype_t type;
	int seq;
	int width, height;
	int widthp2, heightp2, linep2;
	float sl, tl, sh, th;
	float wscale, hscale;
	unsigned char* alloc;
	unsigned char* data;
	struct msurface_s* texturechain;
} image_t;

typedef struct gucontext_s {
	int seq;
	unsigned int stdpalette[256] __attribute__((aligned(16)));
	unsigned int cinpalette[256] __attribute__((aligned(16)));
	image_t* charset;
	image_t* particle;
	image_t* notex;
	char screenshot[MAX_QPATH];
	unsigned char* primbuffer;
	unsigned char* backbuffer;
	float matrix_world[16];
	float matrix_base[16];
} gucontext_t;

#define	MAX_TEXTURES 1024

extern image_t textures[MAX_TEXTURES];

extern	gucontext_t	gucontext;

extern int r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern refdef_t r_newrefdef;
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

extern cvar_t* gu_texfilter;
extern cvar_t* gu_particletype;
extern cvar_t* gu_subdivide;
extern cvar_t* gu_cinscale;
extern cvar_t* gu_clippingepsilon;

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01f


#define SCREENSHOT_PATH "ms0:/PICTURE/quake2/"


image_t* Draw_FindPic(const char *name);

void Draw_GetPicSize(int* w, int* h, const char* name);
void Draw_Pic(int x, int y, const char* name);
void Draw_Image(int x, int y, const image_t* img);
void Draw_StretchPic(int x, int y, int w, int h, const char* name);
void Draw_StretchImage(int x, int y, int w, int h, const image_t* img);
void Draw_Raw(int x, int y, int w, int h, int cols, int rows, byte* data);
void Draw_TileClear(int x, int y, int w, int h, const char* name);
void Draw_Char(int x, int y, int num);
void Draw_Text(int x, int y, const char* text);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_FadeScreen(void);

/*
 *
 */

void R_RotateForEntity(entity_t* e);

void R_DrawAliasModel(entity_t* e);

void R_DrawBrushModel(entity_t* e);

/**
 *
 */
 
void GU_View2D(void);
void GU_View3D(void);

void GU_DefaultState(void);

void GU_Bind(const image_t* img);
void GU_BindPalette(const unsigned char* palette);

void GU_Screenshot(const char* filename);

void GU_RotateForEntity(entity_t* e);

image_t* GU_FindUnusedImage(void);
image_t* GU_FindImage(const char* name, imagetype_t);

qboolean GU_LoadPalette(const char* name, unsigned int* palette);
image_t* GU_LoadPCX(const char* name, imagetype_t);
image_t* GU_LoadWal(const char* name, imagetype_t);
image_t* GU_LoadRaw(const char* name, int x, int y, unsigned char* data, imagetype_t type);
image_t* GU_LoadParticle(void);
image_t* GU_LoadNoTexture(void);

void R_BeginRegistration(const char* world);
void R_EndRegistration(void);

struct image_s* R_RegisterSkin(const char *name);
struct model_s* R_RegisterModel(const char *name);
void            R_RegisterSky(const char* name, float rotate, vec3_t axis);

void R_InitImages(void);
void R_ShutdownImages(void);

void GU_ImageList_f(void);

void R_DrawSky(void);
void R_ClearSky(void);

void R_DrawParticles(void);

void R_Screenshot_f(void);

void R_MarkLeaves(void);
void R_DrawWorld(void);


extern refimport_t ri;
