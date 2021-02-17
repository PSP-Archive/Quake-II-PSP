#include "gu_local.h"
#include "gu_model.h"

#include "gu_math.h"

static vec3_t	modelorg;		// relative to viewpoint

msurface_t* r_alpha_surfaces = 0;
msurface_t* r_sky_surfaces = 0;

unsigned int r_polys;

image_t* R_TextureAnimation(mtexinfo_t *tex) {
	if(!tex->next) {
		return tex->image;
	}

	int c = currententity->frame % tex->numframes;
	while(c) {
		tex = tex->next;
		c--;
	}

	return tex->image;
}

void R_MarkLeaves(void) {
	byte	*vis;
	byte	fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;

	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) {
		return;
	}

#if defined(PSP_DEBUG_CVAR_DBG_LOCKPVS) && (PSP_DEBUG_CVAR_DBG_LOCKPVS > 0)
	if(dbg_lockpvs->value) {
		return;
	}
#endif

#if 0
	if(r_oldviewcluster  == r_viewcluster
	&& r_oldviewcluster2 == r_viewcluster2
	&& r_viewcluster != -1) {
		return;
	}
#endif

#if 0 // PSP_REMOVE
	if(!r_novis->value) {
		return;
	}
#endif

	r_visframecount++;
	r_oldviewcluster  = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if(/*r_novis->value || */r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS(r_viewcluster, r_worldmodel);
	// may have to combine two clusters because of solid water boundaries
	if(r_viewcluster2 != r_viewcluster) {
		memcpy(fatvis, vis, (r_worldmodel->numleafs+7)/8);
		vis = Mod_ClusterPVS(r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs+31)/32;
		for(i=0 ; i<c ; i++) {
			((int *)fatvis)[i] |= ((int *)vis)[i];
		}
		vis = fatvis;
	}
	
	for(i=0, leaf=r_worldmodel->leafs; i<r_worldmodel->numleafs; i++, leaf++) {
		int cluster = leaf->cluster;
		if(cluster == -1) {
			continue;
		}

		if(vis[cluster>>3] & (1<<(cluster&7))) {
			node = (mnode_t*)leaf;
			do {
				if(node->visframe == r_visframecount) {
					break;
				}
				node->visframe = r_visframecount;
				node = node->parent;
			} while(node);
		}
	}
}

qboolean R_CullBox(vec3_t mins, vec3_t maxs) {
#if 0
	if(r_nocull->value) {
		return false;
	}
#endif

	int i;
	for(i = 0; i < 4; i++) {
		if(BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2) {
			return true;
		}
	}
	return false;
}

static int poly_all;
static int poly_forcenoclip;
static int poly_oversized;
static int poly_earlyoutside;
static int poly_earlynoclip;
static int poly_outside;
static int poly_noclip;
static int poly_clip;

static float distancePointPlane(const GUVec3* point, const GUPlane* plane) {
	return plane->nrm.x * point->x
		 + plane->nrm.y * point->y
		 + plane->nrm.z * point->z
		 + plane->dis;
}

static float distanceSpherePlane(const GUSphere* sphere, const GUPlane* plane) {
	return plane->nrm.x * sphere->pos.x
		 + plane->nrm.y * sphere->pos.y
		 + plane->nrm.z * sphere->pos.z
		 + plane->dis   - sphere->rad;
}

static float distanceRayPlane(const GURay* ray, const GUPlane* plane) {
	float m = plane->nrm.x * ray->org.x
		    + plane->nrm.y * ray->org.y
			+ plane->nrm.z * ray->org.z
			+ plane->dis;

	float n = plane->nrm.x * ray->dir.x
		    + plane->nrm.y * ray->dir.y
			+ plane->nrm.z * ray->dir.z;

	return -(m / n);
}

