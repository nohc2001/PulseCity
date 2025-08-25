#include "main.h"

unordered_map<type_offset_pair, short, hash<type_offset_pair>> GameObjectType::GetClientOffset;
unordered_map<void*, GameObjectType> GameObjectType::VptrToTypeTable;
vector<string> Mesh::MeshNameArr;
unordered_map<string, Mesh*> Mesh::meshmap;

World gameworld;
float DeltaTime = 0;

int main() {
	PrintOffset();

	Socket listenSocket(SocketType::Tcp); // 1
	listenSocket.Bind(Endpoint("0.0.0.0", 1000)); // 2
	listenSocket.SetReceiveBuffer(new twoPage, sizeof(twoPage));

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.SetNonblocking();
	vector<PollFD> readFds;
	//??
	listenSocket.Listen();	// 3
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	gameworld.Init();

	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.016) { // limiting fps.
			DeltaTime = (float)DeltaFlow;
			gameworld.Update();
			DeltaFlow = 0;
		}

		readFds.reserve(gameworld.clients.size + 1);
		readFds.clear();

		PollFD item2;
		item2.m_pollfd.events = POLLRDNORM;
		item2.m_pollfd.fd = listenSocket.m_fd;
		item2.m_pollfd.revents = 0;
		readFds.push_back(item2); // first is listen socket.
		for (int i=0;i< gameworld.clients.size;++i)
		{
			PollFD item;
			item.m_pollfd.events = POLLRDNORM;
			item.m_pollfd.fd = gameworld.clients[i].socket.m_fd;
			item.m_pollfd.revents = 0;
			readFds.push_back(item);
		}

		// I/O 가능 이벤트가 있을 때까지 기다립니다.
		Poll(readFds.data(), (int)readFds.size(), 50);
		
		int num = 0;
		for (auto readFd : readFds)
		{
			if (readFd.m_pollfd.revents != 0)
			{
				if (num == 0) {
					// 4
					Socket tcpConnection;
					string ignore;

					int newindex = gameworld.clients.Alloc();
					if (newindex >= 0) {
						listenSocket.Accept(gameworld.clients[newindex].socket, ignore);
						auto a = gameworld.clients[newindex].socket.GetPeerAddr().ToString();
						gameworld.clients[newindex].socket.SetNonblocking();
						if (gameworld.clients[newindex].socket.m_receiveBuffer == nullptr) {
							gameworld.clients[newindex].socket.SetReceiveBuffer(new twoPage, 4096 * 2);
						}

						// player index sending
						int indexes[2] = {};
						int clientindex = newindex;
						gameworld.SendingAllObjectForNewClient(newindex);

						Player* p = new Player();
						p->clientIndex = clientindex;
						p->m_worldMatrix.Id();
						p->m_worldMatrix.pos.f3.y = 5;
						p->MeshIndex = Mesh::GetMeshIndex("Player");
						p->mesh = *Mesh::meshmap["Player"];
						int objindex = gameworld.NewObject(p, GameObjectType::_Player);
						gameworld.clients[clientindex].pObjData = p;
						int datacap = gameworld.Sending_AllocPlayerIndex(newindex, objindex);
						gameworld.clients[clientindex].socket.Send(gameworld.tempbuffer.data, datacap);
						gameworld.clients[clientindex].objindex = objindex;

						if (p->MeshIndex >= 0) {
							datacap = gameworld.Sending_SetMeshInGameObject(objindex, Mesh::MeshNameArr[p->MeshIndex]);
							gameworld.clients[clientindex].socket.Send((char*)gameworld.tempbuffer.data, datacap);
						}
						
						//print log
						cout << "Socket from " << a << " is accepted.\n";
						char newParticipantstr[128] = {};
						sprintf_s(newParticipantstr, "New client - %d\n", newindex);
						cout << newParticipantstr << endl;
					}
					else {
						cout << "too many client!!" << endl;
					}
				}
				else {
					int index = num - 1;
					if (gameworld.clients.isnull(index)) continue;
					int result = gameworld.clients[index].socket.Receive();
					if (result > 0) {
						char* cptr = gameworld.clients[index].socket.m_receiveBuffer;

						RECEIVE_LOOP:
						int offset = gameworld.Receiving(index, cptr);
						cptr += offset;
						result -= offset;
						if (result > 1) {
							goto RECEIVE_LOOP;
						}
					}
					else if (result <= 0) {
						cout << "client " << index << " Left the Game." << endl;
						int objindex = gameworld.clients[index].objindex;
						delete gameworld.clients[index].pObjData;
						gameworld.clients[index].pObjData = nullptr;
						gameworld.gameObjects[objindex] = nullptr;
						gameworld.gameObjects.Free(objindex);
						gameworld.clients.Free(index);
					}
				}
			}
			num += 1;
		}

		st = ft;
	}
	return 0;
}

void Mesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
{
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
		}
	}
	// For each vertex of each triangle
	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}
	MAXpos.x = (maxPos.x - minPos.x) * 0.5f;
	MAXpos.y = (maxPos.y - minPos.y) * 0.5f;
	MAXpos.z = (maxPos.z - minPos.z) * 0.5f;
	MAXpos *= 0.01f;
}

BoundingOrientedBox Mesh::GetOBB()
{
	return BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::CreateWallMesh(float width, float height, float depth)
{
	MAXpos = XMFLOAT3(width, height, depth);
}

int Mesh::GetMeshIndex(string meshName)
{
	for (int i = 0; i < MeshNameArr.size(); ++i) {
		if (MeshNameArr[i] == meshName) {
			return i;
		}
	}
	return -1;
}

int Mesh::AddMeshName(string meshName)
{
	int r = MeshNameArr.size();
	MeshNameArr.push_back(meshName);
	return r;
}

GameObject::GameObject()
{
	isExist = false;
	m_worldMatrix.Id();
	mesh.MAXpos = 0;
	LVelocity = 0;
	tickLVelocity = 0;
}

GameObject::~GameObject()
{
}

void GameObject::Update(float deltaTime)
{
}

void GameObject::Event()
{
}

BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local = mesh.GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, m_worldMatrix);
	return obb_world;
}

void GameObject::CollisionMove(GameObject* gbj1, GameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());

	GameObject* movObj = nullptr;
	GameObject* colObj = nullptr;
	BoundingOrientedBox obb1 = gbj1->GetOBB();
	BoundingOrientedBox obb2 = gbj2->GetOBB();

Collision_By_Move:

	if (!bi && bj) {
		// i : moving GameObject
		// j : Collision Check GameObject
		movObj = gbj1;
		colObj = gbj2;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (bi && !bj) {
		// i : Collision Check GameObject
		// j : moving GameObject
		movObj = gbj2;
		colObj = gbj1;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (!bi && !bj) {
		// i : moving GameObject
		// j : moving GameObject
		gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
		obb1 = gbj1->GetOBB();
		gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
		gbj2->m_worldMatrix.pos += gbj2->tickLVelocity;
		obb2 = gbj2->GetOBB();
		gbj2->m_worldMatrix.pos -= gbj2->tickLVelocity;

		if (obb1.Intersects(obb2)) {
			gbj1->OnCollision(gbj2);
			gbj2->OnCollision(gbj1);

			float mul = 0.5f;
			float rate = 0.25f;
			float maxLen = XMVector3Length(gbj1->tickLVelocity).m128_f32[0];
			float temp = XMVector3Length(gbj2->tickLVelocity).m128_f32[0];
			if (maxLen < temp) maxLen = temp;

		CMP_INTERSECT:
			vec4 v1 = mul * gbj1->tickLVelocity;
			vec4 v2 = mul * gbj2->tickLVelocity;
			gbj1->m_worldMatrix.pos += v1;
			obb1 = gbj1->GetOBB();
			gbj1->m_worldMatrix.pos -= v1;
			gbj2->m_worldMatrix.pos += v2;
			obb2 = gbj2->GetOBB();
			gbj2->m_worldMatrix.pos -= v2;
			bool isMoveForward = false;
			if (obb1.Intersects(obb2)) {
				mul -= rate;
				rate *= 0.5f;
				if (maxLen * rate > epsillon) goto CMP_INTERSECT;
			}
			else {
				isMoveForward = true;
				mul += rate;
				rate *= 0.5f;
				if (maxLen * rate > epsillon) goto CMP_INTERSECT;
			}

			if (isMoveForward == false) {
				v1 = 0;
				v2 = 0;
			}

			vec4 preMove1 = v1;
			vec4 postMove1 = gbj1->tickLVelocity - v1;
			gbj1->tickLVelocity = postMove1;

			vec4 preMove2 = v2;
			vec4 postMove2 = gbj2->tickLVelocity - v2;
			gbj2->tickLVelocity = postMove2;

			gbj2->m_worldMatrix.pos += preMove2;
			obb2 = gbj2->GetOBB();
			gbj2->m_worldMatrix.pos -= preMove2;

			CollisionMove_DivideBaseline_rest(gbj1, gbj2, obb2, preMove1);
			gbj1->tickLVelocity += preMove1;

			gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
			obb1 = gbj1->GetOBB();
			gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;

			CollisionMove_DivideBaseline_rest(gbj2, gbj1, obb1, preMove2);
			gbj2->tickLVelocity += preMove2;
		}
		return;
	}
	else {
		//no move
		return;
	}

Collision_By_Move_static_vs_dynamic:
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	obb2 = colObj->GetOBB();

	if (obb1.Intersects(obb2)) {
		movObj->OnCollision(colObj);
		colObj->OnCollision(movObj);

		CollisionMove_DivideBaseline(movObj, colObj, obb2);
	}
}

void GameObject::CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB)
{
	XMMATRIX basemat = colObj->m_worldMatrix;
	XMMATRIX invmat = colObj->m_worldMatrix.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}
}

