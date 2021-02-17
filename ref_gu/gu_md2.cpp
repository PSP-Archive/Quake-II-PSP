#if 0

#include "gu_local.h"
#include "gu_model.h"

#define MD2_VERTEXNORMALS 162

ScePspFVector3 md2_normals[MD2_VERTEXNORMALS] = {
#include "anorms.h"
};

typedef struct md2_header_s {
	int ident;
	int version;

	int skinwidth;
	int skinheight;
	int framesize;

	int num_skins;
	int num_xyz;
	int num_st;
	int num_tris;
	int num_cmds;
	int num_frames;

	int ofs_skins;
	int ofs_st;
	int ofs_tris;
	int ofs_frames;
	int ofs_cmds;
	int ofs_end;
} md2_header_t;

typedef struct md2_vertex_s {
	unsigned char x, y, z;
	unsigned char normal;
} md2_vertex_t;

typedef struct md2_cmd_s {
	float s, t;
	int vindex;
} md2_cmd_t;

typedef struct md2_frame_s {
	ScePspFVector3 scale;
	ScePspFVector3 translate;
	char name[16];
	md2_vertex_t verts;
} md2_frame_t;

typedef struct md2_info_s {
	int framesize;
	void* frames;
	void* cmds;
} md2_info_t;

/*
===========
Md2_Load
===========
*/
void Md2_Load(model_t* mod, char* name) {
	FILE* fh;
	int flength = FS_FOpenFile(name, &fh);
	if(flength == -1) {
		ri.Sys_Error(ERR_DROP, "%s file not found", mod->name);
		return;
	}
	int foffset = ftell(fh);

	md2_header_t header;
	if(fread(&header, sizeof(header), 1, fh) != 1) {
		ri.Sys_Error(ERR_DROP, "%s has to short header", mod->name);
		return;
	}

	mod->type      = mod_alias;
	mod->numframes = header.num_frames;
	mod->extradata = Hunk_Begin(0x100000);

	md2_info_t* info = Hunk_Alloc(sizeof(md2_info_t));
	info->framesize = header.framesize;

	void SCR_UpdateScreen(void);

	ri.Con_Printf(PRINT_ALL, "%s load skins\n", mod->name);
	SCR_UpdateScreen();

	{ // Load Skins
		fseek(fh, foffset + header.ofs_skins, SEEK_SET);
		int i;
		for(i = 0; i < header.num_skins; i++) {
			char skin[MAX_SKINNAME];
			fread(skin, 1, sizeof(skin), fh);
			mod->skins[i] = GU_FindImage(skin, it_skin);
		}
	}

	ri.Con_Printf(PRINT_ALL, "%s load frames\n", mod->name);
	SCR_UpdateScreen();

	{ // Load Frames
		info->frames = Hunk_Alloc(info->framesize * mod->numframes);
		fseek(fh, foffset + header.ofs_frames, SEEK_SET);
		fread(info->frames, mod->numframes, info->framesize, fh);
	}

#if 0
	ri.Con_Printf(PRINT_ALL, "%s load cmds\n", mod->name);
	SCR_UpdateScreen();

	{ // Load Commands
		info->cmds = Hunk_Alloc(flength - header.ofs_cmds);
		fseek(fh, foffset + header.ofs_cmds, SEEK_SET);
		fread(info->cmds, 1, flength - header.ofs_cmds, fh);
	}

	mod->mins[0] = mod->mins[1] = mod->mins[2] = -32.0f;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] =  32.0f;
#endif

	mod->extradatasize = Hunk_End();

	fclose(fh);

	ri.Con_Printf(PRINT_ALL, "%s finished %d\n", mod->name, mod->extradatasize);
	SCR_UpdateScreen();
}

static void Md2_DrawSingleFrame(model_t* mod, int frame) {
	ri.Con_Printf(PRINT_ALL, "Md2_DrawSingleFrame\n");
}

static void Md2_DrawLerpFrame(model_t* mod, int frame, int oldframe, float lerp) {
	ri.Con_Printf(PRINT_ALL, "Md2_DrawLerpFrame\n");
}

/*
===========
Md2_Draw
===========
*/
void Md2_Draw(entity_t* ent) {
	model_t* mod = ent->model;

#if 0
	if(ent->flags & RF_WEAPONMODEL) {
		if(gu_hand->value == 2.0f) {
			return;
		}

		if(gu_hand->value == 1.0f) {
			sceGumMatrixMode(GU_PROJECTION);
			sceGumPushMatrix();
			sceGumLoadIdentity();
			ScePspFVector3 scl = { -1.0f, 1.0f, 1.0f };
			sceGumScale(&scl);
			sceGumPerspective(r_newrefdef.fov_y, (float)r_newrefdef.width/r_newrefdef.height, 4.0f, 4096.0f);
			sceGumMatrixMode(GU_MODEL);
			sceGuFrontFace(GU_CCW);
		}
	}

	if(ent->flags & RF_DEPTHHACK) {
		sceGuDepthRange(50000, 35000);
	}

	if(ent->flags & RF_TRANSLUCENT) {
		sceGuEnable(GU_BLEND);
	}

	sceGuShadeModel(GU_SMOOTH);

	sceGuColor(0xffffff); // PSP_FIXME

	sceGumPushMatrix();
	ent->angles[PITCH] = -ent->angles[PITCH];
	GU_RotateForEntity(ent);
	ent->angles[PITCH] = -ent->angles[PITCH];

	{ // Texture
		image_t* skin;
		if(ent->skin) {
			skin = ent->skin;
		} else {
			if(ent->skinnum >= MAX_MD2SKINS) {
				skin = mod->skins[0];
			} else {
				skin = mod->skins[ent->skinnum];
				if(!skin) {
					skin = mod->skins[0];
				}
			}
		}
		if(!skin) {
			skin = gucontext.notex;
		}
		GU_Bind(skin);
	}
#endif

	{ // Draw
		if(ent->frame >= mod->numframes || ent->frame < 0) {
			ri.Con_Printf(PRINT_ALL, "Md2_Draw %s: no frame %d\n", mod->name, ent->frame);
			ent->frame = ent->oldframe = 0;
		}

		if(ent->oldframe >= mod->numframes || ent->oldframe < 0) {
			ri.Con_Printf(PRINT_ALL, "Md2_Draw %s: no frame %d\n", mod->name, ent->oldframe);
			ent->frame = ent->oldframe = 0;
		}

		if(ent->frame == ent->oldframe) {
			Md2_DrawSingleFrame(mod, ent->frame);
		} else {
			Md2_DrawLerpFrame(mod, ent->frame, ent->oldframe, ent->backlerp);
		}
	}

#if 0
	sceGumPopMatrix();

	sceGuColor(0xffffff);

	sceGuShadeModel(GU_FLAT);

	if(ent->flags & RF_TRANSLUCENT) {
		sceGuDisable(GU_BLEND);
	}

	if(ent->flags & RF_DEPTHHACK) {
		sceGuDepthRange(50000, 10000);
	}

	if(ent->flags & RF_WEAPONMODEL) {
		if(gu_hand->value == 1.0f) {
			sceGuFrontFace(GU_CW);
			sceGumMatrixMode(GU_PROJECTION);
			sceGumPopMatrix();
			sceGumMatrixMode(GU_MODEL);
		}
	}
#endif
}

#endif
