#include "gu_local.h"

static qboolean gu_imagesinit = false;

#if 0
typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

static void R_FloodFillSkin(byte* skin, int skinwidth, int skinheight) {
#if 0
	byte		fillcolor = *skin;
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int			inpt = 0, outpt = 0;
	int			filledcolor = -1;
	int			i;

	if(filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for(i = 0; i < 256; ++i)
			if(d_8to24table[i] == (255 << 0)) {
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
#endif
}
#endif

/*
===============
GU_FindImages
===============
*/
image_t* GU_FindImage(const char* name, imagetype_t type) {
	if(!name) {
		return 0;
	}

	int len = (int)strlen(name);
	if(len < 5) {
		return 0;
	}

	int i;
	for(i = 0; i < MAX_TEXTURES; i++) {
		// PSP_FIXME: add type compare
		if(!strcmp(name, textures[i].name)) {
			textures[i].seq = gucontext.seq;
			return &textures[i];
		}
	}

	image_t* image;
	if(!strcmp(name + len - 4, ".pcx")) {
		image = GU_LoadPCX(name, type);
	} else if(!strcmp(name + len - 4, ".wal")) {
		image = GU_LoadWal(name, type);
	} else {
		return 0;
	}



	return image;
}

/*
===============
R_RegisterSkin
===============
*/
image_t* R_RegisterSkin(const char *name) {
	return GU_FindImage(name, it_skin);
}

/*
===============
R_InitImages
===============
*/
void R_InitImages(void) {
	if(gu_imagesinit) {
		ri.Sys_Error(PRINT_ALL, "Called GU_InitImages() without GU_ShutdownImages()");
	}
	gu_imagesinit = true;

	memset(textures, 0, sizeof(textures));
	gucontext.seq = 1;

	memset(gucontext.stdpalette, 0xcc, sizeof(gucontext.stdpalette));
	GU_LoadPalette("pics/colormap.pcx", gucontext.stdpalette);

	gucontext.charset  = GU_LoadPCX("pics/conchars.pcx", it_pic);
	gucontext.particle = GU_LoadParticle();
	gucontext.notex    = GU_LoadNoTexture();
	
	ri.Cmd_AddCommand("imagelist", GU_ImageList_f);
}

/*
===============
R_ShutdownImages
===============
*/
void R_ShutdownImages(void) {
	if(!gu_imagesinit) {
		ri.Sys_Error(PRINT_ALL, "Called GU_ShutdownImages() without GU_InitImages()");
	}
	gu_imagesinit = false;
	
	ri.Cmd_RemoveCommand("imagelist");
	
	int	i;
	for(i = 0; i < MAX_TEXTURES; i++) {
		if(textures[i].seq && textures[i].alloc) { // PSP_CHECK
			free(textures[i].alloc);
		}
	}

	memset(textures, 0, sizeof(textures));
}

void GU_ImageList_f(void) {
	const char colorformat[2] = { 'T', 'P' };
	
	ri.Con_Printf(PRINT_ALL, "-------- Imagelist --------\n");

	image_t* img;
	
	unsigned int bytes_use = 0;
	unsigned int bytes_res = 0;
	
	int i;
	for(i = 0, img = textures; i < MAX_TEXTURES; i++, img++) {
		if(img->seq == 0) {
			continue;
		}
		
		char type;
		switch(img->type) {
			case it_skin :
				type = 'M';
				break;
			case it_sprite :
				type = 'S';
				break;
			case it_wall :
				type = 'W';
				break;
			case it_pic :
				type = 'P';
				break;
			default :
				type = ' ';
				break;
		}
		
		ri.Con_Printf(PRINT_ALL, "%c%c %3d %3d : %s\n",
			type, colorformat[1],
			img->width, img->height,
			img->name
		);
		
		bytes_use += img->width   * img->height;
		bytes_res += img->widthp2 * img->heightp2;
	}
	
	ri.Con_Printf(PRINT_ALL, "Texels  use %d  reserved %d  (%d%%)\n",
		bytes_use, bytes_res, bytes_use * 100 / bytes_res
	);
}
