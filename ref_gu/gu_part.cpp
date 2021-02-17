#include "gu_local.h"

static void DrawSpriteParticles(void) {
	GU_Bind(gucontext.particle);

	sceGuEnable(GU_BLEND);
	sceGuDepthMask(GU_TRUE);

	register vec3_t scale;
	VectorAdd(vup, vright, scale);

	typedef struct {
		float s, t;
		unsigned int color;
		float x, y, z;
	} VERT;
	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * r_newrefdef.num_particles * 2);
		
	register VERT* v0 = v + 0;
	register VERT* v1 = v + 1;
	register particle_t* p0 = r_newrefdef.particles;
	register int i;
	for(i = 0; i < r_newrefdef.num_particles; i++, p0++, v0+=2, v1+=2) {
		unsigned int color = (gucontext.stdpalette[p0->color] & 0x00ffffff) | ((unsigned int)(p0->alpha * 255) << 24);

		register vec3_t temp;

		VectorAdd(p0->origin, scale, temp);
		v0->color = color;
		v0->s = 0.00f;
		v0->t = 0.00f;
		v0->x = temp[0];
		v0->y = temp[1];
		v0->z = temp[2]; 
		
		VectorSubtract(p0->origin, scale, temp);
		v1->color = color;
		v1->s = 1.00f;
		v1->t = 1.00f;
		v1->x = temp[0];
		v1->y = temp[1];
		v1->z = temp[2];
	}
	
	sceGumDrawArray(GU_SPRITES,
		GU_VERTEX_32BITF | GU_COLOR_8888 | GU_TEXTURE_32BITF | GU_TRANSFORM_3D,
		r_newrefdef.num_particles * 2, 0, v
	);

	sceGuDepthMask(GU_FALSE);
	sceGuDisable(GU_BLEND);
}

static void DrawPointParticles(void) {
	sceGuDisable(GU_TEXTURE_2D);
	sceGuEnable(GU_BLEND);
	sceGuDepthMask(GU_TRUE);

	typedef struct {
		unsigned int color;
		float x, y, z;
	} VERT;
	VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * r_newrefdef.num_particles);
	
	int i;
	for(i = 0; i < r_newrefdef.num_particles; i++) {
		const particle_t* p0 = &r_newrefdef.particles[i];
		VERT* v0 = &v[i];
		v0->color = (gucontext.stdpalette[p0->color] & 0x00FFFFFF)
                  | ((unsigned int)(p0->alpha * 255) << 24);
		v0->x = p0->origin[0];
		v0->y = p0->origin[1];
		v0->z = p0->origin[2];
	}
	sceGumDrawArray(GU_POINTS,
		GU_VERTEX_32BITF | GU_COLOR_8888 | GU_TRANSFORM_3D,
		r_newrefdef.num_particles, 0, v
	);
	
	sceGuDepthMask(GU_FALSE);
	sceGuDisable(GU_BLEND);
	sceGuEnable(GU_TEXTURE_2D);
}

void R_DrawParticles(void) {
	if(gu_particletype->value == 1) {
		DrawPointParticles();
	} else if(gu_particletype->value == 2) {
		DrawSpriteParticles();
	}
}
