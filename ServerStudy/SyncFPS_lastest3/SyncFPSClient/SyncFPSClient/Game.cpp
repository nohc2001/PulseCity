#include "stdafx.h"
#include "main.h"
#include "Game.h"
#include "Render.h"
#include "Shader.h"
#include "Mesh.h"
#include "GameObject.h"
#include "NetworkDefs.h"
#include "Player.h"
#include "Monster.h"

template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

union GameObjectType {
	static constexpr int ObjectTypeCount = 3;

	short id;
	enum {
		_GameObject = 0,
		_Player = 1,
		_Monster = 2,
	};

	operator short() { return id; }

	static constexpr int ClientSizeof[ObjectTypeCount] = {
		sizeof(GameObject),
		sizeof(Player),
		sizeof(Monster)
	};

	static constexpr int ServerSizeof[ObjectTypeCount] = {
#ifdef _DEBUG
		128,
		352,
		272,
#else
		128,
		352,
		272,
#endif
	};

	static void* vptr[ObjectTypeCount];
	static void STATICINIT() {
		vptr[GameObjectType::_GameObject] = GetVptr<GameObject>();
		vptr[GameObjectType::_Player] = GetVptr<Player>();
		vptr[GameObjectType::_Monster] = GetVptr<Monster>();
	}
};

void* GameObjectType::vptr[GameObjectType::ObjectTypeCount];

void Game::SetLight()
{
	LightCBData = new LightCB_DATA();
	UINT ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCBResource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCBResource.resource->Map(0, NULL, (void**)&LightCBData);
	LightCBData->dirlight.gLightColor = { 0.5f, 0.5f, 0 };
	LightCBData->dirlight.gLightDirection = { 1, -1, 1 };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData->pointLights[i];
		p.LightColor = { 1, 1, 1 };
		p.LightIntencity = 100;
		p.LightRange = 120;
		p.LightPos = { (float)(rand() % 80 - 40), 1, (float)(rand() % 80 - 40) };
	}
	LightCBData->pointLights[0].LightPos = { 0, 0, 0 };
	//LightCBData->pointLights[1].LightPos = { -5, 1, 5 };
	//LightCBData->pointLights[2].LightPos = { 5, 1, 5 };
	//LightCBData->pointLights[3].LightPos = { 5, 1, -5 };
	LightCBResource.resource->Unmap(0, nullptr);

	//LightCBData_withShadow = new LightCB_DATA_withShadow();
	ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCB_withShadowResource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->dirlight.gLightColor = { 0.5f, 0.5f, 0.5f };
	vec4 dir = vec4(1, -2, 1, 0);
	dir.len3 = 1;
	LightCBData_withShadow->dirlight.gLightDirection = { dir.x, dir.y, dir.z };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData_withShadow->pointLights[i];
		p.LightPos = { (float)(rand() % 40 - 20), 1, (float)(rand() % 40 - 20) };
		p.LightIntencity = 20;
		p.LightColor = { 1, 1, 1 };
		p.LightRange = 50;
	}
	LightCBData_withShadow->LightProjection = gd.viewportArr[0].ProjectMatrix;
	LightCBData_withShadow->LightView = MySpotLight.View;
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCBData_withShadow->pointLights[0].LightPos = { 10, 0, 0 };
	LightCB_withShadowResource.resource->Unmap(0, nullptr);
}

