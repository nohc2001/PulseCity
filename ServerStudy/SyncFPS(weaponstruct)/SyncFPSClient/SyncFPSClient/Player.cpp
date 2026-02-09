#include "stdafx.h"
#include "Player.h"
#include "Render.h"
#include "Shader.h"
#include "Mesh.h"
#include "Game.h"

void Player::Update(float deltaTime)
{
	PositionInterpolation(deltaTime);
}

void Player::ClientUpdate(float deltaTime)
{
	if ((uintptr_t)m_pWeapon > 0x00007FFFFFFFFFFF) {
		m_pWeapon = nullptr;
	}

	if (m_pWeapon == nullptr || (int)m_pWeapon->m_info.type != m_currentWeaponType)
	{
		if (m_pWeapon != nullptr) {
			delete m_pWeapon;
			m_pWeapon = nullptr;
		}

		m_pWeapon = new Weapon((WeaponType)m_currentWeaponType);

	}

	if (m_pWeapon == nullptr) return;

	m_pWeapon->Update(deltaTime);

	if (game.player == this) {
		float recoilT = m_pWeapon->GetRecoilAlpha();
		if (recoilT > 0.7f) {
			float f = m_pWeapon->m_info.recoilVelocity * 10.0f * (recoilT - 0.7f) * deltaTime;
			game.DeltaMousePos.y -= f;
		}
	}

	gunMatrix_firstPersonView.Id();

	WeaponType currentType = m_pWeapon->m_info.type;

	switch (currentType)
	{
	case WeaponType::Pistol:
	{
		// --- 권총 로직 ---
		float shootrate = powf(m_pWeapon->m_shootFlow / m_pWeapon->m_info.shootDelay, 5);
		if (shootrate > 1.0f) shootrate = 1.0f;

		gunMatrix_firstPersonView.pos.z = 0.3f + 0.2f * shootrate;
		constexpr float RotHeight = 1.5f * 10;

		if (0 <= shootrate && shootrate <= 1) {
			gunMatrix_firstPersonView.LookAt(vec4(0, RotHeight - RotHeight * shootrate, 5) - gunMatrix_firstPersonView.pos);
		}
	}
	break;

	case WeaponType::MachineGun:
	{
		// --- 머신건 로직 ---
		float shootrate = powf(m_pWeapon->m_shootFlow / m_pWeapon->m_info.shootDelay, 3);
		if (shootrate > 1.0f) shootrate = 1.0f;

		float recoilAmount = 1.0f - shootrate;
		gunMatrix_firstPersonView.pos.z = 0.3f + 0.02f * recoilAmount;

		// 회전 로직
		gunBarrelSpeed = 150.0f;
		if (shootrate < 1.0f) {
			float spinRate = powf(1.0f - shootrate, 2);
			gunBarrelAngle += gunBarrelSpeed * spinRate * deltaTime;
			if (gunBarrelAngle > XM_2PI) gunBarrelAngle -= XM_2PI;
		}
		UpdateGunBarrelNodes();
	}
	break;

	case WeaponType::Sniper:
	{
		// --- 스나이퍼 로직 ---
		float recoilT = m_pWeapon->GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.70f) {
			float stageT = (recoilT - 0.70f) / 0.30f;
			float smoothT = sinf(stageT * XM_PIDIV2);
			pitchAngle = m_pWeapon->m_info.recoilVelocity * smoothT;
			zOffset -= 0.2f * smoothT;
		}
		else if (recoilT > 0.60f) {
			
		}
		else {
			float stageT = recoilT / 0.60f;
			float wobble = sinf(stageT * XM_PI * 2.0f) * 0.08f;
			zOffset += wobble * stageT;
		}

		XMMATRIX rotMat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle));
		XMMATRIX transMat = XMMatrixTranslation(0.0f, 0.0f, zOffset);
		gunMatrix_firstPersonView.mat = rotMat * transMat;
	}
	break;

	
	}

	if (m_pWeapon->m_shootFlow < 0) {
		float reloadProgress = 1.0f - (abs(m_pWeapon->m_shootFlow) / m_pWeapon->m_info.reloadTime);
		float drop = sinf(reloadProgress * XM_PI) * 0.5f;

		XMMATRIX dropMat = XMMatrixTranslation(0, -drop, 0);
		gunMatrix_firstPersonView.mat = gunMatrix_firstPersonView.mat * dropMat;
	}
}