void GU_DrawPoly(const brushpoly_t* p, const GUPlane* planes) {
	// one hurray for unreadable code

	poly_all++;

	// no clipping at all
	if(!planes || gu_clippingepsilon->value < 0.0f) {
		sceGumDrawArray(GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			p->numverts, 0, p->verts
		);

		poly_forcenoclip++;
		return;
	}

	// poly has too much vertices, don't clip just draw
	if(p->numverts > 16) {
		//sceGuColor(0xff8080ff);
		//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGumDrawArray(GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			p->numverts, 0, p->verts
		);
		//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		//sceGuColor(0xffffffff);

		poly_oversized++;
		return;
	}

	union {
		unsigned int i;
		unsigned char c[4];
	} cliptest;
	cliptest.i = 0;

	// try to early skip polygons based on sphere/plane intersection
	for(int k = 0; k < 4; k++) {
		float dis = distanceSpherePlane(&p->sphere, &planes[k]);
		if(dis < 0.0f) {
			cliptest.c[k] = 1;
		}
	}

	if(cliptest.i == 0) {
		// polygon does not need clipping
		//sceGuColor(0xffff8080);
		//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGumDrawArray(GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			p->numverts, 0, p->verts
		);
		//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		//sceGuColor(0xffffffff);

		poly_earlynoclip++;
		return;
	}

	bool clipped = false;

	int verti[5][16] = { {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	} };

	brushvertex_t newverts[17];
	int newvertc = 1;

	int streaminsize;
	int streamoutsize = p->numverts;
	int* streamin;
	int* streamout;
	int* streaminlast;
	int* streaminend;

	int streamindex = -1;

	for(int k = 0; k < 4; k++) {
		if(!cliptest.c[k]) {
			continue;
		}

		streamindex++;

		streaminsize  = streamoutsize;
		streamoutsize = 0;

		streamin     = verti[streamindex];
		streamout    = verti[streamindex+1];
		streaminend  = verti[streamindex] + streaminsize;
		streaminlast = verti[streamindex] + streaminsize;

		GUVec3 point;

		// find first point inside
		float dis;
		do {
			point = *streamin >= 0 ? p->verts[*streamin].pos : newverts[-*streamin].pos;
			dis = distancePointPlane(&point, &planes[k]);

			if(dis > 0.0f) {
				goto clip_firstin;
			}

			streamin++;
		} while(streamin != streaminlast);

		// polygon is completly outside
		poly_outside++;
		return;

clip_firstin:;
		// write first point to the output stream
		*streamout = *streamin;
		streamout++;
		streamoutsize++;

		int lastIndex = *streamin;

		streaminend = streamin;
		streamin++;
		if(streamin == streaminlast) streamin = verti[streamindex];

		do {
			point = *streamin >= 0 ? p->verts[*streamin].pos : newverts[-*streamin].pos;
			dis = distancePointPlane(&point, &planes[k]);

			if(dis < 0.0f) {
				goto clip_in2out;
			}

			*streamout = *streamin;
			streamout++;
			streamoutsize++;

			lastIndex = *streamin;

			streamin++;
			if(streamin == streaminlast) streamin = verti[streamindex];
		} while(streamin != streaminend);

		// no clipping needed for this plane
		continue;

clip_in2out:;
		clipped = true;

		// calc point between in and out
		{
			int i0 = lastIndex;
			int i1 = *streamin;

			brushvertex_t b0 = i0 >= 0 ? p->verts[i0] : newverts[-i0];
			brushvertex_t b1 = i1 >= 0 ? p->verts[i1] : newverts[-i1];

			GUVec3 dir;
			GUMath_Sub(&dir, &b1.pos, &b0.pos);
			float len = GUMath_Length(&dir);
			GUMath_Mul(&dir, &dir, 1.0f/len);

			GURay ray;
			ray.org = b0.pos;
			ray.dir = dir;

			float d = distanceRayPlane(&ray, &planes[k]) + 1.0f;
			float weight = (d / len);

			newverts[newvertc].tex.x = b0.tex.x + (b1.tex.x - b0.tex.x) * weight;
			newverts[newvertc].tex.y = b0.tex.y + (b1.tex.y - b0.tex.y) * weight;
			newverts[newvertc].pos.x = b0.pos.x + (b1.pos.x - b0.pos.x) * weight;
			newverts[newvertc].pos.y = b0.pos.y + (b1.pos.y - b0.pos.y) * weight;
			newverts[newvertc].pos.z = b0.pos.z + (b1.pos.z - b0.pos.z) * weight;

			*streamout = -newvertc;
			streamout++;
			streamoutsize++;
			newvertc++;
		}

		// skip all vertices outside
		do {
			point = *streamin >= 0 ? p->verts[*streamin].pos : newverts[-*streamin].pos;
			dis = distancePointPlane(&point, &planes[k]);

			if(dis > 0.0f) {
				goto clip_out2in;
			}

			lastIndex = *streamin;

			streamin++;
			if(streamin == streaminlast) streamin = verti[streamindex];
		} while(1);

clip_out2in:;

		// calc point between out and in
		{
			int i0 = *streamin;
			int i1 = lastIndex;

			brushvertex_t b0 = i0 >= 0 ? p->verts[i0] : newverts[-i0];
			brushvertex_t b1 = i1 >= 0 ? p->verts[i1] : newverts[-i1];

			GUVec3 dir;
			GUMath_Sub(&dir, &b1.pos, &b0.pos);
			float len = GUMath_Length(&dir);
			GUMath_Mul(&dir, &dir, 1.0f/len);

			GURay ray;
			ray.org = b0.pos;
			ray.dir = dir;

			float d = distanceRayPlane(&ray, &planes[k]) + 1.0f;
			float weight = (d / len);

			newverts[newvertc].tex.x = b0.tex.x + (b1.tex.x - b0.tex.x) * weight;
			newverts[newvertc].tex.y = b0.tex.y + (b1.tex.y - b0.tex.y) * weight;
			newverts[newvertc].pos.x = b0.pos.x + (b1.pos.x - b0.pos.x) * weight;
			newverts[newvertc].pos.y = b0.pos.y + (b1.pos.y - b0.pos.y) * weight;
			newverts[newvertc].pos.z = b0.pos.z + (b1.pos.z - b0.pos.z) * weight;

			*streamout = -newvertc;
			streamout++;
			streamoutsize++;
			newvertc++;
		}

		// add rest
		while(streamin != streaminend) {
			*streamout = *streamin;
			streamout++;
			streamoutsize++;

			streamin++;
			if(streamin == streaminlast) streamin = verti[streamindex];
		}
	}

	if(!clipped) {
		//sceGuColor(0xff80ff80);
		//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGumDrawArray(GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			p->numverts, 0, p->verts
		);
		//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		//sceGuColor(0xffffffff);

		poly_noclip++;
		return;
	}

	brushvertex_t* verts = (brushvertex_t*)sceGuGetMemory(streamoutsize * sizeof(brushvertex_t));

	streamout = verti[streamindex+1];
	for(int i = 0; i < streamoutsize; i++, streamout++) {
		int index = *streamout;
		verts[i] = index >= 0 ? p->verts[index] : newverts[-index];
	}

