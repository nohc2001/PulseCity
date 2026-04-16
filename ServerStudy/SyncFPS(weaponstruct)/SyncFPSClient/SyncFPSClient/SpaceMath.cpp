#include "SpaceMath.h"

inline BoundingOrientedBox GetBottomOBB(const BoundingOrientedBox& obb) {
	constexpr float margin = 0.1f;
	BoundingOrientedBox robb;
	robb.Center = obb.Center;
	robb.Center.y -= obb.Extents.y;
	robb.Extents = obb.Extents;
	robb.Extents.y = 0.4f;
	robb.Extents.x -= margin;
	robb.Extents.z -= margin;
	robb.Orientation = obb.Orientation;
	return robb;
}

bool PointInTriangle(XMVECTOR point, XMVECTOR p0, XMVECTOR p1, XMVECTOR p2)
{
	XMVECTOR v0 = p1 - p0;
	XMVECTOR v1 = p2 - p1;
	XMVECTOR v2 = p0 - p2;
	XMVECTOR normal = XMVector3Cross(v1, v2);
	normal = XMVector3Normalize(normal);
	XMVECTOR e0 = XMVector3Cross(v0, normal);
	XMVECTOR w0 = p0 - point;
	if (XMVector3Dot(w0, e0).m128_f32[0] < 0.0f) return(false);
	XMVECTOR e1 = XMVector3Cross(v1, normal);
	XMVECTOR w1 = p1 - point;
	if (XMVector3Dot(w1, e1).m128_f32[0] < 0.0f) return(false);
	XMVECTOR e2 = XMVector3Cross(v2, normal);
	XMVECTOR w2 = p2 - point;
	if (XMVector3Dot(w2, e2).m128_f32[0] < 0.0f) return(false);
	return(true);
}

bool bPointInTriangleRange(float px, float py, float t0x, float t0y, float t1x, float t1y, float t2x, float t2y) {
	/*XMVECTOR p = XMVectorSet(px, py, 0, 0);
	XMVECTOR t0 = XMVectorSet(t0x, t0y, 0, 0);
	XMVECTOR t1 = XMVectorSet(t1x, t1y, 0, 0);
	XMVECTOR t2 = XMVectorSet(t2x, t2y, 0, 0);
	XMVECTOR normal = XMVectorSet(0, 0, -1, 0);
	XMVECTOR v01 = XMVector3Normalize(t1 - t0);
	XMVECTOR v01R = XMVector3Cross(v01, normal);
	XMVECTOR v12 = XMVector3Normalize(t2 - t1);
	XMVECTOR v12R = XMVector3Cross(v12, normal);
	XMVECTOR v20 = XMVector3Normalize(t0 - t2);
	XMVECTOR v20R = XMVector3Cross(v20, normal);
	float f0 = XMVector3Dot(v01R, (t0 - p)).m128_f32[0];
	float f1 = XMVector3Dot(v12R, (t1 - p)).m128_f32[0];
	float f2 = XMVector3Dot(v20R, (t2 - p)).m128_f32[0];
	bool b = f0 > 0 &&
		f1 > 0 &&
		f2 > 0;
	return b;*/

	return PointInTriangle(XMVectorSet(px, py, 0, 0), XMVectorSet(t0x, t0y, 0, 0), XMVectorSet(t1x, t1y, 0, 0), XMVectorSet(t2x, t2y, 0, 0));
}

bool bTriangleInPolygonRange(float t0x, float t0y, float t1x, float t1y, float t2x, float t2y, std::vector<XMFLOAT3> polygon)
{
	XMVECTOR gcenter = XMVectorSet(t0x + t1x + t2x, t0y + t1y + t2y, 0, 0);
	gcenter = gcenter / 3;
	bool result = true;
	if (bPointInPolygonRange(gcenter, polygon) == false) {
		return false;
	}

	XMVECTOR considerP = (gcenter + XMVectorSet(t0x, t0y, 0, 0) * 3) / 4;
	if (bPointInPolygonRange(considerP, polygon) == false) {
		return false;
	}

	considerP = (gcenter + XMVectorSet(t1x, t1y, 0, 0) * 3) / 4;
	if (bPointInPolygonRange(considerP, polygon) == false) {
		return false;
	}

	considerP = (gcenter + XMVectorSet(t2x, t2y, 0, 0) * 3) / 4;
	if (bPointInPolygonRange(considerP, polygon) == false) {
		return false;
	}

	// too many sample to find include triangle.
	for (float a = 0.1f; a < 0.9f; a += 0.1f) {
		float b = 1 - a;

		considerP = a * gcenter + b * XMVectorSet(t0x, t0y, 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}

		considerP = a * gcenter + b * XMVectorSet(t1x, t1y, 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}

		considerP = a * gcenter + b * XMVectorSet(t2x, t2y, 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}

		/*considerP = XMVectorSet(a * t0x + b * t1x + 0.1f * gcenter.m128_f32[0], a * t0y + b * t1y + 0.1f * gcenter.m128_f32[1], 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}

		considerP = XMVectorSet(a * t1x + b * t2x + 0.1f * gcenter.m128_f32[0], a * t1y + b * t2y + 0.1f * gcenter.m128_f32[1], 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}

		considerP = XMVectorSet(a * t2x + b * t0x + 0.1f * gcenter.m128_f32[0], a * t2y + b * t0y + 0.1f * gcenter.m128_f32[1], 0, 0);
		if (bPointInPolygonRange(considerP, polygon) == false) {
			return false;
		}*/
	}

	return true;
}