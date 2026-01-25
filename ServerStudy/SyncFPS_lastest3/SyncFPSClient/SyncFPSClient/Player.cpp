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
	////Interpolation
	float pow = deltaTime * 5;
	game.DeltaMousePos = (1.0f - pow) * game.DeltaMousePos + pow * DeltaMousePos;

	/*if (m_worldMatrix.pos.fast_square_of_len3 > 10000) {
		m_worldMatrix.pos = vec4(0, 10, 0, 1);
		LVelocity = 0;
	}*/

	// 권총 반동 로직
	/*ShootFlow += deltaTime;
	if (ShootFlow >= ShootDelay) ShootFlow = ShootDelay;
	float shootrate = powf(ShootFlow / ShootDelay, 5);
	gunMatrix_firstPersonView.pos.z = 0.3f + 0.2f * shootrate;
	constexpr float RotHeight = ShootDelay * 10;

	if (0 <= shootrate && shootrate <= 1) {
		gunMatrix_firstPersonView.LookAt(vec4(0, RotHeight - RotHeight * shootrate, 5) - gunMatrix_firstPersonView.pos);
	}*/
	// 머신건 반동 로직
	ShootFlow += deltaTime;
	if (ShootFlow >= ShootDelay) ShootFlow = ShootDelay;

	float shootrate = powf(ShootFlow / ShootDelay, 3);

	float recoilAmount = 1.0f - shootrate;

	gunMatrix_firstPersonView.pos.z = 0.3f + 0.01f * recoilAmount;

	// 머신건 회전 로직
	gunBarrelSpeed = 100.0f;

	float t = ShootFlow / ShootDelay;
	if (t < 1.0f) {
		float spinRate = powf(1.0f - t, 2);
		gunBarrelAngle += gunBarrelSpeed * spinRate * deltaTime;
		if (gunBarrelAngle > XM_2PI) gunBarrelAngle -= XM_2PI;
	}

	UpdateGunBarrelNodes();

	/*recoilFlow += deltaTime;
	if (recoilFlow < recoilDelay) {
		float power = 5;
		float delta_rate = (power / recoilDelay) * pow(1 - recoilFlow / recoilDelay, (power - 1));
		float f = recoilVelocity * deltaTime * delta_rate;
		DeltaMousePos.y -= f;
	}*/

	//if (collideCount == 0) isGround = false;
	//collideCount = 0;
	//
	///*if (isGround) {
	//	float distance_from_lastplane = CurrentPosPlane.getPlaneDistance(m_worldMatrix.pos);
	//	if (distance_from_lastplane > 0.1f) {
	//		dbglog3(L"out plane %g %g %g \n", CurrentPosPlane.x, CurrentPosPlane.y, CurrentPosPlane.z);
	//		isGround = false;
	//	}
	//}*/

	//if (isGround == false) {
	//	LVelocity.y -= 9.81f * deltaTime;
	//}

	//XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	//constexpr float speed = 3.0f;

	//if (isGround) {
	//	if (m_pKeyBuffer[VK_SPACE] & 0xF0) {
	//		LVelocity.y = JumpVelocity;
	//		isGround = false;
	//	}
	//}
	//tickLVelocity = LVelocity * deltaTime;

	//if (m_pKeyBuffer['W'] & 0xF0) {
	//	tickLVelocity += speed * m_worldMatrix.look * game.DeltaTime;
	//}
	//if (m_pKeyBuffer['S'] & 0xF0) {
	//	tickLVelocity -= speed * m_worldMatrix.look * game.DeltaTime;
	//}
	//if (m_pKeyBuffer['A'] & 0xF0) {
	//	tickLVelocity -= speed * m_worldMatrix.right * game.DeltaTime;
	//}
	//if (m_pKeyBuffer['D'] & 0xF0) {
	//	tickLVelocity += speed * m_worldMatrix.right * game.DeltaTime;
	//}
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
	if (game.bFirstPersonVision)
	{
		/*((Shader*)game.MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);

		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCBResource.resource->GetGPUVirtualAddress());

		matrix gunmat = gunMatrix_firstPersonView;
		gunmat *= viewmat;
		gunmat.transpose();

		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &gunmat, 0);

		game.MyDiffuseTextureShader->SetTextureCommand(&game.GunTexture);

		Gun->Render(gd.pCommandList, 1);*/
		if (GunModel) {
			matrix gunmat = gunMatrix_firstPersonView;

			gunmat *= XMMatrixScaling(0.5f, 0.5f, 0.5f);
			const float PI = 3.141592f;
			//gunmat *= XMMatrixRotationY(PI);

			gunmat.pos.y -= 0.40f;
			gunmat.pos.x += 0.35f;
			gunmat.pos.z += 0.90f;

			gunmat *= viewmat;
			//gunmat.transpose(); 

			//GunModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderNormal);
			if (Game::renderViewPort == &game.MySpotLight.viewport) {
				//shadowMapping
				if (GunModel) {
					GunModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderShadowMap);
					return;
				}
			}
			else {
				if (GunModel) {
					GunModel->Render(gd.pCommandList, gunmat, Shader::RegisterEnum::RenderWithShadow);
				}
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
	if (!GunModel) return;

	if (gunBarrelNodeIndices.empty()) return;

	matrix rot = XMMatrixRotationZ(gunBarrelAngle);

	for (int idx : gunBarrelNodeIndices) {
		GunModel->Nodes[idx].transform = rot * GunModel->BindPose[idx];
	}
}