#if 0
	if(poly_clip == 0) {
		sceGuColor(0xff8080ff);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGumDrawArray(GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
			streamoutsize, 0, verts
		);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		sceGuColor(0xffffffff);

		GUPlane plane;
		plane.nrm.x =  frustum[0].normal[0];
		plane.nrm.y =  frustum[0].normal[1];
		plane.nrm.z =  frustum[0].normal[2];
		plane.dis   = -frustum[0].dist;

		printf("Plane\n");
		printf("% 4.2f, % 4.2f, % 4.2f, % 4.2f\n", plane.nrm.x, plane.nrm.y, plane.nrm.z, plane.dis);

		printf("Original Polygon\n");
		for(int i = 0; i < p->numverts; i++) {
			float dis = distancePointPlane(&p->verts[i].pos, &plane);
			printf("% 4.2f, % 4.2f, % 4.2f dis:% 4.2f\n",
				p->verts[i].pos.x, p->verts[i].pos.y, p->verts[i].pos.z, dis);
		}

		printf("New Polygon\n");
		for(int i = 0; i < streamoutsize; i++) {
			float dis = distancePointPlane(&verts[i].pos, &plane);
			printf("% 4.2f, % 4.2f, % 4.2f dis:% 4.2f\n",
				verts[i].pos.x, verts[i].pos.y, verts[i].pos.z, dis);
		}

		for(int k = 0; k < 5; k++) {
			printf("Stream %d\n", k);
			for(int i = 0; i < streamoutsize; i++) {
				printf("% 2d ", verti[k][i]);
			}
			printf("\n");
		}
		
		poly_clip++;
		return;
	}
#endif

	//sceGuColor(0xff80ffff);
	//sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGumDrawArray(GU_TRIANGLE_FAN,
		GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		streamoutsize, 0, verts
	);
	//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	//sceGuColor(0xffffffff);

	poly_clip++;
}

void R_RenderBrushPoly(msurface_t* surf) {
	image_t* img = R_TextureAnimation(surf->texinfo);
	GU_Bind(img);

#if 0
	if(surf->flags & SURF_DRAWTURB) {
		GU_DrawWaterPoly(surf->polys);
		return;
	}
#endif

#if 0 // PSP_REMOVE
	if(surf->texinfo->flags & SURF_FLOWING) {
		GU_DrawFlowingPoly(surf->polys);
		return;
	}
#endif

	GU_DrawPoly(surf->polys, gu_cplanes);
}

