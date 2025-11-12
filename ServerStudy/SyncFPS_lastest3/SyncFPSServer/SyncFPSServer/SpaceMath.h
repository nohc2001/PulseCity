#pragma once
#include <iostream>
#include <Windows.h>
#include <bit>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

typedef unsigned int ui32;
typedef unsigned long long ui64;

#define bitcast(t, a) _Bit_cast<t>(a)

#define __property(S) __declspec(property(get=Get##S, put=Set##S))
#define __property_from_index(T, A, N, I) \
	private:\
	__forceinline T Get##N##() const {return A[I];} \
	__forceinline void Set##N##(T v) {A[I] = v;} \
	public:\
	__property(N) T

#define AddUnit(N, M) \
constexpr long double operator"" ##N##(long double val) { return val * M; } \
constexpr unsigned long long operator"" ##N##(unsigned long long val) { return (long double)val * M; }

#pragma region DistanceUnit
AddUnit(km, 1000)
AddUnit(m, 1)
AddUnit(cm, 0.01)
AddUnit(mm, 0.001)
#pragma endregion
#pragma region WeightUnit
AddUnit(ton, 1000)
AddUnit(kg, 1)
AddUnit(g, 0.001)
#pragma endregion
#pragma region TimeUnit
AddUnit(ms, 0.001)
AddUnit(sec, 1)
AddUnit(minute, 60)
AddUnit(hour, 3600)
AddUnit(day, 3600 * 24)
#pragma endregion
#pragma region SpeedUnit
AddUnit(mps, 1)
AddUnit(kmph, (0.2777777777777778))
#pragma endregion

float sign_spread(float Dest, float Source) {
	ui32 f = bitcast(ui32, Source) & 0x80000000;
	ui32 g = bitcast(ui32, Dest) & 0x7FFFFFFF;
	return bitcast(float, f | g);
}

//vec4 x1 : position, color, vector, dir, angle, plane, querternien, bound
//vec4 x2 : cube range(AABB), ray, line
//vec4 x3 : triangle, OBB
//vec4 x4 : matrix
struct vec4 {
	union {
		XMFLOAT3 f3;
		XMFLOAT4 f4;
		XMVECTOR v4;
		struct {
			float x;
			float y;
			float z;
			float w;
		};
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		__m128i ssei;
		__m128 sse;
	};

	operator XMFLOAT4() const { return f4; }
	operator XMVECTOR() const { return v4; } // can using operator of XMVECTOR. +,-,*,/

	vec4() {
		v4 = XMVectorZero();
	}
	vec4(std::initializer_list<float> initList) {
		auto iter = initList.begin();
		x = *iter;
		y = *++iter;
		z = *++iter;
		w = *++iter;
	}
	__vectorcall vec4(float _x, float _y, float _z, float _w = 0.0f) {
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}
	vec4(const vec4& ref) {
		v4 = ref.v4;
	}
	vec4(const XMVECTOR& ref) {
		v4 = ref;
	}
	vec4(const XMFLOAT4& ref) {
		f4 = ref;
	}
	vec4(XMFLOAT3 ref) {
		f3 = ref;
		w = 0;
	}
	vec4(float f) {
		v4 = XMVectorSet(f, f, f, f);
	}
	__forceinline void operator=(float f) {
		v4 = XMVectorSet(f, f, f, f);
	}
	__forceinline void operator=(const XMVECTOR& v) {
		v4 = v;
	}
	__forceinline void operator=(const XMFLOAT4& f) {
		f4 = f;
		vec4 v;
	}
#pragma region PrivateLengthFuncs
	__forceinline const float getlen2() const { return XMVector2Length(v4).m128_f32[0]; }
	__forceinline void SetLen2D(float len) { v4 = XMVector2Normalize(v4) * len; }
	__forceinline const float getlen3() const { return XMVector3Length(v4).m128_f32[0]; }
	__forceinline void SetLen3D(float len) {
		float w = f4.w;
		v4 = XMVector3Normalize(v4) * len;
		f4.w = w;
		XMVector3Normalize(*this);
	}
	__forceinline const float getlen4() const { return XMVector4Length(v4).m128_f32[0]; }
	__forceinline void SetLen4D(float len) { v4 = XMVector4Normalize(v4) * len; }