void GameObject::CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove)
{
	movObj->m_worldMatrix.pos += preMove;

	XMMATRIX basemat = colObj->m_worldMatrix;
	XMMATRIX invmat = colObj->m_worldMatrix.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	movObj->m_worldMatrix.pos -= preMove;
}

void GameObject::OnCollision(GameObject* other)
{
}

void GameObject::OnCollisionRayWithBullet()
{
}

void Player::Update(float deltaTime)
{
	ShootFlow += deltaTime;
	if (ShootFlow >= ShootDelay) ShootFlow = ShootDelay;
	float shootrate = powf(ShootFlow / ShootDelay, 5);
	constexpr float RotHeight = ShootDelay * 10;

	recoilFlow += deltaTime;
	if (recoilFlow < recoilDelay) {
		float power = 5;
		float delta_rate = (power / recoilDelay) * pow(1 - recoilFlow / recoilDelay, (power - 1));
		float f = recoilVelocity * deltaTime * delta_rate;
		DeltaMousePos.y -= f;
	}

	if (collideCount == 0) isGround = false;
	collideCount = 0;

	if (isGround == false) {
		LVelocity.y -= 9.8f * deltaTime;
	}

	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	constexpr float speed = 3.0f;

	if (isGround) {
		if (InputBuffer[InputID::KeyboardSpace]) {
			LVelocity.y = JumpVelocity;
			isGround = false;
		}
	}
	tickLVelocity = LVelocity * deltaTime;

	if (InputBuffer[InputID::KeyboardW] == true) {
		tickLVelocity += speed * m_worldMatrix.look * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardS] == true) {
		tickLVelocity -= speed * m_worldMatrix.look * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardA] == true) {
		tickLVelocity -= speed * m_worldMatrix.right * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardD] == true) {
		tickLVelocity += speed * m_worldMatrix.right * deltaTime;
	}

	//if (InputBuffer[InputID::MouseLbutton] == true) {
	//	if (ShootFlow >= ShootDelay) {
	//		if (bFirstPersonVision) {
	//			matrix shootmat = ViewMatrix.RTInverse;
	//			gameworld.FireRaycast((GameObject*)this, shootmat.pos + shootmat.look, shootmat.look, 50.0f);
	//		}
	//		else {
	//			matrix shootmat = ViewMatrix.RTInverse;
	//			gameworld.FireRaycast((GameObject*)this, shootmat.pos + shootmat.look * 3, shootmat.look, 50.0f);
	//		}
	//		ShootFlow = 0;
	//		recoilFlow = 0;
	//	}
	//}

	//CameraUpdate
	XMVECTOR vEye = m_worldMatrix.pos;
	vec4 peye = m_worldMatrix.pos;
	vec4 pat = m_worldMatrix.pos;

	const float rate = 0.005f;

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(rate * DeltaMousePos.y, rate * DeltaMousePos.x, 0);
	vec4 clook = { 0, 0, 1, 0 };
	vec4 plook;
	clook = XMVector3Rotate(clook, quaternion);

	plook = clook;
	plook.y = 0;
	plook.len3 = 1;
	plook.w = 0;
	m_worldMatrix.SetLook(plook);

	if (bFirstPersonVision) {
		peye += 1.0f * m_worldMatrix.up;
		pat += 1.0f * m_worldMatrix.up;
		pat += 10 * clook;
	}
	else {
		peye -= 3 * clook;
		pat += 10 * clook;
		peye += 0.5f * m_worldMatrix.up;
		peye += 0.5f * m_worldMatrix.right;
	}

	XMFLOAT3 Up = { 0, 1, 0 };
	peye.w = 0;
	pat.w = 0;
	ViewMatrix = XMMatrixLookAtLH(peye, pat, XMLoadFloat3(&Up));

	if (InputBuffer[InputID::MouseLbutton] == true) {
		if (ShootFlow >= ShootDelay) {
			matrix shootmat = ViewMatrix.RTInverse;
			gameworld.FireRaycast((GameObject*)this, shootmat.pos + shootmat.look * 3, shootmat.look, 50.0f);
			ShootFlow = 0;
			recoilFlow = 0;
			int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Player, &ShootFlow, 8);
			gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
		}
	}

	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeltaMousePos, 8);
	gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
}