//non using
void Game::Init()
{
	GameObjectType::STATICINIT();

	DropedItems.reserve(4096);
	NpcHPBars.Init(32);

	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);
	DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
	GunTexture.CreateTexture_fromFile(L"Resources/m1911pistol_diffuse0.png", game.basicTexFormat, game.basicTexMip);
	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_TileTex].CreateTexture_fromFile(L"Resources/Tiles036_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);
	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_WallTex].CreateTexture_fromFile(L"Resources/Chip001_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);
	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_Monster].CreateTexture_fromFile(L"Resources/PaintedMetal004_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);

	DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
	DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);

	MiniGunModel = new Model();
	MiniGunModel->LoadModelFile("Resources/Model/minigun_m134.model");

	Map = new GameMap();
	Map->LoadMap("The_Port");

	SetLight();

	bulletRays.Init(32);

	gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);

	//gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	//

	Mesh* ItemMesh = new UVMesh();
	ItemMesh->ReadMeshFromFile_OBJ("Resources/Mesh/BulletMag001.obj", vec4(1, 1, 1, 1), true);
	ItemTable.push_back(Item(0, vec4(0, 0, 0, 0), nullptr, nullptr, L"")); // blank space in inventory.
	ItemTable.push_back(Item(1, vec4(1, 0, 0, 1), ItemMesh, &gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_Monster], L"[빨간 탄알집]"));
	ItemTable.push_back(Item(2, vec4(0, 1, 0, 1), ItemMesh, &gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_WallTex], L"[녹색 탄알집]"));
	ItemTable.push_back(Item(3, vec4(0, 0, 1, 1), ItemMesh, &DefaultTex, L"[하얀 탄알집]")); // test items. red, green, blue bullet mags.

	MyShader = new Shader();
	MyShader->InitShader();

	MyOnlyColorShader = new OnlyColorShader();
	MyOnlyColorShader->InitShader();

	MyDiffuseTextureShader = new DiffuseTextureShader();
	MyDiffuseTextureShader->InitShader();

	MyScreenCharactorShader = new ScreenCharactorShader();
	MyScreenCharactorShader->InitShader();

	MyShadowMappingShader = new ShadowMappingShader();
	MyShadowMappingShader->InitShader();

	MyPBRShader1 = new PBRShader1();
	MyPBRShader1->InitShader();

	MySkyBoxShader = new SkyBoxShader();
	MySkyBoxShader->InitShader();
	MySkyBoxShader->LoadSkyBox(L"Resources/GlobalTexture/SkyBox_0.dds");

	TextMesh = new UVMesh();
	TextMesh->CreateTextRectMesh();

	bulletRays.Init(32);

	Mesh* MyMesh = (Mesh*)new UVMesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 1, 1, 1 });
	Shape::AddMesh("Player", MyMesh);

	Mesh* MyGroundMesh = (Mesh*)new UVMesh();
	MyGroundMesh->CreateWallMesh(40.0f, 0.5f, 40.0f, { 1, 1, 1, 1 });
	Shape::AddMesh("Ground001", MyGroundMesh);

	Mesh* MyWallMesh = (Mesh*)new UVMesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f, { 1, 1, 1, 1 });
	Shape::AddMesh("Wall001", MyWallMesh);

	BulletRay::mesh = (Mesh*)new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);

	Mesh* MyMonsterMesh = (Mesh*)new UVMesh();
	MyMonsterMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 0, 0, 1 });
	Shape::AddMesh("Monster001", MyMonsterMesh);

	game.GunMesh = (Mesh*)new UVMesh();
	game.GunMesh->ReadMeshFromFile_OBJ("Resources/Mesh/minigun.obj", { 1, 1, 1, 1 });
	//Shape::AddMesh("Gun001", GunMesh);

	game.HPBarMesh = new Mesh();
	game.HPBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 0, 1, 0, 1 }, false);
	//Shape::AddMesh("HPBar", HPBarMesh);

	game.HeatBarMesh = new Mesh();
	game.HeatBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 0, 0, 1 }, false);
	//Mesh::meshmap.insert(pair<string, Mesh*>("HeatBar", HeatBarMesh));

	game.ShootPointMesh = new Mesh();
	game.ShootPointMesh->CreateWallMesh(0.05f, 0.05f, 0.05f, { 1, 1, 1, 0.5f });
	//Mesh::meshmap.insert(pair<string, Mesh*>("ShootPoint", ShootPointMesh));

	MySpotLight.ShadowMap = MyShadowMappingShader->CreateShadowMap(4096/*gd.ClientFrameWidth*/, 4096/*gd.ClientFrameHeight*/, 0);
	MySpotLight.View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

	/*
	Player* MyPlayer = new Player(&pKeyBuffer[0]);
	MyPlayer->SetMesh(Mesh::meshmap["Player"]);
	MyPlayer->SetShader(MyShader);
	MyPlayer->SetWorldMatrix(XMMatrixTranslation(5.0f, 5.0f, 3.0f));
	m_gameObjects.push_back(MyPlayer);
	player = MyPlayer;
	player->Gun = new Mesh();
	player->Gun->ReadMeshFromFile_OBJ("Resources/Mesh/m1911pistol.obj", { 1, 1, 1, 1 });

	player->gunMatrix_thirdPersonView.Id();
	player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);

	player->gunMatrix_firstPersonView.Id();
	player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
	player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - player->gunMatrix_firstPersonView.pos);

	player->ShootPointMesh = new Mesh();
	player->ShootPointMesh->CreateWallMesh(0.05f, 0.05f, 0.05f, { 1, 1, 1, 0.5f });

	player->HPBarMesh = new Mesh();
	player->HPBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 0, 0, 1 }, false);
	player->HPMatrix.pos = vec4(-1, 1, 1, 1);
	player->HPMatrix.LookAt(vec4(-1, 0, 0));

	Monster* myMonster_1 = new Monster();
	myMonster_1->SetMesh(MyMonsterMesh);
	myMonster_1->SetShader(MyShader);
	myMonster_1->Init(XMMatrixTranslation(0.0f, 5.0f, 5.0f));
	m_gameObjects.push_back(myMonster_1);

	Monster* myMonster_2 = new Monster();
	myMonster_2->SetMesh(MyMonsterMesh);
	myMonster_2->SetShader(MyShader);
	myMonster_2->Init(XMMatrixTranslation(-5.0f, 0.5f, -2.5f));
	m_gameObjects.push_back(myMonster_2);

	Monster* myMonster_3 = new Monster();
	myMonster_3->SetMesh(MyMonsterMesh);
	myMonster_3->SetShader(MyShader);
	myMonster_3->Init(XMMatrixTranslation(5.0f, 0.5f, -2.5f));
	m_gameObjects.push_back(myMonster_3);

	Mesh* ColMesh = new Mesh();
	ColMesh->CreateWallMesh(0.1f, 0.15f, 0.2f, { 0, 1, 0, 1 });

	for (int i = 0; i < 100; ++i) {
		LMoveObject_CollisionTest* Col1 = new LMoveObject_CollisionTest();
		Col1->SetMesh(ColMesh);
		Col1->SetShader(MyShader);
		Col1->m_worldMatrix.pos = vec4(-5 + (rand() % 100) / 10.0f, 1 + (rand() % 100) / 10.0f, -5 + (rand() % 100) / 10.0f, 1);

		vec4 look = vec4(-5 + (rand() % 100) / 10.0f, 1 + (rand() % 100) / 10.0f, -5 + (rand() % 100) / 10.0f, 1);
		look.len3 = 1;
		Col1->m_worldMatrix.LookAt(look);

		Col1->LVelocity = -Col1->m_worldMatrix.pos;
		Col1->LVelocity.len3 = 1;
		m_gameObjects.push_back(Col1);
	}

	GameObject* groundObject = new GameObject();
	groundObject->SetMesh(MyGroundMesh);
	groundObject->SetShader(MyShader);
	groundObject->SetWorldMatrix(XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	m_gameObjects.push_back(groundObject);

	GameObject* wallObject = new GameObject();
	wallObject->SetMesh(MyWallMesh);
	wallObject->SetShader(MyShader);
	wallObject->SetWorldMatrix(XMMatrixTranslation(0.0f, 1.0f, 5.0f));
	m_gameObjects.push_back(wallObject);

	GameObject* wallObject2 = new GameObject();
	wallObject2->SetMesh(MyWallMesh);
	wallObject2->SetShader(MyShader);
	wallObject2->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation(10.0f, 1.0f, 0.0f)));
	m_gameObjects.push_back(wallObject2);

	GameObject* wallObject3 = new GameObject();
	wallObject3->SetMesh(MyWallMesh);
	wallObject3->SetShader(MyShader);
	wallObject3->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation(5.0f, 0.0f, -5.0f)));
	m_gameObjects.push_back(wallObject3);

	GameObject* wallObject4 = new GameObject();
	wallObject4->SetMesh(MyWallMesh);
	wallObject4->SetShader(MyShader);
	wallObject4->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 4), XMMatrixTranslation(5.0f, -1.0f, -10.0f)));
	m_gameObjects.push_back(wallObject4);
	*/

	gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();
}