	__forceinline const float getrecip_len2() const { return XMVector2ReciprocalLength(v4).m128_f32[0]; }
	__forceinline const float getrecip_len3() const { return XMVector3ReciprocalLength(v4).m128_f32[0]; }
	__forceinline const float getrecip_len4() const { return XMVector4ReciprocalLength(v4).m128_f32[0]; }

	__forceinline const float getsq_len2() const { return XMVector2LengthSq(v4).m128_f32[0]; }
	__forceinline const float getsq_len3() const { return XMVector3LengthSq(v4).m128_f32[0]; }
	__forceinline const float getsq_len4() const { return XMVector4LengthSq(v4).m128_f32[0]; }
#pragma endregion
	__declspec(property(get = getlen2, put = SetLen2D)) float len2;
	__declspec(property(get = getlen3, put = SetLen3D)) float len3;
	__declspec(property(get = getlen4, put = SetLen4D)) float len4;

	//A * recip_len = A / len;
	__declspec(property(get = getrecip_len3)) const float recip_len2;
	__declspec(property(get = getrecip_len3)) const float recip_len3;
	__declspec(property(get = getrecip_len4)) const float recip_len4;

	//v1.fast_square_of_len3 > v2.fast_square_of_len3 // is faster than v1.len3 > v2.len3
	__declspec(property(get = getsq_len2)) const float fast_square_of_len2;
	__declspec(property(get = getsq_len3)) const float fast_square_of_len3;
	__declspec(property(get = getsq_len4)) const float fast_square_of_len4;

	__forceinline void lengthLimit(float lmin, float lmax) { v4 = XMVector4ClampLength(v4, lmin, lmax); }

	__forceinline const XMVECTOR tr4(const XMMATRIX& mat) { return XMVector4Transform(v4, mat); }
	__forceinline const XMVECTOR tr3w1(const XMMATRIX& mat) { return XMVector3TransformCoord(v4, mat); }
	__forceinline const XMVECTOR tr3w0(const XMMATRIX& mat) { return XMVector3TransformNormal(v4, mat); }

	__forceinline const XMVECTOR operator*(const XMMATRIX& mat) { return XMVector4Transform(v4, mat); }
	__forceinline const void operator*=(const XMMATRIX& mat) { v4 = XMVector4Transform(v4, mat); }

	__forceinline const void operator+=(float f) { v4 += vec4(f); }
	__forceinline const void operator-=(float f) { v4 -= vec4(f); }
	__forceinline const void operator*=(float f) { v4 *= f; }
	__forceinline const void operator/=(float f) { v4 /= f; }
	__forceinline const void operator+=(const XMVECTOR& ref) { v4 += ref; }
	__forceinline const void operator-=(const XMVECTOR& ref) { v4 -= ref; }
	__forceinline const void operator*=(const XMVECTOR& ref) { v4 *= ref; }
	__forceinline const void operator/=(const XMVECTOR& ref) { v4 /= ref; }

	__forceinline const float dot2(const vec4& ref) const { return XMVector2Dot(v4, ref).m128_f32[0]; }
	__forceinline const float dot3(const vec4& ref) const { return XMVector3Dot(v4, ref).m128_f32[0]; }
	__forceinline const float dot4(const vec4& ref) const { return XMVector4Dot(v4, ref).m128_f32[0]; }

	__forceinline const XMVECTOR cross(const vec4& ref) const { return XMVector3Cross(v4, ref); }
	__forceinline const XMVECTOR cross4(const vec4& ref1, const vec4& ref2) const { return XMVector4Cross(v4, ref1, ref2); }

	__forceinline const bool operator==(vec4& ref) const { return XMColorEqual(v4, ref); }
	__forceinline const bool operator!=(vec4& ref) const { return !XMColorEqual(v4, ref); }
	__forceinline const bool is_contain_inf() const { return XMVector4IsInfinite(v4); }
	__forceinline const bool is_contain_nan() const { return XMVector4IsNaN(v4); }

	//bound vector = {a, b, c, d} // xmin : -a, xmax : a ...
	//self vec4 is position vector.
	__forceinline const bool is_in_bound(const vec4& bound) {
		return XMVector4InBounds(v4, bound);
	}