void R_DrawTextureChains(void) {
	int i;
	image_t* img;
	for(i = 0, img = &textures[0]; i < MAX_TEXTURES; i++, img++) {
		if(!img->seq) {
			continue;
		}

		msurface_t* surf = img->texturechain;
		if(!surf) {
			continue;
		}

		for(; surf; surf = surf->texturechain) {
			R_RenderBrushPoly(surf);
		}
		
		img->texturechain = 0;
	}
}

void R_DrawAlphaSurfaces(void) {
	sceGuEnable(GU_BLEND);
	sceGuDepthMask(GU_TRUE);

	msurface_t* s;
	for(s = r_alpha_surfaces; s; s = s->texturechain) {
		GU_Bind(s->texinfo->image);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);

		if(s->texinfo->flags & SURF_TRANS33) {
			sceGuColor(0x55ffffff);
		} else if(s->texinfo->flags & SURF_TRANS66) {
			sceGuColor(0xaaffffff);
		} else {
			sceGuColor(0xffffffff);
		}

		GU_DrawPoly(s->polys, gu_cplanes);
	}

	r_alpha_surfaces = 0;

	sceGuDepthMask(GU_FALSE);
	sceGuDisable(GU_BLEND);
}

void R_RecursiveWorldNode(mnode_t *node) {
	int			side, sidebit;
	msurface_t	*surf;
	float		dot;
	image_t*	image;

	if(node->contents == CONTENTS_SOLID) {
		return;		// solid
	}

	if(node->visframe != r_visframecount) {
		return;
	}
	if(R_CullBox(node->minmaxs, node->minmaxs+3)) {
		return;
	}

// if a leaf node, draw stuff
	if(node->contents != -1) {
		mleaf_t* pleaf = (mleaf_t*)node;

		// check for door connected areas
		if(r_newrefdef.areabits) {
			if(!(r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)))) {
				return;
			}
		}

		msurface_t** mark = pleaf->firstmarksurface;
		int c = pleaf->nummarksurfaces;
		if(c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while(--c);
		}

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	cplane_t* plane = node->plane;

	switch(plane->type) {
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if(dot >= 0) {
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side]);

	// draw stuff
	int c;
	for(c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++) {
		if(surf->visframe != r_framecount) {
			continue;
		}

#if 0
		if((surf->flags & SURF_PLANEBACK) != sidebit) {
			continue;		// wrong side
		}
#endif

		if(surf->texinfo->flags & SURF_SKY) {
			surf->texturechain = r_sky_surfaces;
			r_sky_surfaces = surf;
		} else if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66)) {
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		} else {
#if 0
			if(!(surf->flags & SURF_DRAWTURB)) {
				GU_RenderLightmappedPoly(surf);
			} else {
#endif
				image = R_TextureAnimation(surf->texinfo);
				surf->texturechain = image->texturechain;
				image->texturechain = surf;
#if 0
			}
#endif
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side]);
}

void R_DrawWorld(void) {
	r_polys = 0;

	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL) {
		return;
	}

	currentmodel = r_worldmodel;

	VectorCopy(r_newrefdef.vieworg, modelorg);

	entity_t ent;
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time * 2);
	currententity = &ent;

	sceGuColor(0xffffffff);

	poly_all = 0;
	poly_forcenoclip = 0;
	poly_oversized = 0;
	poly_earlyoutside = 0;
	poly_earlynoclip = 0;
	poly_outside = 0;
	poly_noclip = 0;
	poly_clip = 0;

	//R_ClearSky();

	R_RecursiveWorldNode(currentmodel->nodes);

	R_DrawTextureChains();

	R_DrawSky();

#if 0
	printf("Clipping stats:\n");
	printf("Force Noclip:  % 4d\n", poly_forcenoclip);
	printf("Oversized:     % 4d\n", poly_oversized);
	printf("Early Outside: % 4d\n", poly_earlyoutside);
	printf("Early Noclip:  % 4d\n", poly_earlynoclip);
	printf("Outside:       % 4d\n", poly_outside);
	printf("Noclip:        % 4d\n", poly_noclip);
	printf("Clip:          % 4d\n", poly_clip);
	printf("All:           % 4d\n", poly_all);
#endif
}