void Game::Render() {

	gd.viewportArr[0].UpdateFrustum();

	if (isPrepared == false) return;

	//load text texture
	if (gd.addTextureStack.size() > 0) {
		for (int i = 0; i < gd.addTextureStack.size(); ++i) {
			gd.AddTextTexture(gd.addTextureStack[i]);
		}
	}
	gd.addTextureStack.clear();

	gd.ShaderVisibleDescPool.Reset();

	Render_ShadowPass();

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.pCommandList->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	float pfClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	gd.pCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	gd.pCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE,
		&d3dDsvCPUDescriptorHandle);
	//render begin ----------------------------------------------------------------

	MySkyBoxShader->RenderSkyBox();
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//////////////////////
	((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);
	MyDiffuseTextureShader->SetTextureCommand(&game.DefaultTex);

	/*XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(gd.viewportArr[0].ViewMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);*/

	matrix viewMat = gd.viewportArr[0].ViewMatrix;
	viewMat *= gd.viewportArr[0].ProjectMatrix;
	viewMat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);

	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);

	gd.pCommandList->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());

	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->isExist == false) continue;
		m_gameObjects[i]->Render();
	}

	// already droped items. (non move..)
	static float itemRotate = 0;
	itemRotate += DeltaTime;
	matrix mat;
	mat.trQ(vec4::getQ(vec4(0, itemRotate, 0, 0)));
	for (int i = 0; i < DropedItems.size(); ++i) {
		if (DropedItems[i].itemDrop.id != 0) {
			mat.pos = DropedItems[i].pos;
			mat.pos.w = 1;
			matrix rmat = XMMatrixTranspose(mat);
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &rmat, 0);
			MyDiffuseTextureShader->SetTextureCommand(ItemTable[DropedItems[i].itemDrop.id].tex);
			ItemTable[DropedItems[i].itemDrop.id].MeshInInventory->Render(gd.pCommandList, 1);
		}
	}

	//////////////////
	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::RegisterEnum::RenderWithShadow);

	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	game.MyPBRShader1->SetShadowMapCommand(game.MySpotLight.renderDesc);

	Hierarchy_Object* obj = Map->MapObjects[0];
	matrix mat2 = XMMatrixIdentity();
	//mat2.pos.y -= 1.75f;
	Hierarchy_Object::renderViewPort = &gd.viewportArr[0];
	obj->Render_Inherit(mat2, Shader::RegisterEnum::RenderWithShadow);
	//////////////////

	((Shader*)MyOnlyColorShader)->Add_RegisterShaderCommand(gd.pCommandList);

	viewMat = gd.viewportArr[0].ViewMatrix;
	viewMat *= gd.viewportArr[0].ProjectMatrix;
	viewMat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);

	//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Render();
	}

	for (int i = 0; i < game.NpcHPBars.size; ++i)
	{
		if (game.NpcHPBars.isAlloc(i))
		{
			matrix& hpBarWorldMat = game.NpcHPBars[i];
			hpBarWorldMat.transpose();
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &hpBarWorldMat, 0);
			game.HPBarMesh->Render(gd.pCommandList, 1);
		}
	}

	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	//((Shader*)MyShader)->Add_RegisterShaderCommand(gd.pCommandList);

	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float HealSkillCooldownFlow = 0;


	// UI/AfterDepthClear Render
	if (game.player != nullptr) {
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		HealSkillCooldownFlow = game.player->HealSkillCooldownFlow;

	}

	// HP 
	//maybe..1706x960
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);
	((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);
	std::wstring ui_hp = L"HP: " + std::to_wstring(hhpp);
	RenderText(ui_hp.c_str(), ui_hp.length(), rt, 30);

	//Heat Gauge
	vec4 rt_heat = rt;
	rt_heat.y -= 80 * Rate;   // HP보다 약간 아래로 내림 (간격 50 정도)
	std::wstring ui_heat = L"Heat: " + std::to_wstring((int)HeatGauge);
	RenderText(ui_heat.c_str(), ui_heat.length(), rt_heat, 30);

	//Skill
		rt = Rate * vec4(-900, 850, -200, 700);
	std::wstring ui_cool = L"[Q] Heal CD: " + std::to_wstring((int)HealSkillCooldownFlow);
	RenderText(ui_cool.c_str(), ui_cool.length(), rt, 30);


	// Bullet
	rt = Rate * vec4(900, -800, 1550, -900);
	std::wstring ui_bullet = L"Bullet: " + std::to_wstring(bulletCount);
	RenderText(ui_bullet.c_str(), ui_bullet.length(), rt, 30);

	// Kill/Death Counter
	rt = Rate * vec4(1100, 920, 1550, 700);
	std::wstring ui_kd = std::to_wstring(kill) + L" / " + std::to_wstring(death);
	RenderText(L"Kill/Death", 10, rt, 30);
	rt = Rate * vec4(1190, 850, 1550, 600);
	RenderText(ui_kd.c_str(), ui_kd.length(), rt, 30);

	// Player name
	rt = Rate * vec4(-1650, 700, -1000, 500);
	std::wstring playerName = L"Player: Leo";
	RenderText(playerName.c_str(), playerName.length(), rt, 30);

	// ----------Inventory------------
	if (isInventoryOpen) {
		matrix orthoMatrix = XMMatrixOrthographicOffCenterLH(0.0f, (float)gd.ClientFrameWidth, (float)gd.ClientFrameHeight, 0.0f, 0.01f, 1.0f);
		matrix uiViewMat = orthoMatrix;
		uiViewMat.transpose();
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);
		MyScreenCharactorShader->SetTextureCommand(&DefaultTex);
		/*gd.pCommandList->SetPipelineState(MyOnlyColorShader->pUiPipelineState);*/
		//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &uiViewMat, 0);

		const int gridColumns = 5;
		const int gridRows = 5;
		const float slotSize = Rate * 150.0f;
		const float itemPadding = Rate * 20.0f;
		const float slotPadding = Rate * 10.0f;

		const float actualItemSize = slotSize - (itemPadding * 2);

		float invWidth = (gridColumns * slotSize) + ((gridColumns + 1) * slotPadding);
		float invHeight = (gridRows * slotSize) + ((gridRows + 1) * slotPadding);
		float invPosX = 0;
		float invPosY = 0;

		vec4 bgColor = { 0.2f, 0.2f, 0.2f, 0.8f };
		float CB[11] = { invPosX - invWidth ,invPosY - invHeight , invPosX + invWidth ,invPosY + invHeight, bgColor.r, bgColor.g, bgColor.b, bgColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.5f };
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 11, CB, 0);
		TextMesh->Render(gd.pCommandList, 1);

		vec4 slotColor = { 0.15f, 0.15f, 0.15f, 0.9f };
		//Mesh* slotMesh = GetOrCreateColoredQuadMesh(slotColor);

		float startSlotX = invPosX - invWidth + slotPadding + slotSize;
		float startSlotY = invPosY - invHeight + slotPadding + slotSize;

		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = startSlotY + row * (2 * (slotSize + slotPadding));

				float CB2[11] = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize, slotColor.r, slotColor.g, slotColor.b, slotColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.05f };
				gd.pCommandList->SetGraphicsRoot32BitConstants(0, 11, CB2, 0);
				TextMesh->Render(gd.pCommandList, 1);
			}
		}

		float startItemX = invPosX - Rate * 140.0f;
		float startItemY = invPosY - Rate * 130.0f;

		matrix viewMat2 = DirectX::XMMatrixLookAtLH(vec4(0, 0, 0), vec4(0, 0, 1), vec4(0, 1, 0));
		viewMat2 *= gd.viewportArr[0].ProjectMatrix;
		gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());

		for (int i = 0; i < player->Inventory.size(); ++i) {
			if (i >= gridColumns * gridRows)
				break;

			ItemStack& currentStack = player->Inventory[i];
			ItemID currentItemID = currentStack.id;
			if (currentItemID == 0) continue;

			if (currentItemID >= ItemTable.size())
				continue;

			Item& itemInfo = ItemTable[currentItemID];

			//Mesh* itemMesh = GetOrCreateColoredQuadMesh(itemInfo.color);

			int column = i % gridColumns;
			int row = i / gridColumns;

			float itemCurrentX = startItemX + column * (2 * (slotSize + slotPadding));
			float itemCurrentY = startItemY + row * (2 * (slotSize + slotPadding));

			/*vec4 v = gd.viewportArr[0].unproject(vec4(gd.ClientFrameWidth/2, gd.ClientFrameHeight/2, 0, 1));
			v *= 5;
			v += gd.viewportArr[0].Camera_Pos;*/

			/*vec4 unproj = gd.viewportArr[0].unproject(vec4(-0.5f, 0.5f, 0, 1));*/
			matrix itemMat;
			itemMat.pos = gd.viewportArr[0].Camera_Pos;
			//caminvMat.look.x *= -1;
			itemMat.pos += viewMat.look * 7;
			constexpr float xmul = 1.35f;
			constexpr float ymul = 0.825f;
			itemMat.pos += viewMat.right * (-2.7f + xmul * column);
			itemMat.pos += viewMat.up * (1.65f - ymul * row);
			itemMat.pos.w = 1;

			itemMat.transpose();
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &itemMat, 0);
			MyDiffuseTextureShader->SetTextureCommand(ItemTable[currentItemID].tex);
			ItemTable[currentItemID].MeshInInventory->Render(gd.pCommandList, 1);
			//RenderUIObject(itemMesh, itemMat);
		}

		gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);

		int i = 0;
		float top = startSlotY + (gridRows - 1) * (2 * (slotSize + slotPadding));
		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				if (player->Inventory[i].id == 0) {
					i++; continue;
				}

				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = top - row * (2 * (slotSize + slotPadding));

				ItemStack& currentStack = player->Inventory[i];
				std::wstring countStr = L"x" + std::to_wstring(currentStack.ItemCount);
				vec4 textRect = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize };
				float fontSize = 20.0f;
				RenderText(countStr.c_str(), countStr.length(), textRect, fontSize, 0.01f);
				i++;
			}
		}
	}

	//static float stackT = 0;
	//stackT += game.DeltaTime;
	//RenderText(L"0123456789\nHello, World!\n안녕 너무 반가워~~", 36, vec4(-1000, 0, 1000, 100), 50 + 50 * sin(stackT));

	//render end ----------------------------------------------------------------

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	//Command execution
	hResult = gd.pCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	gd.WaitGPUComplete();

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_last() {
	if (isPrepared == false) return;

	//load text texture
	if (gd.addTextureStack.size() > 0) {
		for (int i = 0; i < gd.addTextureStack.size(); ++i) {
			gd.AddTextTexture(gd.addTextureStack[i]);
		}
	}
	gd.addTextureStack.clear();

	gd.ShaderVisibleDescPool.Reset();

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.pCommandList->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	float pfClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	gd.pCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	gd.pCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE,
		&d3dDsvCPUDescriptorHandle);
	//render begin ----------------------------------------------------------------
	((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);
	MyDiffuseTextureShader->SetTextureCommand(&game.DefaultTex);

	/*XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(gd.viewportArr[0].ViewMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);*/

	matrix viewMat = gd.viewportArr[0].ViewMatrix;
	viewMat *= gd.viewportArr[0].ProjectMatrix;
	viewMat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);

	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);

	gd.pCommandList->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());

	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->isExist == false) continue;
		m_gameObjects[i]->Render();
	}

	// already droped items. (non move..)
	static float itemRotate = 0;
	itemRotate += DeltaTime;
	matrix mat;
	mat.trQ(vec4::getQ(vec4(0, itemRotate, 0, 0)));
	for (int i = 0; i < DropedItems.size(); ++i) {
		if (DropedItems[i].itemDrop.id != 0) {
			mat.pos = DropedItems[i].pos;
			mat.pos.w = 1;
			matrix rmat = XMMatrixTranspose(mat);
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &rmat, 0);
			MyDiffuseTextureShader->SetTextureCommand(ItemTable[DropedItems[i].itemDrop.id].tex);
			ItemTable[DropedItems[i].itemDrop.id].MeshInInventory->Render(gd.pCommandList, 1);
		}
	}

	((Shader*)MyOnlyColorShader)->Add_RegisterShaderCommand(gd.pCommandList);

	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Render();
	}

	for (int i = 0; i < game.NpcHPBars.size; ++i)
	{
		if (game.NpcHPBars.isAlloc(i))
		{
			matrix& hpBarWorldMat = game.NpcHPBars[i];
			hpBarWorldMat.transpose();
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &hpBarWorldMat, 0);
			game.HPBarMesh->Render(gd.pCommandList, 1);
		}
	}

	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	//((Shader*)MyShader)->Add_RegisterShaderCommand(gd.pCommandList);

	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;

	// UI/AfterDepthClear Render
	if (game.player != nullptr) {
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
	}

	// HP 
	//maybe..1706x960
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);
	((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);
	std::wstring ui_hp = L"HP: " + std::to_wstring(hhpp);
	RenderText(ui_hp.c_str(), ui_hp.length(), rt, 30);

	//Heat Gauge
	vec4 rt_heat = rt;
	rt_heat.y -= 80 * Rate;   // HP보다 약간 아래로 내림 (간격 50 정도)
	std::wstring ui_heat = L"Heat: " + std::to_wstring((int)HeatGauge);
	RenderText(ui_heat.c_str(), ui_heat.length(), rt_heat, 30);

	// Bullet
	rt = Rate * vec4(900, -800, 1550, -900);
	std::wstring ui_bullet = L"Bullet: " + std::to_wstring(bulletCount);
	RenderText(ui_bullet.c_str(), ui_bullet.length(), rt, 30);

	// Kill/Death Counter
	rt = Rate * vec4(1100, 920, 1550, 700);
	std::wstring ui_kd = std::to_wstring(kill) + L" / " + std::to_wstring(death);
	RenderText(L"Kill/Death", 10, rt, 30);
	rt = Rate * vec4(1190, 850, 1550, 600);
	RenderText(ui_kd.c_str(), ui_kd.length(), rt, 30);

	// Player name
	rt = Rate * vec4(-1650, 700, -1000, 500);
	std::wstring playerName = L"Player: Leo";
	RenderText(playerName.c_str(), playerName.length(), rt, 30);

	// ----------Inventory------------
	if (isInventoryOpen) {
		matrix orthoMatrix = XMMatrixOrthographicOffCenterLH(0.0f, (float)gd.ClientFrameWidth, (float)gd.ClientFrameHeight, 0.0f, 0.01f, 1.0f);
		matrix uiViewMat = orthoMatrix;
		uiViewMat.transpose();
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);
		MyScreenCharactorShader->SetTextureCommand(&DefaultTex);
		/*gd.pCommandList->SetPipelineState(MyOnlyColorShader->pUiPipelineState);*/
		//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &uiViewMat, 0);

		const int gridColumns = 5;
		const int gridRows = 5;
		const float slotSize = Rate * 150.0f;
		const float itemPadding = Rate * 20.0f;
		const float slotPadding = Rate * 10.0f;

		const float actualItemSize = slotSize - (itemPadding * 2);

		float invWidth = (gridColumns * slotSize) + ((gridColumns + 1) * slotPadding);
		float invHeight = (gridRows * slotSize) + ((gridRows + 1) * slotPadding);
		float invPosX = 0;
		float invPosY = 0;

		vec4 bgColor = { 0.2f, 0.2f, 0.2f, 0.8f };
		float CB[11] = { invPosX - invWidth ,invPosY - invHeight , invPosX + invWidth ,invPosY + invHeight, bgColor.r, bgColor.g, bgColor.b, bgColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.5f };
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 11, CB, 0);
		TextMesh->Render(gd.pCommandList, 1);

		vec4 slotColor = { 0.15f, 0.15f, 0.15f, 0.9f };
		//Mesh* slotMesh = GetOrCreateColoredQuadMesh(slotColor);

		float startSlotX = invPosX - invWidth + slotPadding + slotSize;
		float startSlotY = invPosY - invHeight + slotPadding + slotSize;

		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = startSlotY + row * (2 * (slotSize + slotPadding));

				float CB2[11] = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize, slotColor.r, slotColor.g, slotColor.b, slotColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.05f };
				gd.pCommandList->SetGraphicsRoot32BitConstants(0, 11, CB2, 0);
				TextMesh->Render(gd.pCommandList, 1);
			}
		}

		float startItemX = invPosX - Rate * 140.0f;
		float startItemY = invPosY - Rate * 130.0f;

		matrix viewMat2 = DirectX::XMMatrixLookAtLH(vec4(0, 0, 0), vec4(0, 0, 1), vec4(0, 1, 0));
		viewMat2 *= gd.viewportArr[0].ProjectMatrix;
		gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());

		for (int i = 0; i < player->Inventory.size(); ++i) {
			if (i >= gridColumns * gridRows)
				break;

			ItemStack& currentStack = player->Inventory[i];
			ItemID currentItemID = currentStack.id;
			if (currentItemID == 0) continue;

			if (currentItemID >= ItemTable.size())
				continue;

			Item& itemInfo = ItemTable[currentItemID];

			//Mesh* itemMesh = GetOrCreateColoredQuadMesh(itemInfo.color);

			int column = i % gridColumns;
			int row = i / gridColumns;

			float itemCurrentX = startItemX + column * (2 * (slotSize + slotPadding));
			float itemCurrentY = startItemY + row * (2 * (slotSize + slotPadding));

			/*vec4 v = gd.viewportArr[0].unproject(vec4(gd.ClientFrameWidth/2, gd.ClientFrameHeight/2, 0, 1));
			v *= 5;
			v += gd.viewportArr[0].Camera_Pos;*/

			/*vec4 unproj = gd.viewportArr[0].unproject(vec4(-0.5f, 0.5f, 0, 1));*/
			matrix itemMat;
			itemMat.pos = gd.viewportArr[0].Camera_Pos;
			//caminvMat.look.x *= -1;
			itemMat.pos += viewMat.look * 7;
			constexpr float xmul = 1.35f;
			constexpr float ymul = 0.825f;
			itemMat.pos += viewMat.right * (-2.7f + xmul * column);
			itemMat.pos += viewMat.up * (1.65f - ymul * row);
			itemMat.pos.w = 1;

			itemMat.transpose();
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &itemMat, 0);
			MyDiffuseTextureShader->SetTextureCommand(ItemTable[currentItemID].tex);
			ItemTable[currentItemID].MeshInInventory->Render(gd.pCommandList, 1);
			//RenderUIObject(itemMesh, itemMat);
		}

		gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);

		int i = 0;
		float top = startSlotY + (gridRows - 1) * (2 * (slotSize + slotPadding));
		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				if (player->Inventory[i].id == 0) {
					i++; continue;
				}

				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = top - row * (2 * (slotSize + slotPadding));

				ItemStack& currentStack = player->Inventory[i];
				std::wstring countStr = L"x" + std::to_wstring(currentStack.ItemCount);
				vec4 textRect = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize };
				float fontSize = 20.0f;
				RenderText(countStr.c_str(), countStr.length(), textRect, fontSize, 0.01f);
				i++;
			}
		}
	}

	//static float stackT = 0;
	//stackT += game.DeltaTime;
	//RenderText(L"0123456789\nHello, World!\n안녕 너무 반가워~~", 36, vec4(-1000, 0, 1000, 100), 50 + 50 * sin(stackT));

	//render end ----------------------------------------------------------------

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	//Command execution
	hResult = gd.pCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	gd.WaitGPUComplete();

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_ShadowPass()
{
	constexpr int ShadowResolusion = 4096;
	vec4 LightDirection = vec4(1, 2, 1);
	LightDirection.len3 = 1;
	constexpr float LightDistance = 500;
	game.MySpotLight.viewport.Viewport.Width = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.Height = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.MaxDepth = 1.0f;
	game.MySpotLight.viewport.Viewport.MinDepth = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftX = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftY = 0.0f;
	game.MySpotLight.viewport.ScissorRect = { 0, 0, (long)ShadowResolusion, (long)ShadowResolusion };

	vec4 obj = player->m_worldMatrix.pos;
	obj.w = 0;

	game.MySpotLight.viewport.Camera_Pos = obj + LightDirection * LightDistance;
	game.MySpotLight.viewport.Camera_Pos.w = 0;
	game.MySpotLight.LightPos = game.MySpotLight.viewport.Camera_Pos;

	MySpotLight.View.mat = XMMatrixLookAtLH(MySpotLight.LightPos, obj, vec4(0, 1, 0, 0));
	game.MySpotLight.viewport.ViewMatrix = MySpotLight.View;

	constexpr float rate = 1.0f / 32.0f;
	game.MySpotLight.viewport.ProjectMatrix = XMMatrixOrthographicLH(rate * ShadowResolusion, rate * ShadowResolusion, 0.1f, 1000.0f);

	matrix projmat = XMMatrixTranspose(MySpotLight.viewport.ProjectMatrix);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->LightProjection = projmat;
	LightCBData_withShadow->LightView = XMMatrixTranspose(MySpotLight.viewport.ViewMatrix);
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCB_withShadowResource.resource->Unmap(0, nullptr);

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->RSSetViewports(1, &game.MySpotLight.viewport.Viewport);
	gd.pCommandList->RSSetScissorRects(1, &game.MySpotLight.viewport.ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	/*float pfClearColor[4] = { 0, 0, 0, 1.0f };
	gd.pCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);*/

		//Render Shadow Map (Shadow Pass)
		//spotlight view
	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.pCommandList->OMSetRenderTargets(0, nullptr, TRUE, &game.MySpotLight.ShadowMap.hCpu);
	//there is no render target only depth map. ??
	gd.pCommandList->ClearDepthStencilView(game.MySpotLight.ShadowMap.hCpu,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	//MyShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::RegisterEnum::RenderShadowMap);

	//matrix xmf4x4Projection = game.MySpotLight.viewport.ProjectMatrix;
	//xmf4x4Projection.transpose();
	////XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	//for (auto& gbj : m_gameObjects) {
	//	gbj->RenderShadowMap();
	//}

	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::RegisterEnum::RenderShadowMap);

	matrix xmf4x4View = game.MySpotLight.viewport.ViewMatrix;
	xmf4x4View *= game.MySpotLight.viewport.ProjectMatrix;
	xmf4x4View.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16); // no matter

	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

	matrix mat2 = XMMatrixIdentity();
	//mat2.pos.y -= 1.75f;
	game.MySpotLight.viewport.UpdateFrustum();
	Hierarchy_Object::renderViewPort = &game.MySpotLight.viewport;
	game.Map->MapObjects[0]->Render_Inherit(mat2, Shader::RegisterEnum::RenderShadowMap);

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists_Shadow[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists_Shadow);

	gd.WaitGPUComplete();
}