void Player::Render()
{
	if (game.player == this) {
		if (game.bFirstPersonVision == false) {
			GameObject::Render();

			matrix gunmat = gunMatrix_thirdPersonView;

			const float PI = 3.141592f;
			gunmat *= XMMatrixRotationY(PI);

			gunmat.pos.y -= 0.40f;
			gunmat.pos.x += 0.55f;
			gunmat.pos.z += 0.80f;

			gunmat *= m_worldMatrix;
			//gunmat.LookAt(m_worldMatrix.look);
			//gunmat.transpose();

			//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &gunmat, 0);
			//Gun->Render(gd.pCommandList, 1);
			if (Game::renderViewPort == &game.MySpotLight.viewport) {
				//shadowMapping
				if (GunModel) {
					GunModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderShadowMap);
				}
			}
			else {
				if (GunModel) {
					GunModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderWithShadow);
				}
			}
			
		}
	}
	else {
		GameObject::Render();
	}
}

void Player::Render_AfterDepthClear()
{
	matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;

	struct Model* pTargetModel = nullptr;

	if (game.bFirstPersonVision)
	{
		switch ((WeaponType)m_currentWeaponType)
		{
		case WeaponType::Sniper:     pTargetModel = game.SniperModel; break;
		case WeaponType::MachineGun: pTargetModel = game.MachineGunModel; break;
		//case WeaponType::Pistol:     pTargetModel = game.GunModel; break; 
		}

		if (pTargetModel) {
			matrix gunmat = gunMatrix_firstPersonView;

			switch ((WeaponType)m_currentWeaponType)
			{
			case WeaponType::MachineGun:
				gunmat *= XMMatrixScaling(0.5f, 0.5f, 0.5f);
				gunmat.pos.y -= 0.40f;
				gunmat.pos.x += 0.35f;
				gunmat.pos.z += 0.90f;
				break;
			case WeaponType::Sniper:
				gunmat *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
				gunmat.pos.y -= 0.40f;
				gunmat.pos.x += 0.60f;
				gunmat.pos.z += 1.90f;
				break;
			}

			gunmat *= viewmat;

			if (Game::renderViewPort == &game.MySpotLight.viewport) {
				pTargetModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderShadowMap);
				return; 
			}
			else {
				pTargetModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderWithShadow);
			}
		}
	}

	Shader* shader = (Shader*)game.MyOnlyColorShader;
	shader->Add_RegisterShaderCommand(gd.pCommandList);

	matrix vmat = gd.viewportArr[0].ViewMatrix;
	vmat *= gd.viewportArr[0].ProjectMatrix;
	vmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &vmat, 0);

	matrix spmat = viewmat;
	spmat.pos += spmat.look * 10;
	spmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &spmat, 0);
	ShootPointMesh->Render(gd.pCommandList, 1);

	//HP = (ShootFlow / ShootDelay) * MaxHP;
	matrix hpmat;
	hpmat.pos.x = -6;
	hpmat.pos.y = 2;
	hpmat.pos.z = 6;
	hpmat.LookAt(vec4(1, 0, 0));
	hpmat.look *= 2 * HP / MaxHP;
	hpmat *= viewmat;
	hpmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &hpmat, 0);
	HPBarMesh->Render(gd.pCommandList, 1);

	//Heat Bar
	matrix heatmat;
	heatmat.pos.x = -5;
	heatmat.pos.y = 1.5f;
	heatmat.pos.z = 5;
	heatmat.LookAt(vec4(1, 0, 0));
	heatmat.look *= 2 * HeatGauge / 200;
	heatmat *= viewmat;
	heatmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &heatmat, 0);
	HeatBarMesh->Render(gd.pCommandList, 1);
}

void Player::UpdateGunBarrelNodes()
{
	if ((WeaponType)m_currentWeaponType != WeaponType::MachineGun) return;
	if (!game.MachineGunModel || game.MG_BarrelIndices.empty()) return;

	matrix rot = XMMatrixRotationZ(gunBarrelAngle);

	for (int idx : game.MG_BarrelIndices) {
		game.MachineGunModel->Nodes[idx].transform = rot * game.MachineGunModel->BindPose[idx];
	}
}