void Player::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();

	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);

	if (belowhit && belowDist < mesh.GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

BoundingOrientedBox Player::GetOBB()
{
	BoundingOrientedBox obb_local = mesh.GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

BoundingOrientedBox Player::GetBottomOBB(const BoundingOrientedBox& obb)
{
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

void Player::TakeDamage(float damage)
{
	HP -= damage;

	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP, 4);
	gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

	if (HP <= 0) {
		isExist = false;
	}
}

void Player::OnCollisionRayWithBullet()
{
	TakeDamage(2);
}

void World::Init() {
	clients.Init(32);
	gameObjects.Init(128);

	GameObjectType::STATICINIT();

#ifdef _DEBUG
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 128); // pos

	AddClientOffset(GameObjectType::_Player, 144, 160); // ShootFlow
	AddClientOffset(GameObjectType::_Player, 148, 164); // recoilFlow
	AddClientOffset(GameObjectType::_Player, 152, 168); // HP
	AddClientOffset(GameObjectType::_Player, 156, 172); // MaxHP
	AddClientOffset(GameObjectType::_Player, 160, 176); // DeltaMousePos

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
#else
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 128); // pos

	AddClientOffset(GameObjectType::_Player, 144, 160); // ShootFlow
	AddClientOffset(GameObjectType::_Player, 148, 164); // recoilFlow
	AddClientOffset(GameObjectType::_Player, 152, 168); // HP
	AddClientOffset(GameObjectType::_Player, 156, 172); // MaxHP
	AddClientOffset(GameObjectType::_Player, 160, 176); // DeltaMousePos

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
#endif
	

	//bulletRays.Init(32);

	Mesh* MyMesh = new Mesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 1, 1, 1 });
	Mesh::meshmap.insert(pair<string, Mesh*>("Player", MyMesh));
	Mesh::MeshNameArr.push_back("Player");

	Mesh* MyGroundMesh = new Mesh();
	MyGroundMesh->CreateWallMesh(40.0f, 0.5f, 40.0f);
	Mesh::meshmap.insert(pair<string, Mesh*>("Ground001", MyGroundMesh));
	Mesh::MeshNameArr.push_back("Ground001");

	Mesh* MyWallMesh = new Mesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f);
	Mesh::meshmap.insert(pair<string, Mesh*>("Wall001", MyWallMesh));
	Mesh::MeshNameArr.push_back("Wall001");

	/*BulletRay::mesh = new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);*/

	Mesh* MyMonsterMesh = new Mesh();
	MyMonsterMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 0, 0, 1 });
	Mesh::meshmap.insert(pair<string, Mesh*>("Monster001", MyMonsterMesh));
	Mesh::MeshNameArr.push_back("Monster001");

	int newindex = 0;
	int datacap = 0;

	/*Player* MyPlayer = new Player();
	MyPlayer->m_worldMatrix = (XMMatrixTranslation(5.0f, 5.0f, 3.0f));
	MyPlayer->MeshIndex = Mesh::GetMeshIndex("Player");
	newindex = NewObject(MyPlayer, GameObjectType::_Player);
	MyPlayer->mesh = *MyMesh;
	int datacap = Sending_SetMeshInGameObject(newindex, "Player");
	SendToAllClient(datacap);*/

	GameObject* groundObject = new GameObject();
	groundObject->MeshIndex = Mesh::GetMeshIndex("Ground001");
	groundObject->mesh = *MyGroundMesh;
	groundObject->m_worldMatrix = (XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	newindex = NewObject(groundObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Ground001");
	SendToAllClient(datacap);

	GameObject* wallObject = new GameObject();
	wallObject->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject->mesh = *(MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(0.0f, 1.0f, 5.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject2 = new GameObject();
	wallObject2->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject2->mesh = *(MyWallMesh);
	wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation(10.0f, 1.0f, 0.0f)));
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject3 = new GameObject();
	wallObject3->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject3->mesh = *(MyWallMesh);
	wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation(5.0f, 0.0f, -5.0f)));
	newindex = NewObject(wallObject3, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject4 = new GameObject();
	wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject4->mesh = *(MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation(-7.0f, -1.0f, 0)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject4 = new GameObject();
	wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject4->mesh = *(MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation(-10.0f, -1.0f, -10.0f)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject2->mesh = *(MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(-3.141592f / 4); 
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject2->mesh = *(MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(-20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject2->mesh = *(MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, 20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject = new GameObject();
	wallObject->MeshIndex = Mesh::GetMeshIndex("Wall001");
	wallObject->mesh = *(MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(-23.0f, 1.0f, 15.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	for (int i = 0; i < 1; ++i) {
		wallObject2 = new GameObject();
		wallObject2->MeshIndex = Mesh::GetMeshIndex("Wall001");
		wallObject2->mesh = *(MyWallMesh);
		wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation((float)(rand() % 80 - 40), 1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject2, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject3 = new GameObject();
		wallObject3->MeshIndex = Mesh::GetMeshIndex("Wall001");
		wallObject3->mesh = *(MyWallMesh);
		wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation((float)(rand() % 80 - 40), 0.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject3, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject4 = new GameObject();
		wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
		wallObject4->mesh = *(MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject4 = new GameObject();
		wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
		wallObject4->mesh = *(MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);
	}
	
	
	//matrix mat;
	//mat.Id();
	//for (int i = 0; i < 8; ++i) {
	//	int r = rand() % 3;
	//	float rot = 3.141592f / 4.0f;
	//	matrix trmat = XMMatrixRotationRollPitchYaw(rot * mat.mat.r[r].m128_f32[0], rot * mat.mat.r[r].m128_f32[1], rot * mat.mat.r[r].m128_f32[2]);
	//	GameObject* wallObject4 = new GameObject();
	//	wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
	//	wallObject4->mesh = *(MyWallMesh);
	//	wallObject4->m_worldMatrix = (XMMatrixMultiply(trmat, XMMatrixTranslation(rand() % 80 - 40, 0.0f, rand() % 80 - 40)));
	//	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	//	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	//	SendToAllClient(datacap);
	//}

	for (int i = 0; i < 20; ++i) {
		Monster* myMonster_1 = new Monster();
		myMonster_1->MeshIndex = Mesh::GetMeshIndex("Monster001");
		myMonster_1->mesh = *(MyMonsterMesh);
		myMonster_1->Init(XMMatrixTranslation(rand()%80 - 40, 10.0f, rand() % 80 - 40));
		newindex = NewObject(myMonster_1, GameObjectType::_Monster);
		datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
		SendToAllClient(datacap);
	}
	
	/*Monster* myMonster_2 = new Monster();
	myMonster_2->MeshIndex = Mesh::GetMeshIndex("Monster001");
	myMonster_2->mesh = *(MyMonsterMesh);
	myMonster_2->Init(XMMatrixTranslation(-5.0f, 0.5f, -2.5f));
	newindex = NewObject(myMonster_2, GameObjectType::_Monster);
	datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
	SendToAllClient(datacap);

	Monster* myMonster_3 = new Monster();
	myMonster_3->MeshIndex = Mesh::GetMeshIndex("Monster001");
	myMonster_3->mesh = *(MyMonsterMesh);
	myMonster_3->Init(XMMatrixTranslation(5.0f, 0.5f, -2.5f));
	newindex = NewObject(myMonster_3, GameObjectType::_Monster);
	datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
	SendToAllClient(datacap);*/

	cout << "Game Init end" << endl;
}

void World::Update() {
	lowFrequencyFlow += DeltaTime;
	midFrequencyFlow += DeltaTime;
	highFrequencyFlow += DeltaTime;

	for (currentIndex = 0; currentIndex < gameObjects.size; ++currentIndex) {
		if (gameObjects.isnull(currentIndex)) continue;
		if (gameObjects[currentIndex]->isExist == false) {
			continue;
		}
		gameObjects[currentIndex]->Update(DeltaTime);
	}

	// Collision......

	//bool bFixed = false;
	for (int i = 0; i < gameObjects.size; ++i) {
		if (gameObjects.isnull(i)) continue;
		GameObject* gbj1 = gameObjects[i];
		//if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) bFixed = true;

		float fsl1 = gbj1->mesh.MAXpos.fast_square_of_len3;
		vec4 lastpos1 = gbj1->m_worldMatrix.pos + gbj1->tickLVelocity;

		for (int j = i + 1; j < gameObjects.size; ++j) {
			if (gameObjects.isnull(j)) continue;
			GameObject* gbj2 = gameObjects[j];

			vec4 Ext2 = gbj2->mesh.MAXpos;
			vec4 dist = lastpos1 - (gbj2->m_worldMatrix.pos + gbj2->tickLVelocity);
			//if (gbj2->tickLVelocity.fast_square_of_len3 <= 0.001f && bFixed) continue;

			if (fsl1 + Ext2.fast_square_of_len3 > dist.fast_square_of_len3) {
				GameObject::CollisionMove(gbj1, gbj2);
			}
			//GameObject::CollisionMove(gbj1, gbj2);
		}

		//gbj1->tickLVelocity.w = 0;
		if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;
		gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
		if (fabsf(gbj1->m_worldMatrix.pos.x) > 40.0f || fabsf(gbj1->m_worldMatrix.pos.z) > 40.0f) {
			gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
		}
		gbj1->tickLVelocity = XMVectorZero();

		int datacap = Sending_ChangeGameObjectMember(i, gbj1, GameObjectType::_GameObject, &(gbj1->m_worldMatrix.pos), 16);
		SendToAllClient(datacap);
	}

	//for (int i = 0; i < clients.size; ++i) {
	//	if (clients.isnull(i)) continue;
	//	Player& p = *clients[i].pObjData;
	//	if (p.InputBuffer[(int)InputID::KeyboardW]) {
	//		clients[i].pos += XMVectorSet(0, 0, ClientData::MoveSpeed * DeltaTime, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardA]) {
	//		clients[i].pos += XMVectorSet(-ClientData::MoveSpeed * DeltaTime, 0, 0, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardS]) {
	//		clients[i].pos += XMVectorSet(0, 0, -ClientData::MoveSpeed * DeltaTime, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardD]) {
	//		clients[i].pos += XMVectorSet(ClientData::MoveSpeed * DeltaTime, 0, 0, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//}

	for (int i = 0; i < clients.size; ++i) {
		if (clients[i].isPosUpdate) {
			clients[i].socket.Send((char*)&clients[i].pos.m128_f32[0], 12);
			clients[i].isPosUpdate = false;
		}
	}

	if (lowHit()) {
		lowFrequencyFlow = 0;
	}
	if (midHit()) {
		midFrequencyFlow = 0;
	}
	if (highHit()) {
		highFrequencyFlow = 0;
	}
}

int World::Receiving(int clientIndex, char* rBuffer) {
	ClientData& client = clients[clientIndex];
	ui64 putv = 0;
	if (rBuffer[1] == 'D') {
		putv = 1;
	}
	else if (rBuffer[1] == 'U') {
		putv = 0;
	}

	if (rBuffer[0] != InputID::MouseMove) {
		client.pObjData->InputBuffer[rBuffer[0]] = putv;
		cout << "client " << clientIndex << " Input : " << rBuffer[0] << rBuffer[1] << endl;
		return 2;
	}
	else {
		int xdelta = *(int*)&rBuffer[1];
		int ydelta = *(int*)&rBuffer[5];

		client.pObjData->DeltaMousePos.x += xdelta;
		client.pObjData->DeltaMousePos.y += ydelta;

		if (client.pObjData->DeltaMousePos.y > 200) {
			client.pObjData->DeltaMousePos.y = 200;
		}
		if (client.pObjData->DeltaMousePos.y < -200) {
			client.pObjData->DeltaMousePos.y = -200;
		}

		return 9;
	}
}

int World::NewObject(GameObject* obj, GameObjectType gotype)
{
	int newindex = gameObjects.Alloc();
	gameObjects[newindex] = obj;
	obj->isExist = true;

	switch (gotype) {
	case GameObjectType::_GameObject:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send((char*)tempbuffer.data, datacap);
		}
		break;
	}
	case GameObjectType::_Player:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send((char*)tempbuffer.data, datacap);
		}

		char* member = (char*)obj + sizeof(GameObject);
		int len = 32;
		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, member, len);
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send((char*)tempbuffer.data, datacap);
		}
		break;
	}
	case GameObjectType::_Monster:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send((char*)tempbuffer.data, datacap);
		}
		break;
	}
	}
	return newindex;
}

int World::Sending_NewGameObject(int newindex, GameObject* newobj, GameObjectType gotype) {
	int datacap = GameObjectType::ServerSizeof[gotype] + 8;
	SendingType st = SendingType::NewGameObject;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &newindex, 4);
	memcpy(tempbuffer.data + 6, &gotype, 2);

	//char* cptr = tempbuffer.data + 8;
	//for (int i = 0; i < GameObjectType::Sizeof[gotype]; ++i) {
	//	cptr[i] = ((char*)newobj)[i];
	//}
	memcpy(tempbuffer.data + 8, (char*)newobj, GameObjectType::ServerSizeof[gotype]);
	return datacap;
}

int World::Sending_ChangeGameObjectMember(int objindex, GameObject* ptrobj, GameObjectType gotype, void* memberAddr, int memberSize) {
	int datacap = memberSize + 12; // sendingtype + gameobjecttype + clientMemberoffset + membersize = 2*4 = 8
	SendingType st = SendingType::ChangeMemberOfGameObject;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	memcpy(tempbuffer.data + 6, &gotype, 2);
	short serverOffset = (char*)memberAddr - (char*)ptrobj;
	
	short serverReleaseOffset = serverOffset;

//#ifdef _DEBUG
//	serverReleaseOffset -= 16;
//#endif

	short clientOffset = GameObjectType::GetClientOffset[type_offset_pair(gotype, serverReleaseOffset)];
	if (clientOffset == 0) clientOffset = serverOffset; // temp..

	memcpy(tempbuffer.data + 8, &clientOffset, 2);
	memcpy(tempbuffer.data + 10, &memberSize, 2);
	memcpy(tempbuffer.data + 12, memberAddr, memberSize);
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_NewRay(vec4 rayStart, vec4 rayDirection, float rayDistance) {
	SendingType st = SendingType::NewRay;
	int datacap = sizeof(float) * 7 + 2;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &rayStart.f3, 12);
	memcpy(tempbuffer.data + 14, &rayDirection.f3, 12);
	memcpy(tempbuffer.data + 26, &rayDistance, 4);
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_SetMeshInGameObject(int objindex, string str)
{
	SendingType st = SendingType::SetMeshInGameObject;
	int datacap = 10 + str.size();
	ui32 len = str.size();
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	memcpy(tempbuffer.data + 6, &len, 4);
	memcpy(tempbuffer.data + 10, &str[0], str.size());
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_AllocPlayerIndex(int clientindex, int objindex)
{
	SendingType st = SendingType::AllocPlayerIndexes;
	int datacap = 10;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data+2, &clientindex, 4);
	memcpy(tempbuffer.data+6, &objindex, 4);
	return datacap;
}

int World::Sending_DeleteGameObject(int objindex)
{
	SendingType st = SendingType::AllocPlayerIndexes;
	int datacap = 6;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	return datacap;
}

void World::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance)
{
	vec4 rayOrigin = rayStart;

	GameObject* closestHitObject = nullptr;

	float closestDistance = rayDistance;

	for (int i = 0; i < gameObjects.size; ++i) {
		if (gameObjects.isnull(i) || shooter == gameObjects[i]) continue;
		BoundingOrientedBox obb = gameObjects[i]->GetOBB();
		float distance;

		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
			if (distance <= rayDistance) {
				if (distance < closestDistance) {
					closestDistance = distance;
					closestHitObject = gameObjects[i];
					gameObjects[i]->OnCollisionRayWithBullet();
				}
			}
		}
	}

	int datacap = Sending_NewRay(rayStart, rayDirection, closestDistance);
	gameworld.SendToAllClient(datacap);
}

void GameObjectType::STATICINIT()
{
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<GameObject>(), GameObjectType::_GameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Player>(), GameObjectType::_Player));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Monster>(), GameObjectType::_Monster));
}

