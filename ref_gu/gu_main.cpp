#include "gu_local.h"
#include "gu_model.h"

#ifdef PSP_USESONYOSK
#include "../psp/psputilitycommon.h"
#include "../psp/psputilityosk.h"
#endif

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2)

static unsigned char __attribute__((aligned(16))) list[1024 * 1024];

refimport_t ri;

image_t textures[MAX_TEXTURES];

model_t* r_worldmodel;

gucontext_t	gucontext;

refdef_t r_newrefdef;
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

entity_t* currententity;
model_t*  currentmodel;

cplane_t frustum[4];

GUPlane gu_cplanes[4];

int r_visframecount = 0;
int r_framecount = 0;

float v_blend[4];

cvar_t* gu_vsync;
cvar_t* gu_hand;
cvar_t* gu_gamma;
cvar_t* gu_particletype;
cvar_t* gu_subdivide;
cvar_t* gu_cinscale;
cvar_t* gu_flipscreen = 0; // =0 is important
cvar_t* gu_clippingepsilon;

int	r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

static qboolean r_colorclear = true;

int R_Init(void) {
	memset(&gucontext, 0, sizeof(gucontext));

	sceGuInit();
	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
	sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);
	sceGuDepthRange(50000, 10000);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuClearDepth(0);
	sceGuFinish();
	sceGuSync(0,0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	GU_DefaultState();

	ri.Cmd_AddCommand("screenshot", R_Screenshot_f);

	gu_hand          = ri.Cvar_Get("hand", "0", CVAR_ARCHIVE | CVAR_USERINFO);
	gu_vsync         = ri.Cvar_Get("gu_vsync", "1", CVAR_ARCHIVE);
	gu_texfilter     = ri.Cvar_Get("gu_texfilter", "0", CVAR_ARCHIVE);
	gu_gamma         = ri.Cvar_Get("gu_gamma", "1", CVAR_ARCHIVE);
	gu_particletype  = ri.Cvar_Get("gu_particletype", "1", CVAR_ARCHIVE);
	gu_subdivide     = ri.Cvar_Get("gu_subdivide", "1", CVAR_ARCHIVE);
	gu_cinscale      = ri.Cvar_Get("gu_cinscale", "1", CVAR_ARCHIVE);
	gu_flipscreen    = ri.Cvar_Get("gu_flipscreen", "0", CVAR_ARCHIVE);
	gu_clippingepsilon = ri.Cvar_Get("gu_clippinge", "0", CVAR_ARCHIVE);

	R_InitImages();

	Mod_Init();

	return 1;
}

void R_Shutdown(void) {
	ri.Cmd_RemoveCommand("screenshot");

	Mod_Shutdown();

	R_ShutdownImages();

	sceGuTerm();
	
	memset(&gucontext, 0, sizeof(gucontext));
}

void R_BeginFrame(float ____unused) {
	sceGuStart(GU_DIRECT, list);

	GU_DefaultState();

	GU_View2D();
}

void R_EndFrame(void) {
	sceGuFinish();
	sceGuSync(0,0);
	
#if (defined(PSP_PSPOSK) && PSP_PSPOSK > 0)
	if(sceUtilityOskGetStatus() == OSK_VISIBLE) {
		sceUtilityOskUpdate(2);
	}
#endif

	if(gucontext.screenshot[0]) {
		GU_Screenshot(gucontext.screenshot);
		gucontext.screenshot[0] = 0;
	}
	
	if(gu_vsync->value) {
		sceDisplayWaitVblankStart();
	}

	gucontext.backbuffer = (unsigned char*)((unsigned int)sceGuSwapBuffers() + (unsigned int)sceGeEdramGetAddr());
	gucontext.primbuffer = (unsigned char*)((unsigned int)gucontext.backbuffer ^ 0x88000);
}