void Game::Update()
{
	/*if(accSend >= SendPeriod)
	{
		char sendData[9];
		sendData[0] = InputID::MouseMove;
		*(int*)&sendData[1] = stackMouseX;
		*(int*)&sendData[5] = stackMouseY;
		ClientSocket->Send(sendData, 9);
		stackMouseX = 0;
		stackMouseY = 0;
	}*/

	static int accDeltax = 0;
	static int accDeltay = 0;
	static float accSend = 0.0f;
	const float SendPeriod = 0.1f;

	accDeltax += m_stackMouseX;
	accDeltay += m_stackMouseY;
	m_stackMouseX = 0;
	m_stackMouseY = 0;

	accSend += DeltaTime;
	while (accSend >= SendPeriod) {
		if (accDeltax != 0 || accDeltay != 0) {
			char sendData[9];
			sendData[0] = InputID::MouseMove;
			*(int*)&sendData[1] = accDeltax;
			*(int*)&sendData[5] = accDeltay;
			ClientSocket->Send(sendData, 9);
			accDeltax = 0;
			accDeltay = 0;
			SetCursorPos(gd.screenWidth / 2, gd.screenHeight / 2);
		}
		accSend -= SendPeriod;
	}

	while (true) {
		int result = ClientSocket->Receive();
		if (result > 0) {
			char* cptr = ClientSocket->m_receiveBuffer;

		RECEIVE_LOOP:
			int offset = game.Receiving(cptr, result);
			cptr += offset;
			result -= offset;
			if (result > 1) {
				goto RECEIVE_LOOP;
			}
		}
		else break;
	}

	if (isPrepared == false) {
		if (game.isPreparedGo) {
			preparedFlow += DeltaTime;
			if (preparedFlow > 2.0f) isPrepared = true;
		}
		return;
	}

	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->isExist == false) continue;
		m_gameObjects[i]->Update(DeltaTime);
	}

	if (playerGameObjectIndex >= 0 && playerGameObjectIndex < m_gameObjects.size()) {
		Player* p = (Player*)m_gameObjects[playerGameObjectIndex];
		p->ClientUpdate(DeltaTime);
		player = (Player*)m_gameObjects[playerGameObjectIndex];
	}

	if (player != nullptr) {
		//CameraUpdate
		XMVECTOR vEye = player->m_worldMatrix.pos;
		vec4 peye = player->m_worldMatrix.pos;
		vec4 pat = player->m_worldMatrix.pos;

		const float rate = 0.005f;

		// client x = 0; rx = 5; server = 5;
		// x 0 >> rx soft interpolation. by time..

		// client send stack XY >> too fast so slow down..
		// server send deltaMousePos
		// get time diffrence 1time per 0.2 second 
		// define solution. please.
		XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(rate * DeltaMousePos.y, rate * DeltaMousePos.x, 0);
		vec4 clook = { 0, 0, 1, 1 };
		vec4 plook;
		clook = XMVector3Rotate(clook, quaternion);

		plook = clook;
		plook.y = 0;
		plook.len3 = 1;
		player->m_worldMatrix.LookAt(plook);

		if (bFirstPersonVision) {
			peye += 1.0f * player->m_worldMatrix.up;
			pat += 1.0f * player->m_worldMatrix.up;
			pat += 10 * clook;
		}
		else {
			peye -= 3 * clook;
			pat += 10 * clook;
			peye += 0.5f * player->m_worldMatrix.up;
			peye += 0.5f * player->m_worldMatrix.right;
		}

		XMFLOAT3 Up = { 0, 1, 0 };
		gd.viewportArr[0].ViewMatrix = XMMatrixLookAtLH(peye, pat, XMLoadFloat3(&Up));
		gd.viewportArr[0].Camera_Pos = peye;

		/*for (auto& gbj : m_gameObjects) {
			if(gbj->isExist) gbj->Update(DeltaTime);
		}*/

		for (int i = 0; i < bulletRays.size; ++i) {
			if (bulletRays.isnull(i)) continue;
			bulletRays[i].Update();
			if (bulletRays[i].direction.fast_square_of_len3 < 0.01f) {
				bulletRays.Free(i);
			}
		}
	}

	// Collision......

	//for (int i = 0; i < m_gameObjects.size(); ++i) {
	//	GameObject* gbj1 = m_gameObjects[i];
	//	if (gbj1->isExist == false) continue;
	//	if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;

	//	for (int j = i + 1; j < m_gameObjects.size(); ++j) {
	//		GameObject* gbj2 = m_gameObjects[j];
	//		if (gbj2->isExist == false) continue;
	//		GameObject::CollisionMove(gbj1, gbj2);
	//	}

	//	//gbj1->tickLVelocity.w = 0;
	//	gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
	//	gbj1->tickLVelocity = XMVectorZero();
	//}
}

