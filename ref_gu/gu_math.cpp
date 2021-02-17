#include <psptypes.h>

#include <math.h>

#include "gu_local.h"

#define	PITCH	0
#define	YAW		1
#define	ROLL	2

// Basic Vector ops

void GUMath_Cpy3(GUVec3* dst, const GUVec3* src) {
	int i;
	for(i = 0; i < 3; i++) {
		dst->a[i] = src->a[i];
	}
}

void GUMath_Add(GUVec3* dst, const GUVec3* src0, const GUVec3* src1) {
	int i;
	for(i = 0; i < 3; i++) {
		dst->a[i] = src0->a[i] + src1->a[i];
	}
}

void GUMath_Sub(GUVec3* dst, const GUVec3* src0, const GUVec3* src1) {
	int i;
	for(i = 0; i < 3; i++) {
		dst->a[i] = src0->a[i] - src1->a[i];
	}
}

void GUMath_Inv3(GUVec3* dst, const GUVec3* src) {
	int i;
	for(i = 0; i < 3; i++) {
		dst->a[i] = -src->a[i];
	}
}

void GUMath_Mul(GUVec3* dst, const GUVec3* src0, const float src1) {
	int i;
	for(i = 0; i < 3; i++) {
		dst->a[i] = src0->a[i] * src1;
	}
}

// Special Vector 3 ops

void GUMath_Cross(GUVec3* dst, const GUVec3* src0, const GUVec3* src1) {
	dst->a[0] = src0->a[1] * src1->a[2] - src0->a[2] - src1->a[1];
	dst->a[0] = src0->a[2] * src1->a[0] - src0->a[0] - src1->a[2];
	dst->a[0] = src0->a[0] * src1->a[1] - src0->a[1] - src1->a[0];
}

float GUMath_Dot(const GUVec3* src0, const GUVec3* src1) {
	return src0->x * src1->x + src0->y * src1->y + src0->z * src1->z;
}

float GUMath_Length(const GUVec3* src) {
	float len = 0.0f;
	int i;
	for(i = 0; i < 3; i++) {
		len += src->a[i] * src->a[i];
	}
	return sqrtf(len);
}

float GUMath_Normalize(GUVec3* dst, const GUVec3* src) {
	float len = GUMath_Length(src);

	if(len) {
		float ilen = 1.0f / len;
		int i;
		for(i = 0; i < 3; i++) {
			dst->a[i] = src->a[i] * ilen;
		}
	}

	return len;
}

// Other

#if 0
void GUMath_AngleVectors(vec3* fw, vec3* rg, vec3* up, const vec3* src) {
	float angle;

	angle = src->a[YAW] * (M_PI / 180.0f);
	float cy = cosf(angle);
	float sy = sinf(angle);

	angle = src->a[PITCH] * (M_PI / 180.0f);
	float cp = cosf(angle);
	float sp = sinf(angle);

	angle = src->a[ROLL] * (M_PI / 180.0f);
	float cr = cosf(angle);
	float sr = sinf(angle);

	if(fw) {
		fw->x = cp * cy;
		fw->y = cp * sy;
		fw->z = -sp;
	}
	if(rg) {
		rg->x = (-1.0f * sr * sp * cy) + (-1.0f * cr * -sy);
		rg->y = (-1.0f * sr * sp * sy) + (-1.0f * cr * cy);
		rg->z = -1.0f * sr * cp;
	}
	if(up) {
		up->x = (cr * sp * cy) + (-sr * -sy);
		up->y = (cr * sp * sy) + (-sr * cy);
		up->z = cr * cp;
	}
}
#endif

/*
void Math_RotatePoint(ScePspFVector3* dst3, ScePspFVector3* dir, ScePspFVector3* point, float angle) {
	ScePspFVector3 vf;
	vf = dir;
}
*/

/*
void GUMath_ProjectPointOnPlane(vec3* dst, const vec3* point, const vec3* normal) {
	//float inv
}
*/

/*
void GUMath_PerpendicularVector(ScePspFVector3* dst, const ScePspFVector3* src) {
#if 0
	float min = 1.0f;
	int pos = -1;

	int i;
	for(i = 0; i < 3; i++) {
		if(fabsf(src->a[i]) < min) {
			pos = i;
			min = fabsf(src->a[i]);
		}
	}

	ScePspFVector3 temp = { .a = { 0.0f, 0.0f, 0.0f } };
	temp.a[pos] = 1.0f;

	Math_ProjectPointOnPlane(dst, &temp, src);
#endif
}
*/