void R_ClearScreen(int flags) {
	if(r_colorclear) {
		flags |= GU_COLOR_BUFFER_BIT;
	}
	
	sceGuClear(flags);
	
	r_colorclear = true;
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame(void) {
	int i;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy(r_newrefdef.vieworg, r_origin);

	AngleVectors(
		/*(ScePspFVector3*)*/r_newrefdef.viewangles,
		/*(ScePspFVector3*)*/vpn,
		/*(ScePspFVector3*)*/vright,
		/*(ScePspFVector3*)*/vup
	);

	if(!(r_newrefdef.rdflags & RDF_NOWORLDMODEL)) {
		mleaf_t	*leaf;
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if(!leaf->contents) { // look down a bit
			vec3_t temp;
			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2)) {
				r_viewcluster2 = leaf->cluster;
			}
		} else { // look up a bit
			vec3_t temp;
			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf(temp, r_worldmodel);
			if(!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2)) {
				r_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i=0 ; i<4 ; i++) {
		v_blend[i] = r_newrefdef.blend[i];
	}

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) {
		sceGuEnable(GU_SCISSOR_TEST);
		sceGuClearColor(0xff666666);
		sceGuScissor(r_newrefdef.x, 272 - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuClearColor(0);
		sceGuDisable(GU_SCISSOR_TEST);
	}
}

int SignbitsForPlane (cplane_t *out) {
	int	bits, j;
	bits = 0;
	for(j = 0; j < 3; j++) {
		if(out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

void R_SetFrustum(void) {
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );

	for(int i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);

		gu_cplanes[i].nrm.x =  frustum[i].normal[0];
		gu_cplanes[i].nrm.y =  frustum[i].normal[1];
		gu_cplanes[i].nrm.z =  frustum[i].normal[2];
		gu_cplanes[i].dis   = -frustum[i].dist;
	}
}

void R_DrawNullModel(void) {
	vec3_t shadelight;

	//if(currententity->flags & RF_FULLBRIGHT) {
		shadelight[0] = 1.0f;
		shadelight[1] = 1.0f;
		shadelight[2] = 1.0f;
	//} else {
		//R_LightPoint(currententit->origin, shadelight);
	//}

	sceGumPushMatrix();
	sceGuDisable(GU_TEXTURE_2D);

	GU_RotateForEntity(currententity);

	typedef struct VERT_t {
		float x, y, z;
	} VERT;

	VERT* v;

	sceGuColor(0x0099FF);
	v = (VERT*)sceGuGetMemory(sizeof(VERT) * 6);
	v[0].x =  0.0f; v[0].y =  0.0f; v[0].z =  9.0f;
	v[1].x =  9.0f; v[1].y =  0.0f; v[1].z =  0.0f;
	v[2].x =  0.0f; v[2].y = -9.0f; v[2].z =  0.0f;
	v[3].x = -9.0f; v[3].y =  0.0f; v[3].z =  0.0f;
	v[4].x =  0.0f; v[4].y =  9.0f; v[4].z =  0.0f;
	v[5].x =  9.0f; v[5].y =  0.0f; v[5].z =  0.0f;
	sceGumDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, v);

	sceGuColor(0x0000FF);
	v = (VERT*)sceGuGetMemory(sizeof(VERT) * 6);
	v[0].x =  0.0f; v[0].y =  0.0f; v[0].z = -9.0f;
	v[1].x =  9.0f; v[1].y =  0.0f; v[1].z =  0.0f;
	v[2].x =  0.0f; v[2].y =  9.0f; v[2].z =  0.0f;
	v[3].x = -9.0f; v[3].y =  0.0f; v[3].z =  0.0f;
	v[4].x =  0.0f; v[4].y = -9.0f; v[4].z =  0.0f;
	v[5].x =  9.0f; v[5].y =  0.0f; v[5].z =  0.0f;
	sceGumDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, v);

	sceGuColor(0xFFFFFF);
	sceGuEnable(GU_TEXTURE_2D);
	sceGumPopMatrix();
}

#define NUM_BEAM_SEGS 8

