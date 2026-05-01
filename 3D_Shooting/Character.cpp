/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	SkyDome::SkyDome(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	SkyDome::~SkyDome() {}

	void SkyDome::OnCreate()
	{
		SetShadowActive(false);
		SetAlphaActive(false);

		auto ptrDraw = AddComponent<SkyDomeDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetRadius(450.0f);

		AddTag(L"Sky");
	}

	//--------------------------------------------------------------------------------------
	// フロアオブジェクト
	//--------------------------------------------------------------------------------------
	Floor::Floor(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix)
	{
		m_transParam = param;
	}
	Floor::~Floor() {}

	void Floor::OnCreate()
	{
		AddTag(L"Floor");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		ptrDraw->AddBaseModelMesh(meshes);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(
				m_MaterialPrefix + std::to_wstring(i)
			);
		}

		ptrDraw->SetOwnShadowActive(false);
	}

	FloorInstancedRenderer::FloorInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix),
		m_InstanceWorlds(instanceWorlds)
	{
	}

	FloorInstancedRenderer::~FloorInstancedRenderer() {}

	void FloorInstancedRenderer::OnCreate()
	{
		auto ptrDraw = AddComponent<InstancedStaticDraw>();

		ptrDraw->SetMeshKey(m_MeshKey);
		ptrDraw->SetMaterialPrefix(m_MaterialPrefix);
		ptrDraw->SetInstanceWorlds(m_InstanceWorlds);
		ptrDraw->SetBaseColorOverride(Col4(0.627f, 0.659f, 0.788f, 1.0f));
		ptrDraw->SetUseMaterialTexture(false);
		ptrDraw->SetLightingEnabled(true);
		ptrDraw->SetOwnShadowActive(false);
		ptrDraw->BuildInstanceBuffer();

		AddTag(L"Floor");
	}

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
		const Vec3& startPosition) :
		GameObject(stage),
		m_Controller(controller),
		m_EnemyIndex(enemyIndex),
		m_StartPosition(startPosition)
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
		transform->SetScale(0.01f, 0.01f, 0.01f);
		transform->SetRotation(0.0f, 0.0f, 0.0f);

		auto collision = AddComponent<CollisionCapsule>();
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(0.2f);
		collision->SetMakedHeight(0.3f);
		collision->AddExcludeCollisionTag(L"Enemy");

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
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
		EnemyState enemy;
		enemy.position = startPosition;
		enemy.rotation = Quat();
		enemy.steeringTimer = static_cast<double>(m_Enemies.size() & 3) * 0.0125;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = 20;
		enemy.hp = enemy.maxHp;

		const size_t index = m_Enemies.size();
		m_Enemies.push_back(enemy);

		auto proxy = GetStage()->AddGameObject<EnemyCollisionProxy>(GetThis<EnemyBatchController>(), index, startPosition);
		m_Enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
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
			damagePosition.y += 0.35f;
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
		transform->SetScale(m_ModelScale);
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
						seekForce = toTarget * 5.0f - enemy.velocity;
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

			UpdateAnimation(enemy, elapsedTime);
			RotateToVelocity(enemy, 0.35f);
			SyncProxyTransform(i);
			enemy.isGround = false;
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
		StartDamageFlash(enemy, 0.2);

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

		if (pair.m_SrcHitNormal.y > 0.7f)
		{
			enemy.isGround = true;
			if (enemy.gravityVelocity.y < 0.0f)
			{
				enemy.gravityVelocity.y = 0.0f;
			}
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
				m_ModelScale,
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
	}	FloorCollision::FloorCollision(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const Vec3& collisionSize) :
		GameObject(stage),
		m_CollisionSize(collisionSize)
	{
		m_transParam = param;
	}

	FloorCollision::~FloorCollision() {}

	void FloorCollision::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_CollisionSize.x,
			m_CollisionSize.y,
			m_CollisionSize.z);
		ptrColl->SetFixed(true);

		AddTag(L"Floor");
	}

	//--------------------------------------------------------------------------------------
	// ボックスオブジェクト
	//--------------------------------------------------------------------------------------
	FixedBox::FixedBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage)
	{
		m_transParam = param;
	}
	FixedBox::~FixedBox() {}

	void FixedBox::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetFixed(true);
		//タグをつける
		AddTag(L"FixedBox");
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	//--------------------------------------------------------------------------------------
	// 四角のオブジェクト
	//--------------------------------------------------------------------------------------
	WallBox::WallBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage),
		m_totalTime(0.0)
	{
		m_transParam = param;
	}
	WallBox::~WallBox() {}

	void WallBox::OnCreate()
	{
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		//重力をつける
		auto ptrGra = AddComponent<Gravity>();

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	void WallBox::OnUpdate(double elapsedTime)
	{
		//Transformコンポーネントを取り出す
		auto ptrTrans = GetComponent<Transform>();
		auto& param = ptrTrans->GetTransParam();

		m_totalTime += elapsedTime;
		if (m_totalTime >= XM_2PI)
		{
			m_totalTime = 0.0;
		}
		param.position.x = (float)sin(m_totalTime) * 2.0f;
	}


	//--------------------------------------------------------------------------------------
	//	追いかける配置オブジェクト
	//--------------------------------------------------------------------------------------
	//構築と破棄
	SeekObject::SeekObject(const std::shared_ptr<Stage>& StagePtr, const Vec3& startPos) :
		GameObject(StagePtr),
		m_StartPos(startPos),
		m_StateChangeSize(5.0f),
		m_Force(0),
		m_Velocity(0)
	{
		m_transParam.position = startPos;
	}
	SeekObject::~SeekObject() {}

	void SeekObject::StartDamageFlash(double duration)
	{
		if (duration <= 0.0)
		{
			duration = 0.001;
		}

		m_DamageFlashDuration = duration;
		m_DamageFlashTimer = duration;
	}

	float SeekObject::GetDamageFlashValue() const
	{
		if (m_DamageFlashDuration <= 0.0 || m_DamageFlashTimer <= 0.0)
		{
			return 0.0f;
		}

		double value = m_DamageFlashTimer / m_DamageFlashDuration;
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

	Vec3 SeekObject::GetDamageNumberPosition()
	{
		auto transform = GetComponent<Transform>(false);
		Vec3 position = transform ? transform->GetPosition() : m_transParam.position;
		position.y += 0.35f;
		return position;
	}

	void SeekObject::ShowDamageNumber(const DamageInfo& info)
	{
		if (info.m_Damage <= 0)
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			gameStage->SpawnDamageNumber(GetDamageNumberPosition(), info.m_Damage);
		}
	}

	//初期化
	void SeekObject::OnCreate()
	{
		auto ptrTransform = GetComponent<Transform>();
		ptrTransform->SetPosition(m_StartPos);
		ptrTransform->SetScale(0.01f, 0.01f, 0.01f);
		ptrTransform->SetRotation(0.0f, 0.0f, 0.0f);
		SetBatchUpdateManaged(true);

		//オブジェクトのグループを得る
		auto group = GetStage()->GetSharedObjectGroup(L"SeekGroup");
		group->IntoGroup(GetThis<SeekObject>());
		
		// コリジョン
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		const float radius = 0.2f;
		const float segmentHeight = 0.3f;
		ptrColl->SetMakedRadius(radius);
		ptrColl->SetMakedHeight(segmentHeight);

		auto ptrGra = AddComponent<Gravity>();
		//分離行動をつける
		auto PtrSep = GetBehavior<SeparationSteering>();
		PtrSep->SetGameObjectGroup(group);
		
		// 描画は EnemyInstancedRenderer でまとめて行う

		// アニメーション
		auto anim = GetBehavior<AnimationStateBehavior>();
		anim->SetFallbackMeshKey(L"ENEMY_MODEL_SKINNED");
		anim->ChangeAnimation(AnimState::Idle);
		//透明処理をする
		SetAlphaActive(false);
		AddTag(L"Enemy");

		// HP設定
		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		hp->m_OnDamaged = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
		};

		hp->m_OnDeath = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
			self->m_IsDead = true;
			self->m_DeathAnimFinished = false;

			auto anim = self->GetBehavior<AnimationStateBehavior>();
			anim->ChangeAnimation(AnimState::Dead);
		};

		// ステートマシン
		m_StateMachine.reset(new StateMachine<SeekObject>(GetThis<SeekObject>()));
		m_StateMachine->ChangeState(SeekFarState::Instance());

		m_SteeringUpdateTimer = (double)((reinterpret_cast<std::uintptr_t>(this) & 3)) * 0.0125;
	}


	void SeekObject::ApplyGravity(double elapsedTime)
	{
		auto grav = GetComponent<Gravity>(false);
		auto transform = GetComponent<Transform>(false);
		if (!grav || !transform)
		{
			return;
		}

		const float dt = static_cast<float>(elapsedTime);
		auto gravityVelocity = grav->GetGravityVelocity();
		gravityVelocity += grav->GetGravity() * dt;
		grav->SetGravityVelocity(gravityVelocity);

		auto position = transform->GetPosition();
		position += gravityVelocity * dt;
		transform->SetPosition(position);
	}

	void SeekObject::RotateToVelocity(float lerpFact)
	{
		if (lerpFact <= 0.0f || bsmUtil::lengthSqr(m_Velocity) <= 1e-6f)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		Vec3 direction = m_Velocity;
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

		Quat current = transform->GetQuaternion();
		if (lerpFact >= 1.0f)
		{
			current = target;
		}
		else
		{
			current = XMQuaternionSlerp(current, target, lerpFact);
		}
		transform->SetQuaternion(current);
	}

	void SeekObject::UpdateBatched(double elapsedTime, const Vec3& targetPosition, const Vec3& separationForce)
	{
		if (m_DamageFlashTimer > 0.0)
		{
			m_DamageFlashTimer -= elapsedTime;
			if (m_DamageFlashTimer < 0.0)
			{
				m_DamageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_IsDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}

			ApplyGravity(elapsedTime);
			anim->OnUpdate(elapsedTime);

			if (anim->IsFinished())
			{
				m_DeathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_IsGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

		m_SteeringUpdateTimer -= elapsedTime;
		if (m_SteeringUpdateTimer <= 0.0)
		{
			m_SteeringUpdateTimer += m_SteeringUpdateInterval;
			if (m_SteeringUpdateTimer < 0.0)
			{
				m_SteeringUpdateTimer = 0.0;
			}

			auto transform = GetComponent<Transform>(false);
			Vec3 position = transform ? transform->GetPosition() : m_StartPos;
			Vec3 toTarget = targetPosition - position;
			toTarget.y = 0.0f;

			Vec3 seekForce(0.0f, 0.0f, 0.0f);
			if (bsmUtil::lengthSqr(toTarget) > 1e-6f)
			{
				toTarget.normalize();
				seekForce = toTarget * 5.0f - m_Velocity;
			}

			Vec3 separation = separationForce;
			separation.y = 0.0f;
			m_Force = Vec3(0.0f, 0.0f, 0.0f);
			Steering::AccumulateForce(m_Force, seekForce, 20.0f);
			Steering::AccumulateForce(m_Force, separation, 20.0f);
		}

		ApplyForce(elapsedTime);
		ApplyGravity(elapsedTime);
		anim->OnUpdate(elapsedTime);
		RotateToVelocity(0.35f);

		m_IsGround = false;
	}

	//操作
	void SeekObject::OnUpdate(double elapsedTime)
	{
		if (IsBatchUpdateManaged())
		{
			return;
		}
		if (m_DamageFlashTimer > 0.0)
		{
			m_DamageFlashTimer -= elapsedTime;
			if (m_DamageFlashTimer < 0.0)
			{
				m_DamageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_IsDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}
			else if (anim->IsFinished())
			{
				m_DeathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_IsGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

		m_SteeringUpdateTimer -= elapsedTime;

		// 操舵計算は20Hzだけ
		if (m_SteeringUpdateTimer <= 0.0)
		{
			m_SteeringUpdateTimer += m_SteeringUpdateInterval;

			m_Force = Vec3(0);
			m_StateMachine->Update(); // この中で SetForce / ApplyForce される
		}
		else
		{
			// 前回の force を使って移動だけ継続
			ApplyForce();
		}

		// 向き更新は velocity ベース
		if (bsmUtil::lengthSqr(m_Velocity) > 1e-6f)
		{
			auto ptrUtil = GetBehavior<UtilBehavior>();
			ptrUtil->RotToHead(m_Velocity, 0.35f);
		}

		// 地面判定をリセット
		m_IsGround = false;
	}

	void SeekObject::OnCollisionEnter(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
	}

	void SeekObject::OnCollisionExecute(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
	}

	void SeekObject::CheckGroundCollision(const CollisionPair& pair)
	{
		// 衝突法線のY成分をチェック（上向きの法線 = 地面との衝突）
		// 0.7f は約45度（cos(45°) ? 0.707）
		// これより大きい = より水平に近い面 = 地面とみなす
		if (pair.m_SrcHitNormal.y > 0.7f)
		{
			m_IsGround = true;

			// 重力速度をリセット（地面に着地）
			auto grav = GetComponent<Gravity>();
			auto gravVel = grav->GetGravityVelocity();

			// 下向きの速度の場合のみリセット（着地時）
			if (gravVel.y < 0.0f)
			{
				grav->SetGravityVelocity(Vec3(gravVel.x, 0.0f, gravVel.z));
			}
		}
	}

	Vec3 SeekObject::GetTargetPos()const
	{
		auto ptrTarget = GetStage()->GetSharedGameObject(L"Player");
		return ptrTarget->GetComponent<Transform>()->GetWorldPosition();
	}


	void SeekObject::ApplyForce()
	{
		ApplyForce(Scene::GetElapsedTime());
	}

	void SeekObject::ApplyForce(double elapsedTime)
	{
		const float dt = static_cast<float>(elapsedTime);
		m_Velocity += m_Force * dt;
		m_Velocity.y = 0.0f;
		auto ptrTrans = GetComponent<Transform>();
		auto pos = ptrTrans->GetPosition();
		pos += m_Velocity * dt;
		ptrTrans->SetPosition(pos);
	}



	//--------------------------------------------------------------------------------------
	//	プレイヤーから遠いときの移動
	//--------------------------------------------------------------------------------------
	std::shared_ptr<SeekFarState> SeekFarState::Instance()
	{
		static std::shared_ptr<SeekFarState> instance(new SeekFarState);
		return instance;
	}
	void SeekFarState::Enter(const std::shared_ptr<SeekObject>& Obj)
	{
	}
	void SeekFarState::Execute(const std::shared_ptr<SeekObject>& Obj)
	{
		auto ptrSeek = Obj->GetBehavior<SeekSteering>();
		auto ptrSep = Obj->GetBehavior<SeparationSteering>();
		auto force = Obj->GetForce();
		force = ptrSeek->Execute(force, Obj->GetVelocity(), Obj->GetTargetPos());
		force += ptrSep->Execute(force);
		Obj->SetForce(force);
		Obj->ApplyForce();
		float f = bsm::bsmUtil::length(Obj->GetComponent<Transform>()->GetPosition() - Obj->GetTargetPos());
		if (f < Obj->GetStateChangeSize())
		{
			//Obj->GetStateMachine()->ChangeState(SeekNearState::Instance());
		}
	}

	void SeekFarState::Exit(const std::shared_ptr<SeekObject>& Obj)
	{
	}

	//--------------------------------------------------------------------------------------
	//	プレイヤーから近いときの移動
	//--------------------------------------------------------------------------------------
	std::shared_ptr<SeekNearState> SeekNearState::Instance()
	{
		static std::shared_ptr<SeekNearState> instance(new SeekNearState);
		return instance;
	}
	void SeekNearState::Enter(const std::shared_ptr<SeekObject>& Obj)
	{
	}
	void SeekNearState::Execute(const std::shared_ptr<SeekObject>& Obj)
	{
		auto ptrArrive = Obj->GetBehavior<ArriveSteering>();
		auto ptrSep = Obj->GetBehavior<SeparationSteering>();
		auto force = Obj->GetForce();
		force = ptrArrive->Execute(force, Obj->GetVelocity(), Obj->GetTargetPos());
		force += ptrSep->Execute(force);
		Obj->SetForce(force);
		Obj->ApplyForce();
		float f = bsm::bsmUtil::length(Obj->GetComponent<Transform>()->GetPosition() - Obj->GetTargetPos());
		if (f >= Obj->GetStateChangeSize())
		{
			Obj->GetStateMachine()->ChangeState(SeekFarState::Instance());
		}
	}
	void SeekNearState::Exit(const std::shared_ptr<SeekObject>& Obj)
	{
	}
}
