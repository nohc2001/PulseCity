#include "stdafx.h"
#include "main.h"
#include "Player.h"
#include "GameObjectTypes.h"

void Player::Update(float deltaTime)
{
	if (m_pWeapon) m_pWeapon->Update(deltaTime);

	vec4 currentPos = m_worldMatrix.pos;
	XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(0, m_yaw, 0);
	m_worldMatrix.mat = rotMat;
	m_worldMatrix.pos = currentPos;

	if (collideCount == 0) isGround = false;
	collideCount = 0;

	if (isGround == false) {
		LVelocity.y -= 9.8f * deltaTime;
	}

	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	constexpr float speed = 6.0f;

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

	if (HealSkillCooldownFlow >= 0.0f) {
		HealSkillCooldownFlow -= deltaTime;
		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HealSkillCooldownFlow, sizeof(HealSkillCooldownFlow));
		gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
	}

	if (InputBuffer[InputID::KeyboardQ] == true) {
		std::cout << "[Player::Update] Q pressed!  HP=" << HP
			<< " Heat=" << HeatGauge
			<< " CD=" << HealSkillCooldownFlow << std::endl;

		if (HealSkillCooldownFlow <= 0.0f && HeatGauge > 0.0f && HP < MaxHP) {

			// 회복량 HeatGauge 전부를 HP로 전환
			float healAmount = HeatGauge;
			HP += healAmount;

			if (HP > MaxHP) HP = MaxHP;

			// 게이지 소모
			HeatGauge = 0.0f;

			// 쿨타임 리셋
			HealSkillCooldownFlow = HealSkillCooldown;

			// HP, HeatGauge 서버→클라 동기화
			int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP, sizeof(HP));
			gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

			datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge, sizeof(HeatGauge));
			gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
		}
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

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
	vec4 clook = { 0, 0, 1, 0 };
	clook = XMVector3Rotate(clook, quaternion);

	vec4 peye = m_worldMatrix.pos;
	vec4 pat = m_worldMatrix.pos;

	if (bFirstPersonVision) {
		peye += 1.0f * m_worldMatrix.up;
		pat += 1.0f * m_worldMatrix.up;
		pat += 10.0f * clook;
	}
	else {
		peye -= 3.0f * clook;
		pat += 10.0f * clook;
		peye += 0.5f * m_worldMatrix.up;
		peye += 0.5f * m_worldMatrix.right;
	}

	XMVECTOR vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	ViewMatrix = XMMatrixLookAtLH(peye, pat, vUp);

	if (InputBuffer[InputID::MouseLbutton] == true) {
		if (m_pWeapon->m_shootFlow >= m_pWeapon->m_info.shootDelay && bullets > 0 && HeatGauge <= MaxHeatGauge) {

			bullets -= 1;
			HeatGauge += 2;

			m_pWeapon->OnFire();
			
			vec4 shootOrigin = m_worldMatrix.pos + vec4(0, 1.0f, 0, 0);
			gameworld.FireRaycast((GameObject*)this, shootOrigin + clook * 1.5f, clook, 50.0f, m_pWeapon->m_info.damage);

			int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets, 4);
			gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

			datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge, 4);
			gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

			int fireCap = gameworld.Sending_PlayerFire(gameworld.clients[clientIndex].objindex);
			gameworld.SendToAllClient(fireCap);

			if (bullets <= 0) {
				m_pWeapon->m_shootFlow = -m_pWeapon->m_info.reloadTime;

				bullets = m_pWeapon->m_info.maxBullets;

				int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets, 4);
				gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
			}
		}
	}

	//int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeltaMousePos, 8);
	//gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

	BoundingOrientedBox obb = GetOBB();
	for (int i = 0; i < gameworld.DropedItems.size; ++i) {
		if (gameworld.DropedItems.isnull(i)) continue;
		vec4 p = gameworld.DropedItems[i].pos;
		p.w = 0;
		if (obb.Contains(p)) {
			bool isexist = false;
			bool beat = false;
			int firstBlackIndex = -1;
			for (int k = 0; k < Inventory.size(); ++k) {
				if (Inventory[k].id == 0 && firstBlackIndex == -1) {
					firstBlackIndex = k;
				}
				if (Inventory[k].id == gameworld.DropedItems[i].itemDrop.id)
				{
					Inventory[k].ItemCount += gameworld.DropedItems[i].itemDrop.ItemCount;
					isexist = true;
					beat = true;
					firstBlackIndex = k;
					break;
				}
			}
			if (isexist == false && firstBlackIndex != -1) {
				Inventory[firstBlackIndex] = gameworld.DropedItems[i].itemDrop;
				beat = true;
			}

			if (beat) {
				int datacap = gameworld.Sending_InventoryItemSync(Inventory[firstBlackIndex], firstBlackIndex);
				gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
				gameworld.DropedItems.Free(i);
				datacap = gameworld.Sending_ItemRemove(i);
				gameworld.SendToAllClient(datacap);
				break;
			}
		}
	}

	//히트 게이지 지속 감소 (발사 안할시)

	if (HeatGauge > 0.0f) {
		float decayRate = 5.0f; // 초당 감소량
		HeatGauge -= decayRate * deltaTime;
		if (HeatGauge < 0.0f) HeatGauge = 0.0f;
	}

	static float heatSendTimer = 0.0f;
	heatSendTimer += deltaTime;

	if (heatSendTimer > 0.2f) { // 0.2초마다 서버 → 클라 전송
		heatSendTimer = 0.0f;

		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge, sizeof(HeatGauge));

		gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
	}

}

void Player::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();

	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);

	if (belowhit && belowDist < Shape::IndexToShapeMap[ShapeIndex].GetMesh()->GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Player::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;

	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);

	if (belowhit && belowDist < Shape::IndexToShapeMap[ShapeIndex].GetMesh()->GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

BoundingOrientedBox Player::GetOBB()
{
	BoundingOrientedBox obb_local = Shape::IndexToShapeMap[ShapeIndex].GetMesh()->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

void Player::TakeDamage(float damage)
{
	HP -= damage;

	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP, 4);
	gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

	if (HP <= 0) {
		isExist = false;

		DeathCount += 1;
		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeathCount, 4);
		gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

		Respawn();
	}
}

void Player::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	TakeDamage(damage);
}

void Player::Respawn() {
	HP = 100;
	isExist = true;

	m_worldMatrix.Id();
	m_worldMatrix.pos.y = 2;
	//player position send

	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &isExist, 1);
	gameworld.SendToAllClient(datacap);

	datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP, 4);
	gameworld.SendToAllClient(datacap);
}