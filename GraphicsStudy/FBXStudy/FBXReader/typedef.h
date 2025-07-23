#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <DirectXMath.h>

using namespace DirectX;

#define mcvt(t) reinterpret_cast<t>

struct WinEvent {
    HWND hWnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};

struct BasicVertex
{
	XMFLOAT3 position;
    XMFLOAT3 normal;
	XMFLOAT2 texcoord;
    XMFLOAT3 tangent;
    XMFLOAT3 bitangent;

    bool HaveToSetTangent() {
        bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
        b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));

        b |= ((bitangent.x == 0 && bitangent.y == 0) && bitangent.z == 0);
        b |= ((isnan(bitangent.x) || isnan(bitangent.y)) || isnan(bitangent.z));
        return b;
    }

    static XMVECTOR GetVec(XMFLOAT3 f3) {
        XMVECTOR v;
        v.m128_f32[0] = f3.x;
        v.m128_f32[1] = f3.y;
        v.m128_f32[2] = f3.z;
        v.m128_f32[3] = 0;
        return v;
    }

    static XMVECTOR GetVec(XMFLOAT2 f2) {
        XMVECTOR v;
        v.m128_f32[0] = f2.x;
        v.m128_f32[1] = f2.y;
        v.m128_f32[2] = 0;
        v.m128_f32[3] = 0;
        return v;
    }

    static void CreateTBN(BasicVertex& P0, BasicVertex& P1, BasicVertex& P2) {
        XMVECTOR N1, N2, M1, M2;
        //XMFLOAT3 N0, N1, M0, M1;
        N1 = GetVec(P1.texcoord) - GetVec(P0.texcoord);
        N2 = GetVec(P2.texcoord) - GetVec(P0.texcoord);
        M1 = GetVec(P1.position) - GetVec(P0.position);
        M2 = GetVec(P2.position) - GetVec(P0.position);

        float f = 1.0f / (N1.m128_f32[0] * N2.m128_f32[1] - N2.m128_f32[0] * N1.m128_f32[1]);
        XMVECTOR tan = GetVec(P0.tangent);
        tan = f * (M1 * N2.m128_f32[1] - M2 * N1.m128_f32[1]);
        XMVECTOR v = XMVector3Normalize(tan);
        P0.tangent = { v.m128_f32[0], v.m128_f32[1], v.m128_f32[2] };

        XMVECTOR bit = GetVec(P0.bitangent);
        bit = f * (M2 * N1.m128_f32[0] - M1 * N2.m128_f32[0]);
        v = XMVector3Normalize(bit);
        P0.bitangent = { v.m128_f32[0], v.m128_f32[1], v.m128_f32[2] };

        if (P0.HaveToSetTangent()) {
            // why tangent is nan???
            XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
            XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
            XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
            BinV = XMVector3Cross(NorV, M1);
            TanV = XMVector3Cross(NorV, BinV);
            P0.tangent = { 1, 0, 0 };
            P0.bitangent = { 0, 0, 1 };
        }
    }

    bool operator==(const BasicVertex& ref) const {
        bool b = (position.x == ref.position.x);
        b &= (position.y == ref.position.y);
        b &= (position.z == ref.position.z);
        b &= (texcoord.x == ref.texcoord.x);
        b &= (texcoord.y == ref.texcoord.y);
        b &= (normal.x == ref.normal.x);
        b &= (normal.y == ref.normal.y);
        b &= (normal.z == ref.normal.z);
        b &= (tangent.x == ref.tangent.x);
        b &= (tangent.y == ref.tangent.y);
        b &= (tangent.z == ref.tangent.z);
        b &= (bitangent.x == ref.bitangent.x);
        b &= (bitangent.y == ref.bitangent.y);
        b &= (bitangent.z == ref.bitangent.z);
        return b;
    }
};

struct IndexX3 {
    int pos_index;
    int uv_index;
    int normal_index;

    bool operator==(IndexX3 i) {
        if (pos_index == i.pos_index && (uv_index == i.uv_index && normal_index == i.normal_index))
            return true;
        return false;
    }

    IndexX3() {}
    IndexX3(int a0, int a1, int a2) {
        pos_index = a0;
        uv_index = a1;
        normal_index = a2;
    }
};

struct TriangleIndex {
    WORD v[3];