//not using
void Game::Event(WinEvent evt)
{
	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->isExist == false) continue;
		m_gameObjects[i]->Event(evt);
	}
}

int Game::Receiving(char* ptr, int totallen)
{
	static char savBuff[sizeof(twoPage)] = {};
	static int savBuffup = 0;
	static char cuttingType = 't'; // 't' : type, 'm' : meta, 'd' : data
	static bool isCutting = false;
	static bool is_Pack_Reading = false;
	constexpr int typedata_size = 2;

	int offset = 0;

	if (isCutting) {
		int readlen = totallen;
		if (totallen + savBuffup > sizeof(twoPage)) readlen = sizeof(twoPage) - savBuffup;
		memcpy(&savBuff[savBuffup], ptr, readlen);
		isCutting = false;
		int roffset = Receiving(savBuff, sizeof(twoPage));
		offset = roffset - savBuffup;
		savBuffup = 0;
		return offset;
	}

	if (totallen < offset + typedata_size) {
		isCutting = true;
		cuttingType = 't';
		memcpy(savBuff, ptr, totallen);
		savBuffup = totallen;
		offset = totallen;
		return offset;
	}
	ServerInfoType type = *(ServerInfoType*)ptr;
	offset += typedata_size;

	switch (type) {
	case ServerInfoType::NewGameObject:
	{
		ui32 newobjindex = *(unsigned short*)&ptr[offset];
		offset += 4;
		GameObjectType gtype = *(GameObjectType*)&ptr[offset];
		if (gtype.id >= GameObjectType::ObjectTypeCount) {
			return 2;
		}
		offset += 2;
		int newDataSize = GameObjectType::ClientSizeof[gtype];
		int newDataSize_server = GameObjectType::ServerSizeof[gtype];
		int minsiz = min(newDataSize, newDataSize_server);
		void* go = malloc(newDataSize);
		ZeroMemory(go, newDataSize);
		memcpy_s(go, minsiz, &ptr[offset], minsiz);
		*(void**)go = GameObjectType::vptr[gtype];
		GameObject* newGameObject = (GameObject*)go;
		newGameObject->m_pMesh = nullptr;
		newGameObject->m_pShader = nullptr;
		newGameObject->Destpos = newGameObject->m_worldMatrix.pos;
		newGameObject->rmod = GameObject::eRenderMeshMod::single_Mesh;
		offset += newDataSize_server;
		if (newobjindex >= m_gameObjects.size()) {
			m_gameObjects.push_back(newGameObject);
		}
		else {
			if (m_gameObjects[newobjindex] == nullptr) {
				m_gameObjects[newobjindex] = newGameObject;
			}
			else {
				//error!!
			}
		}
		if (gtype == GameObjectType::_Monster) {
			int hpBarIndex = NpcHPBars.Alloc();

			Monster* pMonster = (Monster*)newGameObject;
			pMonster->HPBarIndex = hpBarIndex;
			NpcHPBars[hpBarIndex] = pMonster->HPMatrix;
		}
		else if (gtype == GameObjectType::_Player) {
			if (newobjindex == game.playerGameObjectIndex) {
				if (playerGameObjectIndex >= 0 && playerGameObjectIndex < m_gameObjects.size()) {
					player = (Player*)m_gameObjects[playerGameObjectIndex];
					player->Gun = game.GunMesh;

					player->gunMatrix_thirdPersonView.Id();
					player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);

					player->gunMatrix_firstPersonView.Id();
					player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
					player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - player->gunMatrix_firstPersonView.pos);

					player->ShootPointMesh = game.ShootPointMesh;

					player->HPBarMesh = game.HPBarMesh;
					player->HPMatrix.pos = vec4(-1, 1, 1, 1);
					player->HPMatrix.LookAt(vec4(-1, 0, 0));

					player->HeatBarMesh = game.HeatBarMesh;
					player->HeatBarMatrix.pos = vec4(-1, 1, 1, 1);
					player->HeatBarMatrix.LookAt(vec4(-1, 0, 0));
				}

				game.isPreparedGo = true;
			}
		}

		dbglog2(L"[New Obj] objindex = %d, type = %d\n", newobjindex, gtype.id);
	}
	break;
	case ServerInfoType::ChangeMemberOfGameObject:
	{
		int objindex = *(int*)&ptr[offset];
		if (game.m_gameObjects.size() <= objindex || objindex < 0) {
			return 2;
		}
		offset += 4;
		GameObjectType gotype;
		gotype = *(GameObjectType*)&ptr[offset];
		offset += 2;
		int clientMemberOffset = *(short*)&ptr[offset];
		offset += 2;
		//#ifdef _DEBUG
		//		clientMemberOffset += 16;
		//#endif
		int memberSize = *(short*)&ptr[offset];
		offset += 2;
		//memcpy_s((char*)m_gameObjects[objindex] + clientMemberOffset, memberSize, &ptr[offset], memberSize);
		if (0 <= objindex && objindex < m_gameObjects.size()) {
			if (clientMemberOffset + memberSize > GameObjectType::ClientSizeof[gotype]) {
				memberSize = GameObjectType::ClientSizeof[gotype] - clientMemberOffset;
			}
			if (memberSize > 0) {
				memcpy_s((char*)m_gameObjects[objindex] + clientMemberOffset, memberSize, &ptr[offset], memberSize);
			}
		}
		else {
			offset += memberSize;
			return offset;
		}

		/*char* ptr = (char*)m_gameObjects[objindex] + clientMemberOffset;
		for (int i = 0; i < 4; ++i) {
			int dd = 4 * i;
			swap(ptr[dd + 0], ptr[dd+3]);
			swap(ptr[dd+ 1], ptr[dd+2]);
		}*/
		offset += memberSize;
	}
	break;
	case ServerInfoType::NewRay:
	{
		vec4 start, direction;
		float distance;
		start.f3 = *(XMFLOAT3*)&ptr[offset];
		start.w = 0;
		offset += 12;
		direction.f3 = *(XMFLOAT3*)&ptr[offset];
		direction.w = 0;
		offset += 12;
		distance = *(float*)&ptr[offset];
		offset += 4;

		int BulletIndex = bulletRays.Alloc();
		if (BulletIndex < 0) {
			return offset;
		}
		BulletRay& bray = bulletRays[BulletIndex];
		bray = BulletRay(start, direction, distance);
	}
	break;
	case ServerInfoType::SetMeshInGameObject:
	{
		int objindex = *(int*)&ptr[offset];
		if (game.m_gameObjects.size() <= objindex || objindex < 0) {
			return 2;
		}
		offset += 4;
		int slen = *(int*)&ptr[offset];
		offset += 4;
		string str;
		str.reserve(slen); str.resize(slen);
		memcpy_s(&str[0], slen, &ptr[offset], slen);
		offset += slen;

		// is this AI?? why not describe in comment???

		//9월8일
		// 1) 이름 그대로 출력
		char dbg[256];
		snprintf(dbg, sizeof(dbg), "[SET_MESH] obj=%d name='%s' len=%d\n", objindex, str.c_str(), slen);
		OutputDebugStringA(dbg);
		
		// 2) 매핑 존재 여부 확인
		auto it = Shape::StrToShapeMap.find(str);
		if (it == Shape::StrToShapeMap.end()) {
			snprintf(dbg, sizeof(dbg), "[SET_MESH][ERROR] name '%s' NOT FOUND in meshmap\n", str.c_str());
			OutputDebugStringA(dbg);
			// 안전장치: nullptr 세팅하고 리턴(혹은 기본 메시 붙이기)
			m_gameObjects[objindex]->m_pMesh = nullptr;
			m_gameObjects[objindex]->m_pShader = nullptr;
		}
		else {
			// 3) 실제 포인터 출력
			Shape s = it->second;
			if (s.isMesh()) {
				Mesh* mp = s.GetMesh();
				snprintf(dbg, sizeof(dbg), "[SET_MESH] name '%s' -> ptr=%p\n", str.c_str(), (void*)mp);
				OutputDebugStringA(dbg);

				m_gameObjects[objindex]->rmod = GameObject::eRenderMeshMod::single_Mesh;
				m_gameObjects[objindex]->m_pMesh = mp;
				m_gameObjects[objindex]->m_pShader = MyShader; // 셰이더도 셋
			}
			else {
				Model* mp = s.GetModel();

				snprintf(dbg, sizeof(dbg), "[SET_MODEL] name '%s' -> ptr=%p\n", str.c_str(), (void*)mp);
				OutputDebugStringA(dbg);

				m_gameObjects[objindex]->rmod = GameObject::eRenderMeshMod::Model;
				m_gameObjects[objindex]->pModel = mp;
				m_gameObjects[objindex]->m_pShader = MyPBRShader1; // 셰이더도 셋
			}
		}

		//m_gameObjects[objindex]->m_pMesh = Mesh::meshmap[str];
		//m_gameObjects[objindex]->m_pShader = MyShader;

		//tempcode..?? >> when fix??
		if (m_gameObjects[objindex]->m_pMesh == Shape::StrToShapeMap["Ground001"].GetMesh()) {
			m_gameObjects[objindex]->diffuseTextureIndex = 0;
		}
		else if (m_gameObjects[objindex]->m_pMesh == Shape::StrToShapeMap["Wall001"].GetMesh()) {
			m_gameObjects[objindex]->diffuseTextureIndex = 1;
		}
		else if (m_gameObjects[objindex]->m_pMesh == Shape::StrToShapeMap["Monster001"].GetMesh()) {
			m_gameObjects[objindex]->diffuseTextureIndex = 2;
		}
	}
	break;
	case ServerInfoType::AllocPlayerIndexes:
	{
		int clientindex = *(int*)&ptr[offset];
		offset += 4;
		int objindex = *(int*)&ptr[offset];
		offset += 4;
		game.clientIndexInServer = clientindex;
		game.playerGameObjectIndex = objindex;

		if (game.m_gameObjects.size() <= objindex || objindex < 0) {
			return offset;
		}

		if (playerGameObjectIndex >= 0 && playerGameObjectIndex < m_gameObjects.size()) {
			player = (Player*)m_gameObjects[playerGameObjectIndex];
			player->Gun = game.GunMesh;

			player->gunMatrix_thirdPersonView.Id();
			player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);

			player->gunMatrix_firstPersonView.Id();
			player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
			player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - player->gunMatrix_firstPersonView.pos);

			player->ShootPointMesh = game.ShootPointMesh;

			player->HPBarMesh = game.HPBarMesh;
			player->HPMatrix.pos = vec4(-1, 1, 1, 1);
			player->HPMatrix.LookAt(vec4(-1, 0, 0));

			player->HeatBarMesh = game.HeatBarMesh;
			player->HeatBarMatrix.pos = vec4(-1, 1, 1, 1);
			player->HeatBarMatrix.LookAt(vec4(-1, 0, 0));

			player->Inventory = vector<ItemStack>();
			player->Inventory.reserve(36);
			player->Inventory.resize(36);
			for (int i = 0; i < 36; ++i) {
				player->Inventory[i].id = 0;
				player->Inventory[i].ItemCount = 0;
			}
		}

		game.isPreparedGo = true;
	}
	break;
	case ServerInfoType::DeleteGameObject:
	{
		int objindex = *(int*)&ptr[offset];
		if (game.m_gameObjects.size() <= objindex || objindex < 0) {
			return 2;
		}
		offset += 4;
		m_gameObjects[objindex]->isExist = false;
		delete m_gameObjects[objindex];
		m_gameObjects[objindex] = nullptr;
	}
	break;
	case ServerInfoType::PACK:
	{
		constexpr int metadata_size = 8;
		if (totallen < offset + metadata_size) {
			isCutting = true;
			cuttingType = 'm';
			memcpy(savBuff, ptr, totallen);
			savBuffup = totallen;
			offset = totallen;
			return offset;
		}
		int order_id = *(int*)&ptr[offset];
		offset += 4;
		int datasiz = *(int*)&ptr[offset];
		offset += 4;

		if (totallen < offset + datasiz - 10) {
			isCutting = true;
			cuttingType = 'd';
			memcpy(savBuff, ptr, totallen);
			savBuffup = totallen;
			offset = totallen;
			return offset;
		}
		if (order_id == 0) {
			pack_factory.Clear();
		}
		bool ready_to_read = pack_factory.Recieve(ptr, datasiz);
		offset += datasiz - 10;
		if (ready_to_read) goto PACK_READ;
		return offset;
	}
	break;
	case ServerInfoType::PACK_END:
	{
		constexpr int metadata_size = 8;
		if (totallen < offset + metadata_size) {
			isCutting = true;
			cuttingType = 'm';
			memcpy(savBuff, ptr, totallen);
			savBuffup = totallen;
			offset = totallen;
			return offset;
		}
		int order_id = *(int*)&ptr[offset];
		offset += 4;
		int datasiz = *(int*)&ptr[offset];
		offset += 4;

		if (totallen < offset + datasiz - 10) {
			isCutting = true;
			cuttingType = 'd';
			memcpy(savBuff, ptr, totallen);
			savBuffup = totallen;
			offset = totallen;
			return offset;
		}
		bool ready_to_read = pack_factory.Recieve(ptr, datasiz);
		offset += datasiz - 10;
		if (ready_to_read) goto PACK_READ;
	}
	break;
	case ServerInfoType::ItemDrop:
	{
		int newindex = 0;
		ItemLoot il;
		newindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		il = *(ItemLoot*)&ptr[offset];
		offset += sizeof(ItemLoot);

		if (newindex >= game.DropedItems.size()) {
			while (newindex >= game.DropedItems.size()) {
				ItemLoot til;
				til.pos = 0;
				til.itemDrop.id = 0;
				til.itemDrop.ItemCount = 0;
				game.DropedItems.push_back(til);
			}
			game.DropedItems[newindex] = il;
		}
		else {
			game.DropedItems[newindex] = il;
		}
		return offset;
	}
	break;
	case ServerInfoType::ItemDropRemove:
	{
		int dindex = 0;
		ItemLoot il;
		dindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		game.DropedItems[dindex].itemDrop.id = 0;
		game.DropedItems[dindex].itemDrop.ItemCount = 0;
		game.DropedItems[dindex].pos = vec4(0, 0, 0, 0);
		return offset;
	}
	break;
	case ServerInfoType::InventoryItemSync:
	{
		int inventoryindex = 0;
		ItemStack il;
		il = *(ItemStack*)&ptr[offset];
		offset += sizeof(ItemStack);
		inventoryindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		game.player->Inventory[inventoryindex].id = il.id;
		game.player->Inventory[inventoryindex].ItemCount = il.ItemCount;
		return offset;
	}
	break;
	}
	return offset;

