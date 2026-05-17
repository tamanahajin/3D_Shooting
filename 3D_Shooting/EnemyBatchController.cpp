/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {


	EnemyInstancedRenderer::EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	void EnemyInstancedRenderer::OnCreate()
	{
		m_Draw = AddComponent<InstancedSkinnedDraw>();
		m_Draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_Draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_Draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	void EnemyInstancedRenderer::OnUpdate2(double elapsedTime)
	{
		if (!m_Draw)
		{
			return;
		}

		auto controllerObject = GetStage()->GetSharedGameObject(L"EnemyBatchController", false);
		auto controller = std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
		if (controller)
		{
			controller->FillInstanceSources(m_InstanceSources, m_ModelOffset);
		}
		else
		{
			m_InstanceSources.clear();
		}

		m_Draw->SetInstances(m_InstanceSources);
		m_Draw->BuildInstanceBuffer();
	}

	EnemyCollisionProxy::EnemyCollisionProxy(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyBatchController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status) :
		GameObject(stage),
		m_Controller(controller),
		m_EnemyIndex(enemyIndex),
		m_StartPosition(startPosition),
		m_ModelScale(status.modelScale),
		m_CollisionRadius(status.collisionRadius),
		m_CollisionHeight(status.collisionHeight)
	{
		m_transParam.position = startPosition;
	}

	EnemyCollisionProxy::~EnemyCollisionProxy() {}

	void EnemyCollisionProxy::OnCreate()
	{
		SetBatchUpdateManaged(true);
		SetDrawActive(false);
		SetShadowActive(false);

		auto transform = GetComponent<Transform>();
		transform->SetPosition(m_StartPosition);
		transform->SetScale(m_ModelScale);
		transform->SetRotation(0.0f, 0.0f, 0.0f);

		auto collision = AddComponent<CollisionCapsule>();
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(m_CollisionRadius);
		collision->SetMakedHeight(m_CollisionHeight);
		collision->AddExcludeCollisionTag(L"Enemy");
		collision->AddExcludeCollisionTag(L"Floor");

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	void EnemyCollisionProxy::HandleCollision(const CollisionPair& pair)
	{
		auto otherCollision = pair.m_Dest.lock();
		if (!otherCollision)
		{
			return;
		}

		auto otherObject = otherCollision->GetGameObject();
		if (!otherObject)
		{
			return;
		}

		if (otherObject->FindTag(L"Floor"))
		{
			auto controller = m_Controller.lock();
			if (controller)
			{
				controller->NotifyGroundCollision(m_EnemyIndex, pair);
			}
			return;
		}

		if (!otherObject->FindTag(L"Bullet") && !otherObject->FindTag(L"Bomb"))
		{
			return;
		}

		auto bulletBase = std::dynamic_pointer_cast<IBullet>(otherObject);
		if (bulletBase && !bulletBase->IsActive())
		{
			return;
		}

		if (auto bomb = std::dynamic_pointer_cast<BombBullet>(otherObject))
		{
			CollisionPair swapped = pair;
			swapped.m_Src = pair.m_Dest;
			swapped.m_Dest = pair.m_Src;
			bomb->OnCollisionEnter(swapped);
			return;
		}

		auto damageDealer = otherObject->GetComponent<DamageDealer>(false);
		if (!damageDealer)
		{
			return;
		}

		DamageInfo info;
		info.m_Damage = damageDealer->GetDamage();
		info.m_Instigator = otherObject;
		ApplyDamage(info);

		if (damageDealer->DestroyOnHit() && bulletBase)
		{
			bulletBase->SetActive(false);
		}
	}

	void EnemyCollisionProxy::OnCollisionEnter(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	void EnemyCollisionProxy::OnCollisionExecute(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	bool EnemyCollisionProxy::ApplyDamage(const DamageInfo& info)
	{
		auto controller = m_Controller.lock();
		return controller ? controller->ApplyDamage(m_EnemyIndex, info) : false;
	}

	void EnemyCollisionProxy::AddKnockback(const Vec3& velocity)
	{
		auto controller = m_Controller.lock();
		if (controller)
		{
			controller->AddKnockback(m_EnemyIndex, velocity);
		}
	}

	bool EnemyCollisionProxy::IsAlive() const
	{
		auto controller = m_Controller.lock();
		return controller ? controller->IsEnemyAlive(m_EnemyIndex) : false;
	}

	EnemyBatchController::EnemyBatchController(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyBatchController::~EnemyBatchController() {}

	void EnemyBatchController::OnCreate()
	{
		SetDrawActive(false);
		SetShadowActive(false);
		GetStage()->SetSharedGameObject(L"EnemyBatchController", GetThis<GameObject>());
		AddTag(L"EnemyBatchController");
	}

	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition)
	{
		return AddEnemy(startPosition, EnemyStatus());
	}

	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition, const EnemyStatus& status)
	{
		EnemyState enemy;
		enemy.status = status;
		enemy.position = startPosition;
		enemy.previousPosition = startPosition;
		enemy.rotation = Quat();
		enemy.steeringTimer = static_cast<double>(m_Enemies.size() & 3) * 0.0125;
		enemy.steeringInterval = status.steeringInterval;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = status.maxHp > 0 ? status.maxHp : 1;
		enemy.hp = enemy.maxHp;

		const size_t index = m_Enemies.size();
		m_Enemies.push_back(enemy);

		auto proxy = GetStage()->AddGameObject<EnemyCollisionProxy>(GetThis<EnemyBatchController>(), index, startPosition, enemy.status);
		m_Enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
	}

	void EnemyBatchController::SetMoveSpeedMultiplier(float multiplier)
	{
		m_MoveSpeedMultiplier = bsmUtil::Max(0.1f, multiplier);
	}

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

	bool EnemyBatchController::IsOneShotState(AnimState state) const
	{
		switch (state)
		{
		case AnimState::Dead:
		case AnimState::AttackMeleeLeft:
		case AnimState::AttackMeleeRight:
			return true;
		default:
			return false;
		}
	}

	bool EnemyBatchController::IsHoldLastFrameState(AnimState state) const
	{
		switch (state)
		{
		case AnimState::Dead:
		case AnimState::AttackMeleeLeft:
		case AnimState::AttackMeleeRight:
			return true;
		default:
			return false;
		}
	}

	double EnemyBatchController::GetAnimationDurationSeconds(AnimState state) const
	{
		auto mesh = BaseScene::Get()->GetMesh(L"ENEMY_MODEL_SKINNED");
		if (!mesh)
		{
			return 0.0;
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return 0.0;
		}

		const unsigned int index = static_cast<unsigned int>(state);
		if (index >= static_cast<unsigned int>(assimp->GetAnimationCount()))
		{
			return 0.0;
		}

		return static_cast<double>(assimp->GetAnimationDurationSeconds(index));
	}

	double EnemyBatchController::GetHoldTimeSeconds(double duration) const
	{
		return bsmUtil::Max(0.0, duration - (1.0 / 30.0));
	}

	void EnemyBatchController::ChangeAnimation(EnemyState& enemy, AnimState state, bool forceRestart)
	{
		if (!forceRestart && enemy.animationState == state)
		{
			return;
		}

		enemy.animationState = state;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
	}

	void EnemyBatchController::UpdateAnimation(EnemyState& enemy, double elapsedTime)
	{
		if (IsOneShotState(enemy.animationState))
		{
			double duration = GetAnimationDurationSeconds(enemy.animationState);
			if (duration <= 0.0)
			{
				duration = 0.6;
			}

			if (!enemy.animationFinished)
			{
				enemy.animationTime += elapsedTime;
				if (enemy.animationTime >= duration)
				{
					enemy.animationFinished = true;
					enemy.animationTime = IsHoldLastFrameState(enemy.animationState)
						? GetHoldTimeSeconds(duration)
						: duration;
				}
			}
			else if (IsHoldLastFrameState(enemy.animationState))
			{
				enemy.animationTime = GetHoldTimeSeconds(duration);
			}

			return;
		}

		enemy.animationTime += elapsedTime;
	}

	float EnemyBatchController::GetDamageFlashValue(const EnemyState& enemy) const
	{
		if (enemy.damageFlashDuration <= 0.0 || enemy.damageFlashTimer <= 0.0)
		{
			return 0.0f;
		}

		double value = enemy.damageFlashTimer / enemy.damageFlashDuration;
		if (value < 0.0)
		{
			value = 0.0;
		}
		else if (value > 1.0)
		{
			value = 1.0;
		}
		return static_cast<float>(value);
	}

	void EnemyBatchController::StartDamageFlash(EnemyState& enemy, double duration)
	{
		if (duration <= 0.0)
		{
			duration = 0.001;
		}
		enemy.damageFlashDuration = duration;
		enemy.damageFlashTimer = duration;
	}

	void EnemyBatchController::KillByFall(EnemyState& enemy)
	{
		if (enemy.isDead || enemy.position.y >= kFallDeathY)
		{
			return;
		}

		enemy.hp = 0;
		enemy.isDead = true;
		enemy.deathAnimFinished = false;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.velocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
		enemy.knockbackControlTimer = 0.0;
		ChangeAnimation(enemy, AnimState::Dead, true);
	}

	void EnemyBatchController::ShowDamageNumber(size_t index, const DamageInfo& info)
	{
		if (info.m_Damage <= 0 || index >= m_Enemies.size())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			Vec3 damagePosition = m_Enemies[index].position;
			damagePosition.y += m_Enemies[index].status.damageNumberOffsetY;
			gameStage->SpawnDamageNumber(damagePosition, info.m_Damage);
		}
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

		const float maxEnemyStepUp = 0.75f;
		auto isBlockedByTerrainHeight = [&](const Vec3& candidatePosition)
		{
			float targetGroundY = 0.0f;
			if (!gameStage.TryGetSlopeGroundHeight(candidatePosition, targetGroundY))
			{
				return false;
			}

			float previousGroundY = 0.0f;
			gameStage.TryGetSlopeGroundHeight(groundState.previousPosition, previousGroundY);

			const float previousFeetY = groundState.previousPosition.y - groundState.footOffset;
			const float candidateFeetY = candidatePosition.y - groundState.footOffset;
			const bool comingFromLowerGround = targetGroundY > previousGroundY + maxEnemyStepUp;
			const bool feetAreBelowTarget = previousFeetY < targetGroundY - maxEnemyStepUp &&
				candidateFeetY < targetGroundY - maxEnemyStepUp;
			return comingFromLowerGround && feetAreBelowTarget;
		};

		if (isBlockedByTerrainHeight(groundState.position))
		{
			Vec3 slideX = groundState.position;
			slideX.z = groundState.previousPosition.z;
			Vec3 slideZ = groundState.position;
			slideZ.x = groundState.previousPosition.x;

			if (!isBlockedByTerrainHeight(slideX))
			{
				groundState.position = slideX;
				enemy.velocity.z = 0.0f;
				enemy.knockbackVelocity.z = 0.0f;
			}
			else if (!isBlockedByTerrainHeight(slideZ))
			{
				groundState.position = slideZ;
				enemy.velocity.x = 0.0f;
				enemy.knockbackVelocity.x = 0.0f;
			}
			else
			{
				groundState.position.x = groundState.previousPosition.x;
				groundState.position.z = groundState.previousPosition.z;
				enemy.velocity.x = 0.0f;
				enemy.velocity.z = 0.0f;
				enemy.force.x = 0.0f;
				enemy.force.z = 0.0f;
				enemy.knockbackVelocity.x = 0.0f;
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

	void EnemyBatchController::SyncProxyTransform(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (!proxy)
		{
			return;
		}

		auto transform = proxy->GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		transform->SetPosition(m_Enemies[index].position);
		transform->SetQuaternion(m_Enemies[index].rotation);
		transform->SetScale(m_Enemies[index].status.modelScale);
	}

	void EnemyBatchController::RemoveEnemyProxy(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (proxy)
		{
			proxy->RemoveTag(L"Enemy");
			proxy->SetDrawActive(false);
			proxy->SetUpdateActive(false);
			GetStage()->RemoveGameObject(proxy);
		}

		m_Enemies[index].active = false;
		m_Enemies[index].deathAnimFinished = true;
		m_Enemies[index].proxy.reset();
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

			UpdateAnimation(enemy, elapsedTime);
			RotateToVelocity(enemy, 0.35f);
			SyncProxyTransform(i);
			enemy.isGround = resolvedGeneratedGround;
		}
	}

	void EnemyBatchController::OnUpdate2(double elapsedTime)
	{
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

	bool EnemyBatchController::ApplyDamage(size_t index, const DamageInfo& info)
	{
		if (index >= m_Enemies.size())
		{
			return false;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active || enemy.isDead || info.m_Damage <= 0)
		{
			return false;
		}

		ShowDamageNumber(index, info);
		StartDamageFlash(enemy, enemy.status.damageFlashDuration);

		enemy.hp -= bsmUtil::Clamp(info.m_Damage, 0, info.m_Damage);
		if (enemy.hp <= 0)
		{
			enemy.hp = 0;
			enemy.isDead = true;
			enemy.deathAnimFinished = false;
			ChangeAnimation(enemy, AnimState::Dead, true);
			return true;
		}

		return false;
	}

	void EnemyBatchController::AddKnockback(size_t index, const Vec3& velocity)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active || enemy.isDead)
		{
			return;
		}

		enemy.knockbackVelocity = Vec3(velocity.x, 0.0f, velocity.z);
		enemy.gravityVelocity.y = bsmUtil::Max(enemy.gravityVelocity.y, velocity.y);
		enemy.gravityVelocity.x = 0.0f;
		enemy.gravityVelocity.z = 0.0f;
		enemy.velocity *= 0.2f;
		enemy.force = Vec3(0.0f, 0.0f, 0.0f);
		enemy.isGround = false;
		enemy.knockbackControlTimer = 0.45;
	}

	void EnemyBatchController::NotifyGroundCollision(size_t index, const CollisionPair& pair)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto& enemy = m_Enemies[index];
		if (!enemy.active)
		{
			return;
		}

		bool isGrounded = enemy.isGround;
		Vec3 gravityVelocity = enemy.gravityVelocity;
		if (!TryApplyGroundCollision(pair, gravityVelocity, isGrounded))
		{
			return;
		}

		enemy.isGround = isGrounded;
		enemy.gravityVelocity = gravityVelocity;
		enemy.gravityVelocity.x = 0.0f;
		enemy.gravityVelocity.z = 0.0f;

		auto proxy = enemy.proxy.lock();
		if (proxy)
		{
			auto transform = proxy->GetComponent<Transform>(false);
			if (transform)
			{
				enemy.position = transform->GetPosition();
			}
		}
	}

	bool EnemyBatchController::IsEnemyAlive(size_t index) const
	{
		if (index >= m_Enemies.size())
		{
			return false;
		}

		const auto& enemy = m_Enemies[index];
		return enemy.active && !enemy.isDead && enemy.hp > 0;
	}

	int EnemyBatchController::GetAliveEnemyCount() const
	{
		int count = 0;
		for (const auto& enemy : m_Enemies)
		{
			if (enemy.active && !enemy.isDead && enemy.hp > 0)
			{
				++count;
			}
		}
		return count;
	}

	int EnemyBatchController::GetTotalEnemyCount() const
	{
		return static_cast<int>(m_Enemies.size());
	}

	void EnemyBatchController::FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const
	{
		outSources.clear();
		outSources.reserve(m_Enemies.size());

		for (const auto& enemy : m_Enemies)
		{
			if (!enemy.active)
			{
				continue;
			}

			Mat4x4 world;
			world.affineTransformation(
				enemy.status.modelScale,
				Vec3(0.0f, 0.0f, 0.0f),
				enemy.rotation,
				enemy.position + modelOffset);

			SkinnedInstanceSource source{};
			source.world = world;
			source.animationIndex = static_cast<unsigned int>(enemy.animationState);
			source.animationTime = static_cast<float>(enemy.animationTime);
			source.damage = GetDamageFlashValue(enemy);
			outSources.push_back(source);
		}
	}
}