void R_BuildPolygonFromSurface(msurface_t* fa) {
	int			lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	vec3_t		total;

	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	VectorClear(total);
	
	brushpoly_t* poly = (brushpoly_t*)ri.Hunk_Alloc(sizeof(brushpoly_t) + sizeof(brushvertex_t) * lnumverts);
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	float minx =  1e10f;
	float miny =  1e10f;
	float minz =  1e10f;
	float maxx = -1e10f;
	float maxy = -1e10f;
	float maxz = -1e10f;

	int i;
	for(i = 0; i < lnumverts; i++) {
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0) {
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		} else {
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		VectorAdd(total, vec, total);

		poly->verts[i].pos.x = vec[0];
		poly->verts[i].pos.y = vec[1];
		poly->verts[i].pos.z = vec[2];

		if(vec[0] < minx) minx = vec[0];
		if(vec[1] < miny) miny = vec[1];
		if(vec[2] < minz) minz = vec[2];
		if(vec[0] > maxx) maxx = vec[0];
		if(vec[1] > maxy) maxy = vec[1];
		if(vec[2] > maxz) maxz = vec[2];

		{ // texture coords
			float s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			s /= fa->texinfo->image->width;
			poly->verts[i].tex.x = s;

			float t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
			t /= fa->texinfo->image->height;
			poly->verts[i].tex.y = t;
		}

#if 0
		{ // lightmap coords
			float s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			s -= fa->texturemins[0];
			s += fa->light_s * 16;
			s += 8;
			s /= BLOCK_WIDTH * 16;
			poly->verts[i].light_s = s;

			float t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
			t -= fa->texturemins[1];
			t += fa->light_t * 16;
			t += 8;
			t /= BLOCK_HEIGHT * 16;
			poly->verts[i].light_t = t;
		}
#endif
	}

	poly->sphere.pos.x = (maxx + minx) * 0.5f;
	poly->sphere.pos.y = (maxy + miny) * 0.5f;
	poly->sphere.pos.z = (maxz + minz) * 0.5f;
	poly->sphere.rad   = 0.0f;

	for(int i = 0; i < poly->numverts; i++) {
		GUVec3 v;
		GUMath_Sub(&v, &poly->verts[i].pos, &poly->sphere.pos);
		float t = GUMath_Length(&v);
		if(t > poly->sphere.rad) {
			poly->sphere.rad = t;
		}
	}

#if 0
	printf("POLYGON DATA\n");
	printf("Vertex Count: %d\n", poly->numverts);
	for(int i = 0; i < poly->numverts; i++) {
		printf("Vertex: % 5.2f % 5.2f % 5.2f\n", poly->verts[i].pos.x, poly->verts[i].pos.y, poly->verts[i].pos.z);
	}
	printf("Min   : % 5.2f % 5.2f % 5.2f\n", minx, miny, minz);
	printf("Max   : % 5.2f % 5.2f % 5.2f\n", maxx, maxy, maxz);
	printf("Center: % 5.2f % 5.2f % 5.2f\n", poly->sphere.pos.x, poly->sphere.pos.y, poly->sphere.pos.z);
	printf("Radius: % 5.2f\n", poly->sphere.rad);

	sceKernelDelayThread(300 * 1000 * 1000);
#endif
}

void R_DrawInlineBModel(void) {
	msurface_t* surf = &currentmodel->surfaces[currentmodel->firstmodelsurface];
	int i;
	for(i = 0; i < currentmodel->nummodelsurfaces; i++, surf++) {
		cplane_t* plane = surf->plane;

		float dot = DotProduct(modelorg, plane->normal) - plane->dist;
		if(((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
		  (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
#if 0
			R_RenderBrushPoly(surf, false);
#else
			image_t* img = R_TextureAnimation(surf->texinfo);
			GU_Bind(img);

			GU_DrawPoly(surf->polys, 0);
#endif
		}
	}
}

void R_DrawBrushModel(entity_t* e) {
	if(currentmodel->nummodelsurfaces == 0) {
		return;
	}

	currententity = e;

	qboolean rotated;
	vec3_t mins, maxs;
	if(e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		int i;
		for(i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	} else {
		rotated = false;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if(R_CullBox(mins, maxs)) {
		return;
	}

	VectorSubtract(r_newrefdef.vieworg, e->origin, modelorg);
	if(rotated) {
		vec3_t temp;
		vec3_t forward, right, up;
		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] =  DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] =  DotProduct(temp, up);
	}

	sceGumPushMatrix();
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];
	GU_RotateForEntity(e);
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	R_DrawInlineBModel();

	sceGumPopMatrix();
}