PACK_READ:
	for (int i = 0; i < pack_factory.packs.size(); ++i) {
		if (pack_factory.packs[i].up > 0) {
			char* cptr = pack_factory.packs[i].data.data;
			cptr += 10;
			int result = pack_factory.packs[i].up - 10;
			int offset = 0;
		RECEIVE_LOOP:
			offset = game.Receiving(cptr, result);
			cptr += offset;
			result -= offset;
			if (result > 1) {
				goto RECEIVE_LOOP;
			}
		}
	}
	return offset;
}

void Game::AddMouseInput(int deltaX, int deltaY)
{
	if (player != nullptr)
	{
		m_stackMouseX += deltaX;
		m_stackMouseY += deltaY;

		DeltaMousePos.x += deltaX;
		DeltaMousePos.y += deltaY;

		if (DeltaMousePos.y > 200) {
			DeltaMousePos.y = 200;
		}
		if (DeltaMousePos.y < -200) {
			DeltaMousePos.y = -200;
		}
	}
}

//not using
void Game::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance)
{
	vec4 rayOrigin = rayStart;

	GameObject* closestHitObject = nullptr;

	float closestDistance = rayDistance;

	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->isExist == false) continue;
		if (shooter == m_gameObjects[i]) continue;
		BoundingOrientedBox obb = m_gameObjects[i]->GetOBB();
		float distance;

		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
			if (distance <= rayDistance) {
				if (distance < closestDistance) {
					closestDistance = distance;
					closestHitObject = m_gameObjects[i];
					m_gameObjects[i]->OnCollisionRayWithBullet();
				}
			}
		}
	}

	int BulletIndex = bulletRays.Alloc();
	if (BulletIndex < 0) {
		return;
	}
	BulletRay& bray = bulletRays[BulletIndex];
	bray = BulletRay(rayStart, rayDirection, closestDistance);

	if (closestHitObject) {
		Player* hitPlayer = (Player*)closestHitObject;

		if (hitPlayer == game.player) {
			hitPlayer->TakeDamage(10.0f);
		}

		//OutputDebugString(L"Hit a TargetBoard!\n");
	}
	else {
		//OutputDebugString(L"Not hit any TargetBoard.\n");
	}
}

