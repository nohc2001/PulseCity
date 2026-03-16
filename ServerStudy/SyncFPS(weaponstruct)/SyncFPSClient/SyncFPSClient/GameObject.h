#pragma once
#include "stdafx.h"
#include "main.h"
#include "Mesh.h"

class Mesh;
class Shader;
struct GPUResource;

/*
* 설명 : 클라이언트의 게임 오브젝트
* Sentinal Value : 
* NULL = (isExist == false)
*/
struct GameObject {
	// 게임오브젝트가 활성화 되었는지 여부
	bool isExist = true;
	// 해당 게임 오브젝트가 가진 머터리얼의 인덱스
	int MaterialIndex = 0;
	// 월드 행렬
	matrix m_worldMatrix;
	// 현재 속도
	vec4 LVelocity;
	// 다음 프레임에서 변경될 예정인 위치변화량
	vec4 tickLVelocity;

	union {
		// 가지고 있는 Mesh (Mesh/Model중 하나만 가져야 함.)
		Mesh* m_pMesh;
		// 가지고 있는 Model (Mesh/Model중 하나만 가져야 함.)
		Model* pModel = nullptr;
	};
	// 이 오브젝트를 그릴 셰이더 
	// <셰이더 통일을 할 것인가?>
	Shader* m_pShader = nullptr;
	// 서버로부터 받은 위치. 해당 위치로 클라이언트는 보간한다.
	vec4 Destpos;

	enum eRenderMeshMod {
		single_Mesh = 0,
		Model = 1
	};
	// 렌더링하는 것이 Mesh인지, Model인지 결정하는 enum.
	// improve : Shape으로 통일 못하나??
	eRenderMeshMod rmod = eRenderMeshMod::single_Mesh;

	GameObject();
	virtual ~GameObject();

	/*
	* 설명 : 한 프레임 마다 실행될 업데이트 함수
	* 매개변수 : 
	* float deltaTime : 이전 프레임에서 지금 프레임까지의 시간간격
	*/
	virtual void Update(float deltaTime);
	/*
	* 설명 : 
	* <메쉬일 경우>
	* WorldMatrix를 Root Param 1에 0~16에 Set하고,
	* Material의 텍스쳐는 Root Param 3에 DescTable로 Set하고,
	* Material의 Constant Buffer 값은 Root Param 4에 DescTable로 Set하고,
	* 메쉬를 사용해
	* <모델일 경우>
	* //fix <처리 안함>
	*/
	virtual void Render();

	/*
	* //improve : Shape로 통합되면 이 함수가 필요할까?
	* 설명 : Mesh를 설정한다.
	* 매개변수 : 
	* Mesh* pMesh : 설정할 mesh
	*/
	__forceinline void SetMesh(Mesh* pMesh);
	/*
	* //improve : Shader가 통일되면 이 함수가 필요하나?
	* 설명 : Shader를 설정한다.
	* 매개변수 : 
	* Shader* pShader : 설정할 셰이더
	*/
	__forceinline void SetShader(Shader* pShader);
	/*
	* 설명 : worldMatrix의 Z 기저를 look을 향하도록 한다.
	* 매개변수 : 
	* vec4 look : 바라볼 방향
	* vec4 up : 위쪽 방향
	*/
	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });
	/*
	* 설명 : 게임 오브젝트의 위치를 DestPos로 보간한다.
	*/
	__forceinline void PositionInterpolation(float deltaTime);
};

/*
* 설명 : 총알이 발사된 광선을 표현하는 구조체
*/
struct BulletRay {
	// 광선이 발사되고 사라지기까지의 시간
	static constexpr float remainTime = 0.2f;
	
	// 광선을 표현하는 메쉬
	static Mesh* mesh;

	// 광선의 시작점
	vec4 start;
	// 광선의 방향
	vec4 direction;
	// 광선 타이머
	float t;
	// 광선의 길이
	float distance;
	// 패딩
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);

	/*
	* 설명/반환 : BulletRay::mesh가 렌더링될 때 가지게 되는 worldMatrix를 반환한다.
	*/
	matrix GetRayMatrix();

	/*
	* 설명 : 광선의 타임을 업데이트
	*/ 
	void Update();

	/*
	* 광선을 렌더링 한다.
	*/
	void Render();
};