void R_DrawBeam(entity_t* e) {
	vec3_t dir, dirnormal;
	vec3_t origin;
	vec3_t oldorigin;

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	dirnormal[0] = dir[0] = oldorigin[0] - origin[0];
	dirnormal[1] = dir[1] = oldorigin[1] - origin[1];
	dirnormal[2] = dir[2] = oldorigin[2] - origin[2];

	if(VectorNormalize(dirnormal) == 0) {
		return;
	}

	vec3_t perpvec;
	PerpendicularVector(perpvec, dirnormal);
	VectorScale(perpvec, e->frame / 2, perpvec);

	vec3_t start[NUM_BEAM_SEGS];
	vec3_t end[NUM_BEAM_SEGS];

	int i;
	for(i = 0; i < NUM_BEAM_SEGS; i++) {
		RotatePointAroundVector(start[i], dirnormal, perpvec, (360.0f / (float)NUM_BEAM_SEGS) * (float)i);
		VectorAdd(start[i], origin, start[i]);
		VectorAdd(start[i], dir, end[i]);
	}

	unsigned int color = (gucontext.stdpalette[e->skinnum & 0xff] & 0xffffff) | (((unsigned int)(e->alpha * 255)) << 24);

	sceGuColor(0xffffff);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_BLEND);

	typedef struct {
		unsigned int color;
		float x, y, z;
	} VERT;

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * NUM_BEAM_SEGS * 4);

	for(i = 0; i < NUM_BEAM_SEGS; i++) {
		VERT* v0 = &v[i * 4];
		v0->x = start[i][0];
		v0->y = start[i][1];
		v0->z = start[i][2];
		v0->color = color;

		v0++;
		v0->x = end[i][0];
		v0->y = end[i][1];
		v0->z = end[i][2];
		v0->color = color;

		v0++;
		v0->x = start[(i+1)%NUM_BEAM_SEGS][0];
		v0->y = start[(i+1)%NUM_BEAM_SEGS][1];
		v0->z = start[(i+1)%NUM_BEAM_SEGS][2];
		v0->color = color;

		v0++;
		v0->x = end[(i+1)%NUM_BEAM_SEGS][0];
		v0->y = end[(i+1)%NUM_BEAM_SEGS][1];
		v0->z = end[(i+1)%NUM_BEAM_SEGS][2];
		v0->color = color;
	}

	sceGumDrawArray(GU_TRIANGLE_STRIP,
		GU_VERTEX_32BITF | GU_COLOR_8888 | GU_TRANSFORM_3D,
		NUM_BEAM_SEGS * 4, 0, v
	);

	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_TEXTURE_2D);
}

