#include "gu_local.h"
#include "gu_model.h"

char	    sky_name[MAX_QPATH];
float	    sky_rotate;
float       sky_min;
float       sky_max;
float       skymins[2][6];
float       skymaxs[2][6];
vec3_t	    sky_axis;
image_t*    sky_images[6];
const char* sky_sidenames[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
int	        sky_texorder[6]  = { 0, 2, 1, 3, 4, 5 };

static const vec3_t skyclip[6] = {
	{ 1, 1, 0}, { 1,-1, 0}, { 0,-1, 1}, { 0, 1, 1}, { 1, 0, 1}, {-1, 0, 1} 
};

static const int st_to_vec[6][3] = {
	{ 3,-1, 2}, {-3, 1, 2}, { 1, 3, 2}, {-1,-3, 2}, {-2,-1, 3}, { 2,-1,-3}
};

static const int vec_to_st[6][3] = {
	{-2, 3, 1}, { 2, 3,-1}, { 1, 3, 2}, {-1, 3,-2}, {-2,-1, 3}, {-2, 1,-3}
};

extern msurface_t* r_sky_surfaces;

#define	MAX_CLIP_VERTS	64
#define	ON_EPSILON		0.1			// point on plane side epsilon

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001)
			continue;	// don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		ri.Sys_Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = (float*)skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