    bool operator==(TriangleIndex ti) {
        v[0] = ti.v[0];
        v[1] = ti.v[1];
        v[2] = ti.v[2];
    }

    TriangleIndex() {}
    TriangleIndex(WORD a0, WORD a1, WORD a2) {
        v[0] = a0;
        v[1] = a1;
        v[2] = a2;
    }
};

union RGBA
{
	struct
	{
		BYTE	r;
		BYTE	g;
		BYTE	b;
		BYTE	a;
	};
	BYTE		bColorFactor[4];

    RGBA() {}

    RGBA(const float& R, const float& G, const float B, const float& A) {
        r = R;
        g = G;
        b = B;
        a = A;
    }
};

struct DirLight
{
    XMFLOAT3 direction;

    XMFLOAT3 ambient;
    XMFLOAT3 diffuse;
    XMFLOAT3 specular;

    DirLight() {
        direction = {};
        ambient = {};
        diffuse = {};
        specular = {};
    }
};

struct PointLight
{
    XMFLOAT3 position;
    float intencity;
    float range;

    PointLight() {
        position = {};
        intencity = 0;
        range = 0;
    }

    PointLight(XMFLOAT3 pos, float power, float Range) {
        position = pos;
        intencity = power;
        range = Range;
    }
};

struct SpotLight
{
    XMFLOAT3 position;
    XMFLOAT3 direction;

    float intencity;
    float range;

    float cutOff;
    float outerCutOff;

    XMFLOAT3 ambient;
    XMFLOAT3 diffuse;
    XMFLOAT3 specular;

    SpotLight() {
        position = {};
        direction = {};

        intencity = 0;
        range = 0;
        cutOff = 0;
        outerCutOff = 0;

        ambient = {};
        diffuse = {};
        specular = {};
    }
};

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

struct PlaneEquation {
    float nX;
    float nY;
    float nZ;
    float W;

    void SetPlane(XMVECTOR normal, XMVECTOR pos) {
        XMVECTOR v = XMVector3Normalize(normal);
        nX = v.m128_f32[0];
        nY = v.m128_f32[1];
        nZ = v.m128_f32[2];
        float d = nX * pos.m128_f32[0] + nY * pos.m128_f32[1] + nZ * pos.m128_f32[2];
        W = -d;
    }

    void SetPlane(XMVECTOR p0, XMVECTOR p1, XMVECTOR p2, XMVECTOR innerPos) {
        XMVECTOR normal;
        normal = XMVector3Cross(p1 - p0, p2 - p0);
        if (XMVector3Dot(normal, innerPos).m128_f32[0] < 0) {
            normal *= -1.0f;
        }
        SetPlane(normal, p0);
    }

    bool IsVisiblePlaneAtPos(float x, float y, float z) {
        float v = nX * x + nY * y + nZ * z + W;
        if (v > 0) {
            return true;
        }
    }
};

extern DWORD m_dwWidth;
extern DWORD m_dwHeight;

struct CONSTANT_BUFFER_DEFAULT
{
    static XMMATRIX ViewMatrix;
    static XMMATRIX ProjectMatrix;
    static XMFLOAT3 SCamPos;
    static PlaneEquation pexpr[6];
    static XMVECTOR AtVector;
    static float farZ;
    static float nearZ;
    static XMMATRIX CameraWorldMatrix;

    XMMATRIX model;
    XMMATRIX view;
    XMMATRIX project;
    //XMFLOAT3 ViewPos;
    //float material_ambientRate;
    //float material_specularRate;
    //float material_shininess;
    PointLight pointLights;
    XMFLOAT3 CamPos;
    
    CONSTANT_BUFFER_DEFAULT() {
        model = XMMatrixIdentity();
        view = XMMatrixIdentity();
        project = XMMatrixIdentity();
        //dirLight = DirLight();
        //for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        //    pointLights[i] = PointLight();
            //spotLights[i] = SpotLight();
        //}

        pointLights = PointLight();
       /* ViewPos = {};
        material_ambientRate = 0;
        material_specularRate = 0;
        material_shininess = 0;*/
    }

