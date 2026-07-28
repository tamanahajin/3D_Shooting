#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace {
		Quat MakeSpawnRotationTowardPlayer(const std::shared_ptr<GameStage>& gameStage, const Vec3& startPosition)
		{
			Quat rotation;
			if (!gameStage)
			{
				return rotation;
			}

			auto player = gameStage->GetSharedGameObject(L"Player", false);
			auto playerTransform = player ? player->GetComponent<Transform>(false) : nullptr;
			if (!playerTransform)
			{
				return rotation;
			}

			Vec3 toPlayer = playerTransform->GetWorldPosition() - startPosition;
			toPlayer.y = 0.0f;
			if (!bsmUtil::IsFiniteVec3(toPlayer) || bsmUtil::lengthSqr(toPlayer) <= 1e-6f)
			{
				return rotation;
			}

			rotation.facingY(toPlayer);
			return rotation;
		}
	}

	EnemyController::EnemyController(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyController::~EnemyController() {}

	void EnemyController::OnCreate()
	{
		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		m_gameStage = gameStage;
		SetDrawActive(false);
		SetShadowActive(false);
		if (gameStage)
		{
			gameStage->SetSharedGameObject(L"EnemyController", GetThis<GameObject>());
		}
		AddTag(L"EnemyController");
	}

	size_t EnemyController::AddEnemy(const Vec3& startPosition)
	{
		return AddEnemy(startPosition, EnemyStatus());
	}

	size_t EnemyController::AddEnemy(const Vec3& startPosition, const EnemyStatus& status)
	{
		size_t index = m_enemies.size();
		if (!m_freeEnemyIndices.empty())
		{
			index = m_freeEnemyIndices.back();
			m_freeEnemyIndices.pop_back();
		}
		else
		{
			m_enemies.emplace_back();
		}

		// 初期値を入れる
		EnemyState enemy;
		enemy.status = status;
		enemy.position = startPosition;
		enemy.previousPosition = startPosition;
		auto gameStage = m_gameStage.lock();
		if (!gameStage)
		{
			gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		}
		enemy.rotation = MakeSpawnRotationTowardPlayer(gameStage, startPosition);
		enemy.lifeState = EnemyLifeState::Spawning;
		enemy.spawnIntroTimer = enemy.spawnIntroDuration;
		enemy.steeringTimer = static_cast<double>(index & 3) * 0.0125;
		enemy.steeringInterval = status.steeringInterval;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = status.maxHp > 0 ? status.maxHp : 1;
		enemy.hp = enemy.maxHp;

		m_enemies[index] = enemy;

		// 当たり判定だけは軽量なGameObjectとして残し、描画やAI状態はm_Enemies側でまとめて扱う。
		// 生成スパイクを抑えるため、死亡済みプロキシがあれば再利用する。
		auto proxy = AcquireCollisionProxy(index, startPosition, enemy.status);
		m_enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
	}

	void EnemyController::PrewarmCollisionProxyPool(int count)
	{
		auto gameStage = m_gameStage.lock();
		if (count <= 0 || !gameStage)
		{
			return;
		}

		const int missingCount = count - static_cast<int>(m_collisionProxyPool.size());
		if (missingCount <= 0)
		{
			return;
		}

		// Wave開始フレームにEnemyCollisionProxyの生成が集中しないよう、先に非アクティブ状態で作っておく。
		const Vec3 pooledPosition(0.0f, -1000.0f, 0.0f);
		const EnemyStatus defaultStatus;
		for (int i = 0; i < missingCount; ++i)
		{
			auto proxy = gameStage->AddGameObject<EnemyCollisionProxy>(
				GetThis<EnemyController>(),
				0,
				pooledPosition,
				defaultStatus);
			ReleaseCollisionProxy(proxy);
		}
	}

	void EnemyController::SetMoveSpeedMultiplier(float multiplier)
	{
		m_moveSpeedMultiplier = bsmUtil::Max(0.1f, multiplier);
	}

	void EnemyController::SyncProxyTransform(size_t index)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto proxy = m_enemies[index].proxy.lock();
		if (!proxy)
		{
			return;
		}

		auto transform = proxy->GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		transform->SetPosition(m_enemies[index].position);
		transform->SetQuaternion(m_enemies[index].rotation);
		transform->SetScale(m_enemies[index].status.modelScale);
	}

	std::shared_ptr<EnemyCollisionProxy> EnemyController::AcquireCollisionProxy(
		size_t index,
		const Vec3& startPosition,
		const EnemyStatus& status)
	{
		// Wave開始時のコンポーネント生成コストを抑えるため、プールに戻したプロキシを優先する。
		while (!m_collisionProxyPool.empty())
		{
			auto proxy = m_collisionProxyPool.back();
			m_collisionProxyPool.pop_back();
			if (!proxy)
			{
				continue;
			}

			proxy->ResetForEnemy(GetThis<EnemyController>(), index, startPosition, status);
			return proxy;
		}

		auto gameStage = m_gameStage.lock();
		if (!gameStage)
		{
			return nullptr;
		}

		return gameStage->AddGameObject<EnemyCollisionProxy>(
			GetThis<EnemyController>(),
			index,
			startPosition,
			status);
	}

	void EnemyController::ReleaseCollisionProxy(const std::shared_ptr<EnemyCollisionProxy>& proxy)
	{
		if (!proxy)
		{
			return;
		}

		proxy->DeactivateForPool();
		m_collisionProxyPool.push_back(proxy);
	}

	void EnemyController::RemoveEnemyProxy(size_t index)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto& enemy = m_enemies[index];
		if (!enemy.active)
		{
			// 二重返却すると同じスロットが複数回再利用候補に入ってしまう。
			return;
		}

		auto proxy = enemy.proxy.lock();
		if (proxy)
		{
			ReleaseCollisionProxy(proxy);
		}

		enemy.active = false;
		enemy.proxy.reset();
		m_freeEnemyIndices.push_back(index);
	}

	bool EnemyController::IsEnemyAlive(size_t index) const
	{
		if (index >= m_enemies.size())
		{
			return false;
		}

		const auto& enemy = m_enemies[index];
		return enemy.active && enemy.lifeState != EnemyLifeState::Deading && enemy.hp > 0;
	}

	bool EnemyController::CanDamagePlayer(size_t index) const
	{
		if (index >= m_enemies.size())
		{
			return false;
		}

		const auto& enemy = m_enemies[index];
		return enemy.active &&
			enemy.lifeState == EnemyLifeState::Alive &&
			enemy.hp > 0 &&
			!IsKnockbackActive(enemy);
	}

	int EnemyController::GetContactDamage(size_t index) const
	{
		if (index >= m_enemies.size())
		{
			return 0;
		}

		const auto& enemy = m_enemies[index];
		return enemy.active ? enemy.status.contactDamage : 0;
	}

	int EnemyController::GetAliveEnemyCount() const
	{
		int count = 0;
		for (const auto& enemy : m_enemies)
		{
			if (enemy.active && enemy.lifeState != EnemyLifeState::Deading && enemy.hp > 0)
			{
				++count;
			}
		}
		return count;
	}

	int EnemyController::GetTotalEnemyCount() const
	{
		return static_cast<int>(m_enemies.size());
	}
}