void R_DrawSky() {
#if 1
//	sceGuDisable(GU_CULL_FACE);
	void GU_DrawPoly(const brushpoly_t* p, const GUPlane* planes);

	brushpoly_t* poly;
	
	// right
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x =  2048.0f + r_origin[0];
	poly->sphere.pos.y =     0.0f + r_origin[1];
	poly->sphere.pos.z =     0.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  1.0f;
	poly->verts[0].tex.y =  0.0f;
	poly->verts[0].pos.x =  2048.0f + r_origin[0];
	poly->verts[0].pos.y =  2048.0f + r_origin[1];
	poly->verts[0].pos.z =  2048.0f + r_origin[2];

	poly->verts[1].tex.x =  0.0f;
	poly->verts[1].tex.y =  0.0f;
	poly->verts[1].pos.x =  2048.0f + r_origin[0];
	poly->verts[1].pos.y = -2048.0f + r_origin[1];
	poly->verts[1].pos.z =  2048.0f + r_origin[2];

	poly->verts[2].tex.x =  0.0f;
	poly->verts[2].tex.y =  1.0f;
	poly->verts[2].pos.x =  2048.0f + r_origin[0];
	poly->verts[2].pos.y = -2048.0f + r_origin[1];
	poly->verts[2].pos.z = -2048.0f + r_origin[2];

	poly->verts[3].tex.x =  1.0f;
	poly->verts[3].tex.y =  1.0f;
	poly->verts[3].pos.x =  2048.0f + r_origin[0];
	poly->verts[3].pos.y =  2048.0f + r_origin[1];
	poly->verts[3].pos.z = -2048.0f + r_origin[2];

	GU_Bind(sky_images[0]);
	GU_DrawPoly(poly, gu_cplanes);

	// back
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x =     0.0f + r_origin[0];
	poly->sphere.pos.y = -2048.0f + r_origin[1];
	poly->sphere.pos.z =     0.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  1.0f;
	poly->verts[0].tex.y =  0.0f;
	poly->verts[0].pos.x =  2048.0f + r_origin[0];
	poly->verts[0].pos.y = -2048.0f + r_origin[1];
	poly->verts[0].pos.z =  2048.0f + r_origin[2];

	poly->verts[1].tex.x =  0.0f;
	poly->verts[1].tex.y =  0.0f;
	poly->verts[1].pos.x = -2048.0f + r_origin[0];
	poly->verts[1].pos.y = -2048.0f + r_origin[1];
	poly->verts[1].pos.z =  2048.0f + r_origin[2];

	poly->verts[2].tex.x =  0.0f;
	poly->verts[2].tex.y =  1.0f;
	poly->verts[2].pos.x = -2048.0f + r_origin[0];
	poly->verts[2].pos.y = -2048.0f + r_origin[1];
	poly->verts[2].pos.z = -2048.0f + r_origin[2];

	poly->verts[3].tex.x =  1.0f;
	poly->verts[3].tex.y =  1.0f;
	poly->verts[3].pos.x =  2048.0f + r_origin[0];
	poly->verts[3].pos.y = -2048.0f + r_origin[1];
	poly->verts[3].pos.z = -2048.0f + r_origin[2];

	GU_Bind(sky_images[1]);
	GU_DrawPoly(poly, gu_cplanes);

	// left
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x = -2048.0f + r_origin[0];
	poly->sphere.pos.y =     0.0f + r_origin[1];
	poly->sphere.pos.z =     0.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  0.0f;
	poly->verts[0].tex.y =  0.0f;
	poly->verts[0].pos.x = -2048.0f + r_origin[0];
	poly->verts[0].pos.y =  2048.0f + r_origin[1];
	poly->verts[0].pos.z =  2048.0f + r_origin[2];

	poly->verts[1].tex.x =  0.0f;
	poly->verts[1].tex.y =  1.0f;
	poly->verts[1].pos.x = -2048.0f + r_origin[0];
	poly->verts[1].pos.y =  2048.0f + r_origin[1];
	poly->verts[1].pos.z = -2048.0f + r_origin[2];

	poly->verts[2].tex.x =  1.0f;
	poly->verts[2].tex.y =  1.0f;
	poly->verts[2].pos.x = -2048.0f + r_origin[0];
	poly->verts[2].pos.y = -2048.0f + r_origin[1];
	poly->verts[2].pos.z = -2048.0f + r_origin[2];

	poly->verts[3].tex.x =  1.0f;
	poly->verts[3].tex.y =  0.0f;
	poly->verts[3].pos.x = -2048.0f + r_origin[0];
	poly->verts[3].pos.y = -2048.0f + r_origin[1];
	poly->verts[3].pos.z =  2048.0f + r_origin[2];

	GU_Bind(sky_images[2]);
	GU_DrawPoly(poly, gu_cplanes);

	// front
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x =     0.0f + r_origin[0];
	poly->sphere.pos.y =  2048.0f + r_origin[1];
	poly->sphere.pos.z =     0.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  0.0f;
	poly->verts[0].tex.y =  0.0f;
	poly->verts[0].pos.x =  2048.0f + r_origin[0];
	poly->verts[0].pos.y =  2048.0f + r_origin[1];
	poly->verts[0].pos.z =  2048.0f + r_origin[2];

	poly->verts[1].tex.x =  0.0f;
	poly->verts[1].tex.y =  1.0f;
	poly->verts[1].pos.x =  2048.0f + r_origin[0];
	poly->verts[1].pos.y =  2048.0f + r_origin[1];
	poly->verts[1].pos.z = -2048.0f + r_origin[2];

	poly->verts[2].tex.x =  1.0f;
	poly->verts[2].tex.y =  1.0f;
	poly->verts[2].pos.x = -2048.0f + r_origin[0];
	poly->verts[2].pos.y =  2048.0f + r_origin[1];
	poly->verts[2].pos.z = -2048.0f + r_origin[2];

	poly->verts[3].tex.x =  1.0f;
	poly->verts[3].tex.y =  0.0f;
	poly->verts[3].pos.x = -2048.0f + r_origin[0];
	poly->verts[3].pos.y =  2048.0f + r_origin[1];
	poly->verts[3].pos.z =  2048.0f + r_origin[2];

	GU_Bind(sky_images[3]);
	GU_DrawPoly(poly, gu_cplanes);

	// top
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x =     0.0f + r_origin[0];
	poly->sphere.pos.y =     0.0f + r_origin[1];
	poly->sphere.pos.z =  2048.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  1.0f;
	poly->verts[0].tex.y =  1.0f;
	poly->verts[0].pos.x =  2048.0f + r_origin[0];
	poly->verts[0].pos.y =  2048.0f + r_origin[1];
	poly->verts[0].pos.z =  2048.0f + r_origin[2];

	poly->verts[1].tex.x =  1.0f;
	poly->verts[1].tex.y =  0.0f;
	poly->verts[1].pos.x = -2048.0f + r_origin[0];
	poly->verts[1].pos.y =  2048.0f + r_origin[1];
	poly->verts[1].pos.z =  2048.0f + r_origin[2];

	poly->verts[2].tex.x =  0.0f;
	poly->verts[2].tex.y =  0.0f;
	poly->verts[2].pos.x = -2048.0f + r_origin[0];
	poly->verts[2].pos.y = -2048.0f + r_origin[1];
	poly->verts[2].pos.z =  2048.0f + r_origin[2];

	poly->verts[3].tex.x =  0.0f;
	poly->verts[3].tex.y =  1.0f;
	poly->verts[3].pos.x =  2048.0f + r_origin[0];
	poly->verts[3].pos.y = -2048.0f + r_origin[1];
	poly->verts[3].pos.z =  2048.0f + r_origin[2];

	GU_Bind(sky_images[4]);
	GU_DrawPoly(poly, gu_cplanes);

	// bottom
	poly = (brushpoly_t*)sceGuGetMemory(sizeof(brushpoly_t) + 4 * sizeof(brushvertex_t));
	poly->sphere.pos.x =     0.0f + r_origin[0];
	poly->sphere.pos.y =     0.0f + r_origin[1];
	poly->sphere.pos.z = -2048.0f + r_origin[2];
	poly->sphere.rad   =  2896.309f;
	poly->numverts = 4;

	poly->verts[0].tex.x =  1.0f;
	poly->verts[0].tex.y =  0.0f;
	poly->verts[0].pos.x =  2048.0f + r_origin[0];
	poly->verts[0].pos.y =  2048.0f + r_origin[1];
	poly->verts[0].pos.z = -2048.0f + r_origin[2];

	poly->verts[1].tex.x =  0.0f;
	poly->verts[1].tex.y =  0.0f;
	poly->verts[1].pos.x =  2048.0f + r_origin[0];
	poly->verts[1].pos.y = -2048.0f + r_origin[1];
	poly->verts[1].pos.z = -2048.0f + r_origin[2];

	poly->verts[2].tex.x =  0.0f;
	poly->verts[2].tex.y =  1.0f;
	poly->verts[2].pos.x = -2048.0f + r_origin[0];
	poly->verts[2].pos.y = -2048.0f + r_origin[1];
	poly->verts[2].pos.z = -2048.0f + r_origin[2];

	poly->verts[3].tex.x =  1.0f;
	poly->verts[3].tex.y =  1.0f;
	poly->verts[3].pos.x = -2048.0f + r_origin[0];
	poly->verts[3].pos.y =  2048.0f + r_origin[1];
	poly->verts[3].pos.z = -2048.0f + r_origin[2];

	GU_Bind(sky_images[5]);
	GU_DrawPoly(poly, gu_cplanes);

//	sceGuEnable(GU_CULL_FACE);

#elif 0
	sceGuDisable(GU_TEXTURE_2D);
	sceGuColor(0xff0066cc);

	msurface_t* s;
	for(s = r_sky_surfaces; s; s = s->texturechain) {
		brushpoly_t* p = s->polys;
		for(p = s->polys; p; p = p->next) {
			typedef struct {
				float x, y, z;
			} VERT;
	
			VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * p->numverts);

			int i;
			for(i = 0; i < p->numverts; i++) {
				v[i].x = p->verts[i].pos.x;
				v[i].y = p->verts[i].pos.y;
				v[i].z = p->verts[i].pos.z;
			}

			sceGumDrawArray(GU_TRIANGLE_FAN,
				GU_VERTEX_32BITF | GU_TRANSFORM_3D,
				p->numverts, 0, v
			);
		}
	}

	sceGuColor(0xffffffff);
	sceGuEnable(GU_TEXTURE_2D);

	r_sky_surfaces = 0;