	//rotation and Querternion
	void setQ(const vec4& ref) { v4 = XMQuaternionRotationRollPitchYaw(ref.x, ref.y, ref.z); }
	__forceinline static vec4 getQ(const vec4& ref) {
		vec4 r;
		r = XMQuaternionRotationRollPitchYaw(ref.x, ref.y, ref.z);
		return r;
	}
	void trQ(const vec4& Q) { XMVector3Rotate(v4, Q); }
	void trQinv(const vec4& Q) { XMVector3InverseRotate(v4, Q); }
	const vec4& invQ() const { return XMQuaternionInverse(v4); }
	const vec4& mulQ(const vec4& Q) const { // A+Q
		return XMQuaternionMultiply(v4, Q);
	}
	const vec4& getQdiff(const vec4& B) { // A-B
		return mulQ(B.invQ());
	}
	XMVECTOR ortho() const { return XMVector3Orthogonal(v4); }

	__forceinline const vec4& suffle(int x, int y, int z, int w) {
		return XMVectorSwizzle(v4, x, y, z, w);
	}

	template <int x, int y, int z, int w>
	__forceinline const vec4& constSuffle() {
		return XMVectorSwizzle<x, y, z, w>(v4);
	}

	__forceinline const ui32 GetMask() {
		return _mm_movemask_ps(_mm_castsi128_ps(ssei));
	}

	static __forceinline const vec4& getPlane(const vec4& pos, const vec4& normal) {
		vec4 posnor = pos * normal;
		vec4 plane = normal;
		plane.w = -posnor.fast_square_of_len3;
		return plane;
	}

	__forceinline float getPlaneDistance(const vec4& pos) {
		return fabsf(XMPlaneDot(v4, pos).m128_f32[0]);
	}
};

// temporaly.. function.
__forceinline float GetAngle(const vec4& v1, const vec4& v2) {
	return XMVector3AngleBetweenVectors(v1, v2).m128_f32[0];
}
__forceinline float GetAngleNormal(const vec4& v1, const vec4& v2) {
	return XMVector3AngleBetweenNormals(v1, v2).m128_f32[0];
}

struct matrix {
	union {
		XMMATRIX mat;
		XMFLOAT4X4 f16;
		struct {
			vec4 right;
			vec4 up;
			vec4 look;
			vec4 pos;
		};
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
	};

	operator XMMATRIX() const { return mat; }
	operator XMFLOAT4X4() const { return f16; }

	matrix() { Id(); }
	matrix(const matrix& ref) {
		mat = ref.mat;
	}
	matrix(const XMMATRIX& ref) {
		mat = ref;
	}

	void Id() { mat = XMMatrixIdentity(); }

	const matrix& operator*(const matrix& ref) {
		return XMMatrixMultiply(mat, ref);
	}
	void operator*=(const matrix& ref) {
		mat = XMMatrixMultiply(mat, ref);
	}

	void SetLook(const vec4& look, const vec4& up = { 0, 1, 0, 0 }) {
		matrix xmf4x4View = XMMatrixLookAtLH(pos, pos + look, up);
		_11 = xmf4x4View._11;
		_12 = xmf4x4View._21;
		_13 = xmf4x4View._31;
		_21 = xmf4x4View._12;
		_22 = xmf4x4View._22;
		_23 = xmf4x4View._32;
		_31 = xmf4x4View._13;
		_32 = xmf4x4View._23;
		_33 = xmf4x4View._33;
	}

	__forceinline void transpose() {
		mat = XMMatrixTranspose(mat);
	}

	__forceinline void tr4_arr(vec4* invecArr, vec4* outvecArr, int count) {
		XMVector4TransformStream((XMFLOAT4*)outvecArr, 16, (XMFLOAT4*)invecArr, 16, count, mat);
	}
	__forceinline void tr3w1_arr(vec4* invecArr, vec4* outvecArr, int count) {
		XMVector3TransformCoordStream((XMFLOAT3*)outvecArr, 16, (XMFLOAT3*)invecArr, 16, count, mat);
	}
	__forceinline void tr3w0_arr(vec4* invecArr, vec4* outvecArr, int count) {
		XMVector3TransformNormalStream((XMFLOAT3*)outvecArr, 16, (XMFLOAT3*)invecArr, 16, count, mat);
	}

	void trQ(const vec4& Q) { mat *= XMMatrixRotationQuaternion(Q); }
	void trQinv(const vec4& Q) { mat *= XMMatrixRotationQuaternion(Q.invQ()); }
	const vec4& getQ() {
		return XMQuaternionRotationMatrix(mat);
	}

