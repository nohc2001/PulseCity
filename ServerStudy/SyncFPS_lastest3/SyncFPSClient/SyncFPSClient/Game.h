#pragma once
#include "stdafx.h"
#include "main.h"
#include "NetworkDefs.h"
#include "GraphicDefs.h"
#include "Shader.h"
#include "GameObject.h"

extern Socket* ClientSocket;

class UVMesh;
class Player;

/*
* 설명 : 클라이언트의 게임을 나타내는 자료구조.
*/
class Game {
public:
	//마우스 움직임의 X 부분 변화량을 쌓는다.
	int m_stackMouseX = 0;
	//마우스 움직임의 Y 부분 변화량을 쌓는다.
	int m_stackMouseY = 0;

	//텍스쳐를 사용하지 않는 색과 빛처리만 하는 셰이더
	Shader* MyShader;
	//3D 공간에서 빛처리를 하지 않고, 색만 보여주는 셰이더
	OnlyColorShader* MyOnlyColorShader;
	//Diffuse 텍스쳐와 빛 처리만 하는 셰이더
	DiffuseTextureShader* MyDiffuseTextureShader;
	//RECT를 받아 텍스쳐를 화면상에 영역에 그리는 셰이더. 글자를 출력할때 많이 쓰인다.
	ScreenCharactorShader* MyScreenCharactorShader;
	//PBR 렌더링을 하는 첫번째 셰이더.
	PBRShader1* MyPBRShader1;
	//스카이박스를 그리는 셰이더.
	SkyBoxShader* MySkyBoxShader;

	//MyScreenCharactorShader를 사용해 TextRendering에 사용되는 사각형 Plane Mesh.
	UVMesh* TextMesh;
	
	//HPBar를 나타내는데 사용되는 Mesh
	Mesh* HPBarMesh;
	//열기 를 나타내는데 사용되는 Mesh
	//sus <내가 알기로는 열기든 HP든 Mesh가 같은데 왜 두개가 있는지 모르겠음.>
	Mesh* HeatBarMesh;
	//조준점을 나타내는 정육면체 Mesh.
	Mesh* ShootPointMesh;
	//현재는 미니 건을 로드한다. 플레이어가 들고 있는 총의 Model이다.
	Model* GunModel;

	// GameObject 배열
	std::vector<GameObject*> m_gameObjects;

	// 드롭된 아이템의 배열
	vector<ItemLoot> DropedItems;

	// 클라이언트가 조작하는 플레이어
	Player* player;

	// 불릿Ray를 모아놓은 vecset -> 어짜피 나타나고 사라지는 시간은 같으니, 환형배열이 더 낮다.
	// <환형배열로 고칠 필요가 있다.>
	vecset<BulletRay> bulletRays;

	// 화면을 회전할때, x 부분은 좌우회전을 당담하고, y부분은 상하 회전당담.
	// y는 -200 ~ 200 까지의 값만 가질 수 있다.
	vec4 DeltaMousePos;

	// 현재 1인칭 시점인지
	bool bFirstPersonVision = true;

	// 업데이트에 쓰이는 DeltaTime
	float DeltaTime;

	// 어떤 Key가 눌려있는지 표현하는 배열
	UCHAR pKeyBuffer[256];

	// 서버에서 클라이언트의 인덱스
	int clientIndexInServer = -1;

	// 게임오브젝트 배열에서 클라이언트가 조작하는 플레이어의 인덱스
	int playerGameObjectIndex = -1;

	//이전에 사용되었던 권총 메쉬. 
	//<지금은 사용을 안하지만, 나중에 사용이 가능해질거라서 총을 어떻게 할지 정하고 뭐 처리를 하던 해야함.>
	Mesh* GunMesh;
	// 이전에 사용되었던 권총메쉬의 텍스쳐
	GPUResource GunTexture;
	
	// 텍스쳐가 없을때 대체하기 위한 기본 디퓨즈 텍스쳐
	GPUResource DefaultTex;
	// 텍스쳐가 없을때 대체하기 위한 기본 노멀 텍스쳐
	GPUResource DefaultNoramlTex;
	// 텍스쳐가 없을때 대체하기 위한 기본 앰비언트 텍스쳐
	GPUResource DefaultAmbientTex;

	// 아마 곧 쓰이지 않게될 라이트 데이터 CB 리소스 (쉐도우가 포함된 버전이 존재함.)
	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	// 라이트 정보를 담고 있는 쉐도우 맵핑을 위한 CB 리소스
	GPUResource LightCB_withShadowResource;
	// Upload Buffer에 Mapping 된 cpu 메모리
	LightCB_DATA_withShadow* LightCBData_withShadow;

	// <수정 필요>
	// 쉐도우 맵을 만들어내는 객체. 셰이더라고 되어 있지만 실제 셰이더 역할을 하지는 않는다.
	// 나중에 기능을 따로 빼내어 정비해야 함.
	// fix
	ShadowMappingShader* MyShadowMappingShader;

