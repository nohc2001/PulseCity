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

class Game {
public:
	int m_stackMouseX = 0;
	int m_stackMouseY = 0;

	Shader* MyShader;
	OnlyColorShader* MyOnlyColorShader;
	DiffuseTextureShader* MyDiffuseTextureShader;
	ScreenCharactorShader* MyScreenCharactorShader;
	PBRShader1* MyPBRShader1;
	SkyBoxShader* MySkyBoxShader;

	UVMesh* TextMesh;

	std::vector<GameObject*> m_gameObjects; // GameObjets
	vector<ItemLoot> DropedItems;
	Player* player;

	vecset<BulletRay> bulletRays;

	//inputs
	bool isMouseReturn;
	vec4 DeltaMousePos;
	vec4 LastMousePos;

	bool bFirstPersonVision = true;

	float DeltaTime;
	UCHAR pKeyBuffer[256];

	int clientIndexInServer = -1;
	int playerGameObjectIndex = -1;

	GPUResource DefaultTex;
	GPUResource GunTexture;

	GPUResource DefaultNoramlTex;
	GPUResource DefaultAmbientTex;

	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;
	GPUResource LightCB_withShadowResource;
	LightCB_DATA_withShadow* LightCBData_withShadow;

	ShadowMappingShader* MyShadowMappingShader;
	SpotLight MySpotLight;

	void SetLight();

	vecset<matrix> NpcHPBars;
	bool isPrepared = false;
	bool isPreparedGo = false;
	float preparedFlow = 0;

	DataPackFactory pack_factory;
	bool isInventoryOpen = false;

	vector<GPUResource*> TextureTable;
	vector<Material> MaterialTable;
	GameMap* Map;
	static constexpr int basicTexMip = 10;
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	static constexpr char basicTexFormatStr[] = "BC3_UNORM";

	Game() {}
	~Game() {}
	void Init();
	void Render();
	void Render_last();
	void Render_ShadowPass();
	void Update();
	void Event(WinEvent evt);
	int Receiving(char* ptr, int totallen = 0);

	void AddMouseInput(int deltaX, int deltaY);

	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance);

	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);
};

extern Game game;