/*
* 설명 : 아이템 정보
* Sentinal Value : 
* NULL = 
*/
struct Item
{
	// 아이템 id
	int id;
	// 아이템 색상
	vec4 color;
	// 인벤토리에 나타날 메쉬
	Mesh* MeshInInventory;
	// Mesh를 그릴때 입히는 텍스쳐
	GPUResource* tex;
	// 아이템 설명
	const wchar_t* description;

	Item(int i, vec4 c, Mesh* m, GPUResource* t, const wchar_t* d) :
		id{ i }, color{ c }, MeshInInventory{ m }, tex{ t }, description{
		d
		}
	{

	}
};

typedef int ItemID;

/*
* 설명 : 인벤토리에 들어갈 아이템 스택 정보
* Sentinal Value :
* NULL : (ItemCount == 0)
*/
struct ItemStack {
	//아이템 id
	ItemID id;
	//아이템의 개수
	int ItemCount;
};

/*
* 설명 : 드롭된 아이템을 나타내는 구조체
* Sentinal Value : 
* NULL = (itemDrop.ItemCount == 0)
*/
struct ItemLoot {
	// 아이템 스택 정보
	ItemStack itemDrop;
	// 아이템 드롭 위치
	// improve : <아이템이 바닥으로 중력이 작용되었으면 좋겠음.>
	vec4 pos;
};

// 아이템의 원본 정보가 담긴 아이템들의 테이블
extern vector<Item> ItemTable;

/*
* 설명 : 계층구조를 가지는 오브젝트
* Sentinal Value : 
* NULL = (isExist == false)
*/
class Hierarchy_Object : public GameObject {
public:
	//자식 오브젝트의 개수
	int childCount = 0;
	//자식 오브젝트들의 배열
	vector<Hierarchy_Object*> childs;

	Hierarchy_Object() {
		childCount = 0;
		MaterialIndex = 0;
	}
	~Hierarchy_Object() {}

	/*
	* 설명 : parent_world 로 변환을 수행한후, 계층구조 오브젝트의 자신과 자식을 모두 렌더한다.
	* 계층 구조오브젝트들을 모두 렌더링 할 때 쓰인다.
	* *현재 선택된 Game::renderViewPort에서 <원근투영 프러스텀 컬링>을 사용하여 
	* 그리지 않는 물체를 제외시킨다.
	* 매개변수 : 
	* matrix parent_world : 계승될 행렬
	* Shader::RegisterEnum sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit(matrix parent_world, Shader::RegisterEnum sre = Shader::RegisterEnum::RenderWithShadow);

	/*
	* 설명 : parent_world 로 변환을 수행한후, 계층구조 오브젝트의 자신과 자식을 모두 렌더한다.
	* 계층 구조오브젝트들을 모두 렌더링 할 때 쓰인다. 
	* 현재 선택된 Game::renderViewPort에서 <직교투영 프러스텀 컬링>을 사용하여 
	* 그리지 않는 물체를 제외시킨다.
	* 매개변수 : 
	* matrix parent_world : 계승될 행렬
	* Shader::RegisterEnum sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit_CullingOrtho(matrix parent_world, Shader::RegisterEnum sre = Shader::RegisterEnum::RenderWithShadow);

	/*
	* 설명 :
	* Hierarchy_Object의 Shape(Model/Mesh)의 OBB에 worldMat 변환을 적용해 반환한다.
	* 매개변수 :
	* matrix worldMat : OBB를 변환할 월드변환행렬
	* 반환 :
	* if ShapeIndex == -1 (모양이 없을 경우) >>> Extents.x == -1 인 OBB (OBB계의 NULL임)
	* if (모양이 메쉬인 경우) >>> MeshOBB를 worldMat로 변환한 OBB
	* if (모양이 모델인 경우) >>> ModelOBB를 worldMat로 변환한 OBB
	*/
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

/*
* 설명 : 게임 맵 데이터
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	//맵의 다양한 객체 (오브젝트, 메쉬, 애니메이션, 텍스쳐, 머터리얼)등을 읽을 때 사용되는 중복가능한 이름을 저장함.
	vector<string> name;
	//맵의 메쉬데이터 배열
	vector<Mesh*> meshes;
	//모델의 배열
	vector<Model*> models;
	//맵에 놓여져 있는 충돌처리를 할 계층구조 오브젝트
	vector<Hierarchy_Object*> MapObjects;
	// 맵 전체 영역의 AABB
	vec4 AABB[2] = { 0, 0 };

	/*
	* 설명 : OBB를 받고, 그것을 통해 맵 전체의 AABB를 확장한다.
	* 매개변수 :
	* BoundingOrientedBox obb : 받은 OBB
	*/
	void ExtendMapAABB(BoundingOrientedBox obb);