	// SpotLight 라고 되어있지만, 사실 씬 전체를 덮는 DirectionLight 정보가 있다.
	// improve : 이름 바꾸세요.
	SpotLight MySpotLight;

	// 라이트 정보를 초기화 한다.
	void SetLight();

	// NPC 채력바들의 월드 행렬의 배열.
	vecset<matrix> NpcHPBars;

	// sus <이 세개는 왜 이렇게 되어 있는지 원인 규명 필요>
	// 클라이언트 실행이 준비되었다는 것을 알리는 신호
	bool isPrepared = false;
	// 클라이언트 실행이 완료되었음을 알리는 신호를 켜야 한다는 신호
	bool isPreparedGo = false;
	// 준비 타이머
	float preparedFlow = 0;

	// PACK 프로토콜의 패킷을 받기 위한 팩토리
	DataPackFactory pack_factory;

	// 플레이어 인벤토리 창이 열렸는지 여부
	bool isInventoryOpen = false;

	// 현재 씬에서 쓰일 모든 텍스쳐들이 담겨있는 배열
	vector<GPUResource*> TextureTable;
	// 현재 씬에서 쓰일 모든 머터리얼들이 담겨져 있는 배열
	vector<Material> MaterialTable;
	// 게임의 맵 정보
	GameMap* Map;

	// 기본 텍스쳐 밉맵 레벨
	static constexpr int basicTexMip = 10;
	// 기본 텍스쳐 블럭압축 포맷
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	// 기본 텍스쳐 블럭압축 포맷 문자열 - dds 만들때 쓰인다.
	static constexpr char basicTexFormatStr[] = "BC3_UNORM";

	// particle
	static constexpr UINT FIRE_COUNT = 200;
	static constexpr UINT FIRE_PILLAR_COUNT = 400;
	static constexpr UINT FIRE_RING_COUNT = 300;

	ParticlePool FirePool;
	ParticlePool FirePillarPool;
	ParticlePool FireRingPool;

	ParticleCompute* FireCS = nullptr;
	ParticleCompute* FirePillarCS = nullptr;
	ParticleCompute* FireRingCS = nullptr;

	ParticleCompute* ParticleCS = nullptr;
	ParticleShader* ParticleDraw = nullptr;

	void InitParticlePool(ParticlePool& pool, UINT count);

	// 현재 렌더링을 수행하는 viewport
	static ViewportData* renderViewPort;

	Game() {}
	~Game() {}
	/*
	* 설명 : 게임을 초기화 하는 함수.
	* 1.GameObjectType::STATICINIT(); 을 통해 각 GameObject 의 vptr을 얻어낸다.
	* 2.DropedItems, NpcHPBars, bulletRays 를 초기화 한다.
	* 3.커맨드리스트를 리셋하여 리소스들을 만들기 시작한다.
	* 3-1. 기본 텍스쳐들을 만든다.  Default~~~Tex
	* 3-2. 서버의 정보를 받아 업데이트 할 수 있도록 GlobalTextureArr 에 Tile, Wall, Monster 텍스쳐를 만든다.
	* 3-3. 맵을 로드한다
	* 3-4. SetLight함수로 라이트를 초기화 한다.
	* 3-5. 뷰포트의 투영행렬을 초기화 한다.
	* 3-6. Item 들의 Mesh를 로드하고 아이템 테이블에 아이템들을 담는다.
	* 3-7. 각종 셰이더를 초기화한다.
	* 3-8. 추가적으로 필요한 Shape들을 로드하고 저장한다.
	* 3-9. 커맨드리스트를 닫고 GPU에 실행시킨다.
	*/
	void Init();
	/*
	* 설명 : 게임을 렌더링 한다.
	*/
	void Render();
	/*
	* 설명 : 쉐도우 맵을 렌더링한다.
	*/
	void Render_ShadowPass();
	/*
	* 설명 : 게임을 업데이트 한다.
	*/
	void Update();
	/*
	* 설명 : 서버에서 받은 데이터를 해석해 클라이언트의 동기화를 한다.
	* 매개변수 : 
	* char* ptr : 받은 데이터의 시작주소
	* int totallen : 받은 데이터의 바이트 개수
	* 반환 : 
	* 현재 읽기를 완료한 바이트 수를 반환.
	*/
	int Receiving(char* ptr, int totallen = 0);
	/*
	* 설명 : 마우스 움직임이 일어났을때, DeltaMousePos에 적용시키는 함수.
	* 매개변수 : 
	* int deltaX : X 쪽으로 움직인 정도
	* int deltaY : Y 쪽으로 움직인 정도
	*/
	void AddMouseInput(int deltaX, int deltaY);
	/*
	* 설명 : 텍스트를 렌더링한다.
	* 매개변수 : 
	* const wchar_t* wstr : 출력할 문자열
	* int length : 문자열 길이
	* vec4 Rect : 문자열이 그려질 영역
	* float fontsiz : 텍스트 폰트 사이즈
	* float depth : 어떤 깊이값에서 텍스트가 렌더링 되는지 결정.
	*/
	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);
};

// 게임
extern Game game;