//this function must call after ScreenCharactorShader register in pipeline.
void Game::RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth)
{
	//fontsize = 100 -> 0.1x | 10 -> 0.01x
	vec4 pos = vec4(Rect.x, Rect.y, 0, 0);
	constexpr float Default_LineHeight = 750;
	float mul = fontsiz / Default_LineHeight;

	constexpr float lineheight_mul = 2.5f;
	float lineheight = lineheight_mul * fontsiz;
	for (int i = 0; i < length; ++i) {
		wchar_t wc = wstr[i];
		if (wc == L'\n') {
			pos.x = Rect.x;
			pos.y -= lineheight;
			continue;
		}
		else if (wc == L' ') {
			pos.x += fontsiz;
			continue;
		}
		bool textureExist = false;
		GPUResource* texture = nullptr;
		Glyph g;
		for (int k = 0; k < gd.FontCount; ++k) {
			if (gd.font_texture_map[k].find(wc) != gd.font_texture_map[k].end()) {
				textureExist = true;
				texture = &gd.font_texture_map[k][wc];
				g = gd.font_data[k].glyphs[wc];
				break;
			}
		}

		if (textureExist == false) {
			gd.addTextureStack.push_back(wc);
			continue;
		}

		//set root variables
		vec4 textRt = vec4(pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul);
		float tConst[7] = { textRt.x, textRt.y, textRt.z, textRt.w, gd.ClientFrameWidth, gd.ClientFrameHeight, depth };
		/*gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &textRt, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameWidth, 4);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameHeight, 5);*/
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 7, &tConst, 0);
		MyScreenCharactorShader->SetTextureCommand(texture);

		//Render Text
		TextMesh->Render(gd.pCommandList, 1);

		//calculate next location of text
		pos.x += g.advance_width * mul;
		if (pos.x > Rect.z) {
			pos.x = Rect.x;
			pos.y -= lineheight;
			if (pos.y > Rect.w) {
				return;
			}
		}
	}
}