	//Map을 로드할때, 첫번째로 로드되는 텍스쳐가 몇번째 텍스쳐인지
	unsigned int TextureTableStart = 0;
	//Map을 로드할때, 첫번째로 로드되는 Material이 몇번째 머터리얼인지
	unsigned int MaterialTableStart = 0;

	/*
	* 설명 : 전체 맵을 로드한다.
	* 매개변수 :
	* const char* MapName : 맵 파일의 이름. 경로가 아니니 확장자 또한 붙이지 않고, 오직 파일의 이름만 적는다.
	*	해당 이름을 가진 .map 파일이 Resource/Map 경로에 있어야 한다.
	*/
	void LoadMap(const char* MapName);
};

struct SphereLODObject : public GameObject {
	Mesh* MeshNear;
	Mesh* MeshFar;
	float SwitchDistance;

	vec4 FixedPos;

	virtual void Update(float deltaTime) override; 
	virtual void Render() override;
};

/*
* 설명 : 무기 타입 enum
*/
enum class WeaponType { MachineGun, Sniper, Shotgun, Rifle, Pistol, Max };

/*
* 설명 : 무기 타입 구조체
*/
struct WeaponData {
	WeaponType type;
	float shootDelay;     // 연사 속도
	float recoilVelocity; // 반동 세기
	float recoilDelay;    // 반동 회복 시간
	float damage;         // 기본 데미지
	int maxBullets;       // 탄창 용량
	float reloadTime;     // 장전 시간

	vec4 muzzleOffset; // 총구 위치 오프셋
};

static WeaponData GWeaponTable[] = {
	{ WeaponType::MachineGun, 0.1f, 12.0f, 0.2f, 10.0f, 100, 4.0f, {0.4f, -0.50f, 0.4f, 0.0f} },
	{ WeaponType::Sniper, 1.5f, 10.0f, 1.0f, 100.0f, 5, 2.0f, {0.80f, -0.40f, 3.0f, 0.0f} },
	{ WeaponType::Shotgun, 0.7f, 7.0f, 0.6f, 12.0f, 8, 3.0f, {0.75f, -0.4f, 1.50f, 0.0f} },
	{ WeaponType::Rifle, 0.12f, 10.0f, 0.3f, 15.0f, 30, 2.5f, {0.90f, -0.5f, 1.50f, 0.0f} },
	{ WeaponType::Pistol, 0.4f, 5.0f, 0.2f, 15.0f, 12, 1.5f, {1.0f, -0.50f, 1.10f, 0.0f} },
	// 
};


class Weapon {
public:
	WeaponData m_info;      // GWeaponTable에서 가져온 수치
	float m_shootFlow = 0;  // 다음 발사까지 남은 시간 계산
	float m_recoilFlow = 0; // 반동 애니메이션/에임 상승 진행률

	Weapon(WeaponType type) : m_info(GWeaponTable[(int)type]) {
		m_shootFlow = m_info.shootDelay;
		m_recoilFlow = m_info.recoilDelay;
	}

	virtual void Update(float deltaTime) {
		if (m_shootFlow < m_info.shootDelay) m_shootFlow += deltaTime;
		if (m_recoilFlow < m_info.recoilDelay) m_recoilFlow += deltaTime;
	}

	virtual void OnFire() {
		m_shootFlow = 0.0f;
		m_recoilFlow = 0.0f;
	}

	/*
	* 설명 
	* 현재 반동이 얼마나 진행되었는지 0~1 사이 값으로 반환
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};