void R_DrawSpriteModel(entity_t* e) {
	dsprite_t* psprite = (dsprite_t*)currentmodel->extradata;
	e->frame %= psprite->numframes;
	dsprframe_t* frame = &psprite->frames[e->frame];

	float* up = vup;
	float* right = vright;

	unsigned int color;
	if(e->flags & RF_TRANSLUCENT) {
		color = 0xffffff | (((unsigned int)(e->alpha * 255)) << 24);
	} else {
		color = 0xffffffff;
	}

	sceGuColor(color);

	sceGuEnable(GU_BLEND);

	GU_Bind(currentmodel->skins[e->frame]);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);

	vec3_t point;

	typedef struct {
		float s, t;
	//	unsigned int color;
		float x, y, z;
	} VERT;

	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * 2);

	VectorMA(e->origin, -frame->origin_y, up, point);
	VectorMA(point, -frame->origin_x, right, point);
	//v[0].color = color;
	v[0].s = 0.01f;
	v[0].t = 0.99f;
	v[0].x = point[0];
	v[0].y = point[1];
	v[0].z = point[2];

	VectorMA(e->origin, frame->height - frame->origin_y, up, point);
	VectorMA(point, frame->width - frame->origin_x, right, point);
	//v[1].color = color;
	v[1].s = 0.99f;
	v[1].t = 0.01f;
	v[1].x = point[0];
	v[1].y = point[1];
	v[1].z = point[2];

	sceGumDrawArray(GU_SPRITES,
		GU_TEXTURE_32BITF | GU_VERTEX_32BITF/* | GU_COLOR_8888*/ | GU_TRANSFORM_3D,
		2, 0, v
	);

	sceGuDisable(GU_BLEND);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList(void) {
	int i;
	for(i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];

		if(currententity->flags & RF_TRANSLUCENT) {
			continue;
		}

#if 0 // REMOVE fixes bug already is in original engine
		if(currententity->flags & RF_BEAM) {
			R_DrawBeam(currententity);
			continue;
		}
#endif

		currentmodel = currententity->model;
		if(!currentmodel) {
			R_DrawNullModel();
			continue;
		}

		switch(currentmodel->type) {
		case mod_alias:
#if 0
			Md2_Draw(currententity);
#else
			R_DrawAliasModel(currententity);
#endif
			break;
			
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;

		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;

		default:
			ri.Sys_Error(ERR_DROP, "Bad modeltype");
			break;
		}
	}

	sceGuDepthMask(GU_TRUE);

	for(i = 0; i < r_newrefdef.num_entities; i++) {
		currententity = &r_newrefdef.entities[i];

		if(!(currententity->flags & RF_TRANSLUCENT)) {
			continue;
		}

		if(currententity->flags & RF_BEAM) {
			R_DrawBeam(currententity);
			continue;
		}

		currentmodel = currententity->model;
		if(!currentmodel) {
			R_DrawNullModel();
			continue;
		}

		switch(currentmodel->type) {
		case mod_alias:
#if 0
			Md2_Draw(currententity);
#else
			R_DrawAliasModel(currententity);
#endif
			break;
			
		case mod_brush:
			R_DrawBrushModel(currententity);
			break;

		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;

		default:
			ri.Sys_Error(ERR_DROP, "Bad modeltype");
			break;
		}
	}

	sceGuDepthMask(GU_FALSE);
}

/*
================
R_RenderView
================
*/
void R_RenderView(refdef_t* fd) {
#if 0 // PSP_REMOVE
	if(r_norefresh->value)
		return;
#endif

	r_newrefdef = *fd;

#if 0 // PSP_REMOVE
	if(!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Sys_Error (ERR_DROP, "R_RenderView: NULL worldmodel");
#endif

#if 0 // PSP_REMOVE speed messaruement
	if(r_speeds->value) {
		c_brush_polys = 0;
		c_alias_polys = 0;
	}
#endif

#if 0 // PSP_REMOVE
	R_PushDlights();
#endif

	R_SetupFrame();

	R_SetFrustum();

	GU_View3D();

	R_MarkLeaves();

	R_DrawWorld();

	R_DrawEntitiesOnList();

	void R_DrawAlphaSurfaces();
	R_DrawAlphaSurfaces();
	
	R_DrawParticles();
	
#if 0
	ri.Con_Printf(PRINT_ALL, "%+6.2f %+6.2f %+6.2f\n%+6.2f %+6.2f %+6.2f\n",
		r_newrefdef.viewangles[2], r_newrefdef.viewangles[1], r_newrefdef.viewangles[0],
		r_newrefdef.vieworg[2],    r_newrefdef.vieworg[1],    r_newrefdef.vieworg[0]
	);
#endif

#if 0
	{
		static int i = 0;
		char name[128];
		sprintf(name, "video/video%06d.bmp", i++);
		GU_Screenshot(name);
	}
#endif
}

/*
================
GU_View2D
================
*/
void GU_View2D(void) {
	sceGuOffset(2048 - (480/2), 2048 - (272/2));
	sceGuViewport(2048, 2048, 480, 272);
	
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(0, 0, 480, 272);

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	if(gu_flipscreen->value) {
		sceGumOrtho(480.0f, 0.0f, 0.0f, 272.0f, -1000.0f, 1000.0f);
	} else {
		sceGumOrtho(0.0f, 480.0f, 272.0f, 0.0f, -1000.0f, 1000.0f);
	}

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	
	sceGuDisable(GU_CULL_FACE);

	sceGuEnable(GU_COLOR_TEST);
}

/*
================
GU_View3D
================
*/
void GU_View3D(void) {
	int x, x2, y2, y, w, h;

	x  = (int)floorf(r_newrefdef.x * 480 / 480);
	x2 = (int)ceilf((r_newrefdef.x + r_newrefdef.width) * 480 / 480);
	y  = (int)floorf(272 - r_newrefdef.y * 272 / 272);
	y2 = (int)ceilf(272 - (r_newrefdef.y + r_newrefdef.height) * 480 / 480);

	w = x2 - x;
	h = y - y2;

	sceGuOffset(2048 - (w/2), 2048 - (h/2));
	sceGuViewport(2048 + x, 2048 + y2, w, h);
	
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuScissor(x, y2, w, h);

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(r_newrefdef.fov_y, (float)r_newrefdef.width/r_newrefdef.height, 4.0f, 4096.0f);
	if(gu_flipscreen->value) {
		sceGumRotateZ(D2R(180.0f));
	}

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
	//ScePspFVector3 scl = { .a = { -1.0f, -1.0f, 1.0f } };
	//sceGumScale(&scl);

	// ( +x+y+z ) => ( +x+z-y ) => ( -y+z-x )
	ScePspFVector3 rot1 = { D2R(-90.0f), 0.0f, D2R(90.0f) };
	sceGumRotateXYZ(&rot1);

	// global rotation
	ScePspFVector3 rot2 = { D2R(-r_newrefdef.viewangles[2]), D2R(-r_newrefdef.viewangles[0]), D2R(-r_newrefdef.viewangles[1]) };
	sceGumRotateXYZ(&rot2);

	// global translation
	ScePspFVector3 trn = { -r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2] };
	sceGumTranslate(&trn);

	sceGuEnable(GU_DEPTH_TEST);
	
	sceGuEnable(GU_CULL_FACE);	
	sceGuFrontFace(GU_CW);

	sceGuDisable(GU_COLOR_TEST);
}