#else
	int	i;
	for(i=0; i < 6; i++) {
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}

	msurface_t* s;
	for(s = r_sky_surfaces; s; s = s->texturechain) {
		brushpoly_t* p;
		for(p = s->polys; p; p = p->next) {
			vec3_t verts[MAX_CLIP_VERTS];
			int i;
			for(i = 0; i < p->numverts; i++) {
				verts[i][0] = p->verts[i].x - r_origin[0];
				verts[i][1] = p->verts[i].y - r_origin[1];
				verts[i][2] = p->verts[i].z - r_origin[2];
			}
			ClipSkyPolygon(p->numverts, verts[0], 0);
		}
	}

	r_sky_surfaces = 0;

	sceGumPushMatrix();
	sceGumTranslate((ScePspFVector3*)r_origin);

	for(i = 0; i < 6; i++) {
		if (skymins[0][i] >= skymaxs[0][i]
		|| skymins[1][i] >= skymaxs[1][i])
			continue;

		GU_Bind(sky_images[sky_texorder[i]]);

		typedef struct {
			float s, t;
			float x, y, z;
		} VERT;

		VERT* verts = sceGuGetMemory(sizeof(VERT) * 4);
		
		int vi;
		for(vi = 0; vi < 4; vi++) {
			float s;
			float t;
			switch(vi) {
			case 0  : s = skymins[0][i]; t = skymins[1][i]; break;
			case 1  : s = skymins[0][i]; t = skymaxs[1][i]; break;
			case 2  : s = skymaxs[0][i]; t = skymaxs[1][i]; break;
			default : s = skymaxs[0][i]; t = skymins[1][i]; break;
			}

			int axis = i;

			vec3_t		v, b;
			int			j, k;

			b[0] = s*2300;
			b[1] = t*2300;
			b[2] = 2300;

			for(j=0 ; j<3 ; j++) {
				k = st_to_vec[axis][j];
				if (k < 0) {
					v[j] = -b[-k - 1];
				} else {
					v[j] = b[k - 1];
				}
			}

			// avoid bilerp seam
			s = (s+1)*0.5;
			t = (t+1)*0.5;

			if (s < sky_min)
				s =	sky_min;
			else if (s > sky_max)
				s = sky_max;
			if (t < sky_min)
				t = sky_min;
			else if (t > sky_max)
				t = sky_max;

			t = 1.0 - t;
			verts[vi].s = s;
			verts[vi].s = t;
			verts[vi].x = v[0];
			verts[vi].y = v[0];
			verts[vi].z = v[0];
		}
		sceGumDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, verts);
	}

	sceGumPopMatrix();
#endif
}

void R_RegisterSky(const char* name, float rotate, vec3_t axis) {
	strncpy(sky_name, name, sizeof(sky_name) - 1);
	sky_rotate = rotate;
	VectorCopy(axis, sky_axis);
	
	int i;
	for(i = 0; i < 6; i++) {
		char pathname[MAX_QPATH];
		
		Com_sprintf(pathname, sizeof(pathname), "env/%s%s.pcx", sky_name, sky_sidenames[i]);
		
		sky_images[i] = GU_FindImage(pathname, it_sky);
		
		if(!sky_images[i]) {
			sky_images[i] = gucontext.notex;
		}
		
		sky_min =   1.0f / 256.0f;
		sky_max = 255.0f / 256.0f;
	}
}