	__forceinline const matrix& InverseRTMat() const {
		matrix tmat = *this;
		tmat.pos = XMVectorSet(0, 0, 0, 1);
		tmat.transpose();
		tmat.pos = XMVectorSet(-pos.dot3(right), -pos.dot3(up), -pos.dot3(look), 1);
		return tmat;
	}
	__declspec(property(get = InverseRTMat)) matrix RTInverse;
};

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
};

OBB_vertexVector GetOBBVertexs(BoundingOrientedBox obb)
{
	OBB_vertexVector ovv;
	matrix mat;
	mat.Id();
	mat.trQ(obb.Orientation);
	vec4 xm = -obb.Extents.x * mat.right;
	vec4 xp = obb.Extents.x * mat.right;
	vec4 ym = -obb.Extents.y * mat.up;
	vec4 yp = obb.Extents.y * mat.up;
	vec4 zm = -obb.Extents.z * mat.look;
	vec4 zp = obb.Extents.z * mat.look;

	vec4 pos = obb.Center;
	ovv.vertex[0][0][0] = xm + ym + zm + pos;
	ovv.vertex[0][0][1] = xm + ym + zp + pos;
	ovv.vertex[0][1][0] = xm + yp + zm + pos;
	ovv.vertex[0][1][1] = xm + yp + zp + pos;
	ovv.vertex[1][0][0] = xp + ym + zm + pos;
	ovv.vertex[1][0][1] = xp + ym + zp + pos;
	ovv.vertex[1][1][0] = xp + yp + zm + pos;
	ovv.vertex[1][1][1] = xp + yp + zp + pos;
	return ovv;
}

vec4 GetPlane_FrustomRange(BoundingOrientedBox obb, vec4 point) {
	static vec4 One = XMVectorSetInt(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

	matrix mat;
	mat.Id();
	mat.trQ(obb.Orientation);
	mat.pos = obb.Center;

	matrix InvMat = mat.RTInverse;
	vec4 id = point * InvMat;
	vec4 E = obb.Extents;

	vec4 PlanePos = mat.pos;
	ui32 index = 0;
	bool b;

	vec4 d = XMVectorAbs(id);

	if (d.x != 0) {
		b = d.y / d.x < E.y / E.x;
		b = b && (d.z / d.x < E.z / E.x);
		if (b) {
			index = 0;
			goto RETURN;
		}
	}
	if (d.y != 0) {
		b = d.x / d.y < E.x / E.y;
		b = b && (d.z / d.y < E.z / E.y);
		if (b) {
			index = 1;
			goto RETURN;
		}
	}
	if (d.z != 0) {
		b = d.x / d.z < E.x / E.z;
		b = b && (d.y / d.z < E.y / E.z);
		if (b) {
			index = 2;
			goto RETURN;
		}
	}

RETURN:
	float sign = 1;
	sign = sign_spread(sign, id.v4.m128_f32[index]);
	mat.mat.r[index] *= sign;
	PlanePos += E.v4.m128_f32[index] * mat.mat.r[index];
	return vec4::getPlane(PlanePos, mat.mat.r[index]);


	/*vec4 dsv1 = d.constSuffle<2, 0, 1, 3>();
	vec4 dsv2 = d.constSuffle<1, 2, 0, 3>();
	vec4 Esv1 = E.constSuffle<1, 2, 0, 3>();
	vec4 Esv2 = E.constSuffle<2, 0, 1, 3>();
	vec4 C2 = 2 * d;

	vec4 O1 = Esv1 * dsv1;
	vec4 O2 = Esv2 * dsv2;
	vec4 C1 = Esv1 * Esv2;

	O1 *= O2;
	C1 *= C2;
	O1 *= E;

	O2 = XMVectorLess(C1, O1);
	O2 = XMVectorEqualInt(O2, One);
	ui32 mod = O2.GetMask();

	float sign_mask_float = 0x80000000;
	float sign = 1.0f;

	vec4 PlanePos = mat.pos;
	vec4 PlaneNormal;

	if (mod == 0) return vec4(0);

	int index = _tzcnt_u32(mod);
	sign = sign_spread(sign, d.v4.m128_f32[index]);
	mat.mat.r[index] *= sign;
	PlanePos += E.v4.m128_f32[index] * mat.mat.r[index];
	return vec4::getPlane(PlanePos, mat.mat.r[index]);*/
}