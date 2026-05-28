/*!
@file EnemyBatchMovement.cpp
@brief 敵バッチの移動、分離、地形追従
敵同士の簡易分離はグリッドで近傍だけを見て軽量化する。
このファイルでは追跡、重力、坂や高台の地形解決もまとめて行う。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	long long EnemyBatchController::MakeCellKey(int x, int z) const
	{
		const unsigned long long ux = static_cast<unsigned int>(x);
		const unsigned long long uz = static_cast<unsigned int>(z);
		return static_cast<long long>((ux << 32) ^ uz);
	}

	void EnemyBatchController::BuildSpatialGrid()
	{
		m_CellMap.clear();
		if (m_CellSize <= 0.0f)
		{
			m_CellSize = 1.0f;
		}

		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			const auto& enemy = m_Enemies[i];
			if (!enemy.active || enemy.isDead)
			{
				continue;
			}

			const int cellX = static_cast<int>(floorf(enemy.position.x / m_CellSize));
			const int cellZ = static_cast<int>(floorf(enemy.position.z / m_CellSize));
			m_CellMap[MakeCellKey(cellX, cellZ)].push_back(i);
		}
	}

	Vec3 EnemyBatchController::CalculateSeparation(size_t index) const
	{
		if (index >= m_Enemies.size())
		{
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const Vec3 myPos = m_Enemies[index].position;
		const int cellX = static_cast<int>(floorf(myPos.x / m_CellSize));
		const int cellZ = static_cast<int>(floorf(myPos.z / m_CellSize));
		const float rangeSq = m_SeparationRange * m_SeparationRange;
		Vec3 steeringForce(0.0f, 0.0f, 0.0f);

		for (int z = -1; z <= 1; ++z)
		{
			for (int x = -1; x <= 1; ++x)
			{
				auto it = m_CellMap.find(MakeCellKey(cellX + x, cellZ + z));
				if (it == m_CellMap.end())
				{
					continue;
				}

				for (const auto otherIndex : it->second)
				{
					if (otherIndex == index || otherIndex >= m_Enemies.size())
					{
						continue;
					}

					Vec3 toAgent = myPos - m_Enemies[otherIndex].position;
					toAgent.y = 0.0f;
					const float distSq = bsmUtil::lengthSqr(toAgent);
					if (distSq <= 1e-6f || distSq > rangeSq)
					{
						continue;
					}

					steeringForce += toAgent * (1.0f / distSq);
				}
			}
		}

		return steeringForce;
	}

	void EnemyBatchController::RotateToVelocity(EnemyState& enemy, float lerpFact)
	{
		if (lerpFact <= 0.0f || bsmUtil::lengthSqr(enemy.velocity) <= 1e-6f)
		{
			return;
		}

		Vec3 direction = enemy.velocity;
		direction.y = 0.0f;
		if (bsmUtil::lengthSqr(direction) <= 1e-6f)
		{
			return;
		}

		direction.normalize();
		const float angle = atan2(direction.x, direction.z);
		Quat target;
		target.rotationRollPitchYawFromVector(Vec3(0.0f, angle, 0.0f));
		target.normalize();

		if (lerpFact >= 1.0f)
		{
			enemy.rotation = target;
		}
		else
		{
			enemy.rotation = XMQuaternionSlerp(enemy.rotation, target, lerpFact);
			enemy.rotation.normalize();
		}
	}

	bool EnemyBatchController::ResolveGeneratedGround(const GameStage& gameStage, EnemyState& enemy, double elapsedTime)
	{
		StageGroundResolveState groundState;
		groundState.position = enemy.position;
		groundState.previousPosition = enemy.previousPosition;
		groundState.gravityVelocity = enemy.gravityVelocity;
		groundState.footOffset = 0.35f;
		groundState.wasGrounded = enemy.isGround;
		groundState.elapsedTime = static_cast<float>(elapsedTime);

		bool terrainBlockedX = false;
		bool terrainBlockedZ = false;
		if (TrySlideAgainstGeneratedTerrainStep(gameStage, groundState, 0.75f, terrainBlockedX, terrainBlockedZ))
		{
			if (terrainBlockedX)
			{
				enemy.velocity.x = 0.0f;
				enemy.force.x = 0.0f;
				enemy.knockbackVelocity.x = 0.0f;
			}
			if (terrainBlockedZ)
			{
				enemy.velocity.z = 0.0f;
				enemy.force.z = 0.0f;
				enemy.knockbackVelocity.z = 0.0f;
			}
		}

		if (!TryResolveStageGround(gameStage, groundState))
		{
			const float baseFloorHalf = 32.5f;
			const bool insideBaseFloor = fabsf(groundState.position.x) <= baseFloorHalf &&
				fabsf(groundState.position.z) <= baseFloorHalf;
			if (!insideBaseFloor || !TryResolveGroundHeight(0.0f, groundState))
			{
				return false;
			}
		}

		enemy.position = groundState.position;
		enemy.gravityVelocity = groundState.gravityVelocity;
		enemy.isGround = groundState.isGrounded;
		return groundState.isGrounded;
	}

	void EnemyBatchController::OnUpdate(double elapsedTime)
	{
		if (m_Enemies.empty())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		Vec3 targetPosition(0.0f, 0.0f, 0.0f);
		auto player = GetStage()->GetSharedGameObject(L"Player", false);
		if (player)
		{
			auto playerTransform = player->GetComponent<Transform>(false);
			if (playerTransform)
			{
				targetPosition = playerTransform->GetWorldPosition();
			}
		}

		// 分離力は全敵の現在位置を使うため、各敵更新に入る前にまとめて計算しておく。
		m_SeparationForces.assign(m_Enemies.size(), Vec3(0.0f, 0.0f, 0.0f));
		BuildSpatialGrid();
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			if (m_Enemies[i].active && !m_Enemies[i].isDead)
			{
				m_SeparationForces[i] = CalculateSeparation(i);
			}
		}

		const float dt = static_cast<float>(elapsedTime);
		const Vec3 gravity(0.0f, -9.8f, 0.0f);
		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			auto& enemy = m_Enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			enemy.previousPosition = enemy.position;

			if (enemy.damageFlashTimer > 0.0)
			{
				enemy.damageFlashTimer -= elapsedTime;
				if (enemy.damageFlashTimer < 0.0)
				{
					enemy.damageFlashTimer = 0.0;
				}
			}

			if (enemy.knockbackControlTimer > 0.0)
			{
				enemy.knockbackControlTimer -= elapsedTime;
				if (enemy.knockbackControlTimer < 0.0)
				{
					enemy.knockbackControlTimer = 0.0;
				}
			}

			if (enemy.delayedDeathMinTimer > 0.0)
			{
				enemy.delayedDeathMinTimer -= elapsedTime;
				if (enemy.delayedDeathMinTimer < 0.0)
				{
					enemy.delayedDeathMinTimer = 0.0;
				}
			}

			const bool knockbackActive = enemy.knockbackControlTimer > 0.0
				|| (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f && !enemy.isGround);

			KillByFall(enemy);

			if (enemy.isDead)
			{
				ChangeAnimation(enemy, AnimState::Dead);
				enemy.gravityVelocity += gravity * dt;
				enemy.position.y += enemy.gravityVelocity.y * dt;
				if (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f)
				{
					enemy.position += enemy.knockbackVelocity * dt;
				}
				UpdateAnimation(enemy, elapsedTime);
				SyncProxyTransform(i);

				if (enemy.animationFinished)
				{
					RemoveEnemyProxy(i);
				}
				continue;
			}

			if (!enemy.isGround || knockbackActive)
			{
				ChangeAnimation(enemy, AnimState::Fall);
			}
			else
			{
				ChangeAnimation(enemy, AnimState::Sprint);
			}

			if (!knockbackActive)
			{
				enemy.steeringTimer -= elapsedTime;
				if (enemy.steeringTimer <= 0.0)
				{
					enemy.steeringTimer += enemy.steeringInterval;
					if (enemy.steeringTimer < 0.0)
					{
						enemy.steeringTimer = 0.0;
					}

					Vec3 toTarget = targetPosition - enemy.position;
					toTarget.y = 0.0f;
					Vec3 seekForce(0.0f, 0.0f, 0.0f);
					if (bsmUtil::lengthSqr(toTarget) > 1e-6f)
					{
						toTarget.normalize();
						seekForce = toTarget * (enemy.status.moveSpeed * m_MoveSpeedMultiplier) - enemy.velocity;
					}

					Vec3 separation = m_SeparationForces[i];
					separation.y = 0.0f;
					enemy.force = Vec3(0.0f, 0.0f, 0.0f);
					Steering::AccumulateForce(enemy.force, seekForce, 20.0f);
					Steering::AccumulateForce(enemy.force, separation, 20.0f);
				}

				enemy.velocity += enemy.force * dt;
				enemy.velocity.y = 0.0f;
				enemy.position += enemy.velocity * dt;
			}
			else
			{
				enemy.force = Vec3(0.0f, 0.0f, 0.0f);
				enemy.velocity *= 1.0f / (1.0f + 10.0f * dt);
			}

			if (bsmUtil::lengthSqr(enemy.knockbackVelocity) > 1e-4f)
			{
				if (enemy.isGround && enemy.knockbackControlTimer <= 0.0)
				{
					enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
				}
				else
				{
					enemy.position += enemy.knockbackVelocity * dt;
				}
			}

			enemy.gravityVelocity += gravity * dt;
			enemy.position.y += enemy.gravityVelocity.y * dt;
			enemy.gravityVelocity.x = 0.0f;
			enemy.gravityVelocity.z = 0.0f;

			const bool resolvedGeneratedGround = gameStage
				? ResolveGeneratedGround(*gameStage, enemy, elapsedTime)
				: false;

			if (enemy.delayDeathUntilLanding)
			{
				if (!resolvedGeneratedGround)
				{
					enemy.delayedDeathWasAirborne = true;
				}
				else if (enemy.delayedDeathWasAirborne && enemy.delayedDeathMinTimer <= 0.0)
				{
					// 爆弾で致死ダメージを受けた敵は、吹っ飛びが見えるよう着地してから死亡させる。
					KillEnemy(enemy);
					UpdateAnimation(enemy, elapsedTime);
					SyncProxyTransform(i);
					continue;
				}
			}

			UpdateAnimation(enemy, elapsedTime);
			RotateToVelocity(enemy, 0.35f);
			SyncProxyTransform(i);
			enemy.isGround = resolvedGeneratedGround;
		}
	}

	void EnemyBatchController::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		for (size_t i = 0; i < m_Enemies.size(); ++i)
		{
			auto& enemy = m_Enemies[i];
			if (!enemy.active)
			{
				continue;
			}

			auto proxy = enemy.proxy.lock();
			if (!proxy)
			{
				continue;
			}

			auto transform = proxy->GetComponent<Transform>(false);
			if (!transform)
			{
				continue;
			}

			enemy.position = transform->GetPosition();
			enemy.rotation = transform->GetQuaternion();
		}
	}
}