    void GetCamModelCB(const XMFLOAT3& pos, const XMFLOAT3& rot, const XMFLOAT3& sca) {
        XMMATRIX ObjWorld = XMMatrixIdentity();
        ObjWorld = XMMatrixScaling(sca.x, sca.y, sca.z);
        ObjWorld = XMMatrixMultiply(ObjWorld, XMMatrixRotationX(rot.x));
        ObjWorld = XMMatrixMultiply(ObjWorld, XMMatrixRotationY(rot.y));
        ObjWorld = XMMatrixMultiply(ObjWorld, XMMatrixRotationZ(rot.z));
        ObjWorld = XMMatrixMultiply(ObjWorld, XMMatrixTranslation(pos.x, pos.y, pos.z));
        view = ViewMatrix;
        model = XMMatrixTranspose(ObjWorld);
        CamPos = SCamPos;
        project = ProjectMatrix;
    }

    static void SetCamera(const XMFLOAT3& pos, const XMFLOAT3& objpos, float zoomrate, float winWHrate) {
        SCamPos = pos;
        XMVECTOR Eye = XMVectorSet(pos.x, pos.y, pos.z, 0.0f);
        XMVECTOR At = XMVectorSet(objpos.x, objpos.y, objpos.z, 0.0f);
        XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        ViewMatrix = XMMatrixTranspose(XMMatrixLookAtLH(Eye, At, Up));
        AtVector = At - Eye;
        farZ = 1000.0f;
        nearZ = 0.1f;
        
        //2d view
        //project = XMMATRIX(
        //    2.0f / (float)/*width * */zoomrate, 0, 0, 0,
        //    0, 2.0f / (float)/*height * */zoomrate, 0, 0,
        //    0, 0, 1.0f / (farZ - nearZ), 0,
        //    0, 0, 1.0f / (nearZ - farZ), 1);
        

        //3d view
        // Initialize the projection matrix
        if (winWHrate > 0.1f && !isnan(winWHrate)) {
            ProjectMatrix = XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV4, winWHrate, 0.01f, 100.0f));
        }
        /*ViewPos = pos;*/
    }

    static XMVECTOR GetPickingPos(int ScreenX, int ScreenY) {
        //m_dwWidth, m_dwHeight
        XMFLOAT3 xmf3PickPosition;
        xmf3PickPosition.x = (((2.0f * ScreenX) / (float)m_dwWidth) - 1) / ProjectMatrix.r[0].m128_f32[0];
        xmf3PickPosition.y = -(((2.0f * ScreenY) / (float)m_dwHeight) - 1) / ProjectMatrix.r[1].m128_f32[1];
        xmf3PickPosition.z = 1.0f;

        XMVECTOR xmvPickPosition = XMLoadFloat3(&xmf3PickPosition);
        XMMATRIX xmmtxView = ViewMatrix;

        XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, xmmtxView);

        XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
        XMVECTOR xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
        XMVECTOR xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
        xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
        return xmvPickRayDirection;
    }

    static void SetViewFrustum_Bounding() {
        XMVECTOR bound_line[4];
        bound_line[0] = GetPickingPos(0, 0);
        bound_line[1] = GetPickingPos(m_dwWidth, 0);
        bound_line[2] = GetPickingPos(m_dwWidth, m_dwHeight);
        bound_line[3] = GetPickingPos(0, m_dwHeight);
        XMVECTOR originpos = XMLoadFloat3(&SCamPos);
        XMVECTOR innerpos = originpos + AtVector;

        pexpr[0].SetPlane(originpos, bound_line[0], bound_line[1], innerpos);
        pexpr[1].SetPlane(originpos, bound_line[1], bound_line[2], innerpos);
        pexpr[2].SetPlane(originpos, bound_line[2], bound_line[3], innerpos);
        pexpr[3].SetPlane(originpos, bound_line[3], bound_line[0], innerpos);
        pexpr[4].SetPlane(AtVector, originpos + AtVector * nearZ);
        pexpr[5].SetPlane(-AtVector, originpos + AtVector * farZ);
    }

    static bool isPosVisibleInCamera(float x, float y, float z) {
        bool b = true;
        for (int i = 0; i < 6; ++i) {
            b &= pexpr[5].IsVisiblePlaneAtPos(x, y, z);
        }
        return b;
    }
};

struct TEXTURE_HANDLE
{
    ID3D12Resource* pTexResource;
    D3D12_CPU_DESCRIPTOR_HANDLE srv;
};

struct TEXTURES_VECTOR {
    TEXTURE_HANDLE* tex[8] = {};
};

bool heapcheck();

static inline int64_t GetTicks();

#endif