/*
================
R_RenderFrame
================
*/
void R_RenderFrame(refdef_t *fd) {
	R_RenderView(fd);
	//R_SetLightLevel();
	GU_View2D();
	GU_DefaultState();
	
	r_colorclear = false;
}

void R_Screenshot_f(void) {
	char name[MAX_OSPATH];

	ri.FS_CreatePath(SCREENSHOT_PATH);

	if(ri.Cmd_Argc() == 1) {
		int i;
		for(i = 0; i < 10000; i++) {
			ri.Com_sprintf(name, sizeof(name), SCREENSHOT_PATH"screenshot%02d.bmp", i);
			
			FILE* f = fopen(name, "r");
			if(!f) {
				break;
			}
			fclose(f);
		}
		if(i == 10000) {
			ri.Con_Printf(PRINT_ALL, "No valid screenshot file\n");
			return;
		}
	} else {
		ri.Com_sprintf(name, sizeof(name), SCREENSHOT_PATH"%s.bmp", ri.Cmd_Argv(1));
	}

	strncpy(gucontext.screenshot, name, sizeof(gucontext.screenshot) - 1);
}



/*
%%%%

%%%%
*/

extern "C" refexport_t GetRefAPI(refimport_t rimp) {
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.EndRegistration = R_EndRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;

	re.RegisterSky = R_RegisterSky;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawText = Draw_Text;
	re.DrawTileClear = Draw_StretchPic;
	re.DrawFill = Draw_Fill;

	re.DrawFadeScreen = Draw_FadeScreen;

	re.DrawStretchRaw = Draw_Raw;
	re.CinematicSetPalette = GU_BindPalette;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = R_EndFrame;
	re.ClearScreen = R_ClearScreen;

	return re;
}

#if defined(PSP_DYNAMIC_LINKING)

#include <pspkernel.h>

PSP_MODULE_INFO("q2librefgu", 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU);

int main(int argc, char** argv) {
	sceKernelSleepThread();
	return 0;
}

#endif