void GameObjectType::AddClientOffset_ptr(GameObjectType gotype, char* obj, char* member, int clientOffset)
{
	AddClientOffset(gotype, (short)(member - obj), (short)clientOffset);
}

void Monster::Update(float deltaTime)
{
	if (collideCount == 0) isGround = false;
	collideCount = 0;

	if (isGround == false) {
		LVelocity.y -= 9.81f * deltaTime;
	}
	tickLVelocity = LVelocity * deltaTime;

	vec4 monsterPos = m_worldMatrix.pos;
	monsterPos.w = 0;

	if (Target == nullptr || (Target != nullptr && *Target == nullptr)) {
		int limitseek = 4;
		if (gameworld.clients.size == 0) {
			Target = nullptr;
			return;
		}

		SEEK_TARGET_LOOP:
		for (int i = targetSeekIndex; i < gameworld.clients.size; ++i) {
			limitseek -= 1;
			if (limitseek == 0) {
				return;
			}
			if (gameworld.clients.isnull(i)) continue;
			Target = (Player**)& gameworld.gameObjects[gameworld.clients[i].objindex];
			break;
		}
		if (limitseek != 0 && (Target == nullptr || *Target == nullptr)) {
			targetSeekIndex = 0;
			goto SEEK_TARGET_LOOP;
		}
	}
	
	vec4 playerPos = (*Target)->m_worldMatrix.pos;
	playerPos.w = 0;

	vec4 toPlayer = playerPos - monsterPos;
	toPlayer.y = 0.0f;
	float distanceToPlayer = toPlayer.len3;

	if (distanceToPlayer <= m_chaseRange) {
		m_targetPos = playerPos;
		m_isMove = true;

		toPlayer.len3 = 1.0f;
		tickLVelocity += toPlayer * m_speed * deltaTime;
		m_worldMatrix.SetLook(toPlayer);

		if (distanceToPlayer < 2.0f) {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
		}

		m_fireTimer += deltaTime;
		if (m_fireTimer >= m_fireDelay) {
			m_fireTimer = 0.0f;

			vec4 rayStart = monsterPos + (m_worldMatrix.look * 0.5f);
			rayStart.y += 0.7f;
			vec4 rayDirection = playerPos - rayStart;

			float InverseAccurcy = distanceToPlayer / 6;
			rayDirection.x += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.y += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.z += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.len3 = 1.0f;

			gameworld.FireRaycast(this, rayStart, rayDirection, m_chaseRange);
		}
	}
	else {
		if (!m_isMove) {
			float randomAngle = ((float)rand() / RAND_MAX) * 2.0f * XM_PI;
			float randomRadius = ((float)rand() / RAND_MAX) * m_patrolRange;

			m_targetPos.x = m_homePos.x + randomRadius * cos(randomAngle);
			m_targetPos.z = m_homePos.z + randomRadius * sin(randomAngle);
			m_targetPos.y = m_homePos.y;

			m_isMove = true;
			m_patrolTimer = 0.0f;
		}

		m_patrolTimer += deltaTime;

		if (m_patrolTimer >= 5.0f) {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
			m_isMove = false;
			return;
		}

		vec4 currentPos = m_worldMatrix.pos;
		currentPos.w = 0;
		vec4 direction = m_targetPos - currentPos;
		direction.y = 0.0f;
		float distance = direction.len3;

		if (distance > 1.0f) {
			direction.len3 = 1.0f;
			tickLVelocity += direction * m_speed * deltaTime;
			m_worldMatrix.SetLook(direction);
		}
		else {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
			m_isMove = false;
		}
	}

	if (gameworld.lowHit()) {
		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &(m_worldMatrix), 64);
		gameworld.SendToAllClient(datacap);
	}
}

void Monster::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();
	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < mesh.GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnCollisionRayWithBullet()
{
	HP -= 10;
	if (HP < 0) {
		isExist = false;
		//Destory Object.
	}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	m_worldMatrix = (initialWorldMatrix);
	m_homePos = m_worldMatrix.pos;
}

BoundingOrientedBox Monster::GetOBB()
{
	BoundingOrientedBox obb_local = mesh.GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}
