#include "stdafx.h"
#include "WaveController.h"

namespace shooting {

	WaveController::WaveController(const std::shared_ptr<EnemyController>& controller) :
		WaveController()
	{
		SetController(controller);
	}

	void WaveController::SetController(const std::shared_ptr<EnemyController>& controller)
	{
		m_controller = controller;
		PrepareEnemySpawner(controller);
	}

	void WaveController::SetEnemySpawnPositionResolver(
		const std::shared_ptr<EnemySpawnPositionResolver>& resolver)
	{
		m_enemySpawnPositionResolver = resolver;
		if (m_enemySpawner)
		{
			m_enemySpawner->SetSpawnPositionResolver(resolver);
		}
	}

	bool WaveController::IsValid() const
	{
		return !m_controller.expired() && m_enemySpawner && m_enemySpawner->IsValid();
	}

	void WaveController::SetEnemyStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
		if (m_enemySpawner)
		{
			m_enemySpawner->SetStatus(kind, status);
		}
	}

	EnemyStatus WaveController::GetEnemyStatus(EnemyKind kind) const
	{
		auto it = m_statusByKind.find(kind);
		if (it != m_statusByKind.end())
		{
			return it->second;
		}

		auto defaultIt = m_statusByKind.find(EnemyKind::Default);
		if (defaultIt != m_statusByKind.end())
		{
			return defaultIt->second;
		}

		return EnemyStatus();
	}

	EnemyStatus WaveController::GetEnemyStatusForWave(EnemyKind kind, int wave) const
	{
		EnemyStatus status = GetEnemyStatus(kind);
		if (wave <= 1 || m_settings.addMaxHpPerWave <= 0)
		{
			return status;
		}

		status.maxHp = GetEnemyMaxHpForWave(status.maxHp, wave);
		return status;
	}

	void WaveController::Update(double elapsedTime, const Vec3& spawnCenter)
	{
		ProcessPendingEnemySpawns();
		if (HasPendingEnemySpawns())
		{
			// 生成キューが残っている間は次ウェーブのタイマーを進めない。
			// 敵数を大きくしたベンチ時に、生成待ちのまま次ウェーブまで重なるのを避ける。
			return;
		}

		if (m_currentWave <= 0 || m_settings.intervalSeconds <= 0.0)
		{
			return;
		}

		m_waveTimer -= elapsedTime;
		if (m_waveTimer <= 0.0)
		{
			StartNextWave(spawnCenter);
		}
	}

	void WaveController::StartNextWave(const Vec3& spawnCenter)
	{
		auto controller = GetController();
		if (!controller)
		{
			return;
		}

		PrepareEnemySpawner(controller);
		if (!m_enemySpawner)
		{
			return;
		}

		++m_currentWave;
		m_waveTimer = m_settings.intervalSeconds;

		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.kind = EnemyKind::Default;
		spawnDesc.count = GetEnemyCountForWave(m_currentWave);
		spawnDesc.center = spawnCenter;
		spawnDesc.settings.minDistance = m_settings.spawnMinDistance;
		spawnDesc.settings.maxDistance = m_settings.spawnMaxDistance;
		spawnDesc.settings.spawnY = spawnCenter.y;
		spawnDesc.settings.minSpacing = m_settings.minSpawnSpacing;
		spawnDesc.settings.maxAttempts = m_settings.maxSpawnAttempts;
		spawnDesc.overrideStatus = true;
		spawnDesc.status = GetEnemyStatusForWave(spawnDesc.kind, m_currentWave);

		controller->SetMoveSpeedMultiplier(GetAppliedEnemySpeedMultiplierForWave(m_currentWave));
		QueueEnemy(spawnDesc);
		ProcessPendingEnemySpawns();
	}

	void WaveController::SetNextWaveNumber(int wave)
	{
		m_currentWave = wave > 1 ? wave - 1 : 0;
		m_waveTimer = 0.0;
	}

	int WaveController::CreateEnemy(
		const Vec3& center,
		int count,
		const EnemyFactory::SpawnSettings& settings,
		EnemyKind kind)
	{
		auto controller = GetController();
		if (!controller || count <= 0)
		{
			return 0;
		}

		PrepareEnemySpawner(controller);
		if (!m_enemySpawner)
		{
			return 0;
		}

		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.kind = kind;
		spawnDesc.count = count;
		spawnDesc.center = center;
		spawnDesc.settings = settings;

		const int createdCount = m_enemySpawner->CreateEnemiesAround(spawnDesc);
		m_totalEnemyCount += createdCount;
		return createdCount;
	}

	std::shared_ptr<EnemyController> WaveController::GetController() const
	{
		return m_controller.lock();
	}

	void WaveController::PrepareEnemySpawner(const std::shared_ptr<EnemyController>& controller)
	{
		if (!controller)
		{
			return;
		}

		if (!m_enemySpawner)
		{
			m_enemySpawner = std::make_shared<EnemySpawner>(controller);
		}
		else
		{
			m_enemySpawner->SetController(controller);
		}
		m_enemySpawner->SetSpawnPositionResolver(m_enemySpawnPositionResolver.lock());

		for (const auto& statusByKind : m_statusByKind)
		{
			m_enemySpawner->SetStatus(statusByKind.first, statusByKind.second);
		}
	}

	void WaveController::QueueEnemy(const EnemyFactory::SpawnBatchDesc& desc)
	{
		if (!m_enemySpawner || desc.count <= 0)
		{
			return;
		}

		m_enemySpawner->QueueEnemies(desc);
	}

	void WaveController::ProcessPendingEnemySpawns()
	{
		if (!m_enemySpawner)
		{
			return;
		}

		m_totalEnemyCount += m_enemySpawner->ProcessPendingSpawns(GetEnemySpawnPerFrame());
	}

	bool WaveController::HasPendingEnemySpawns() const
	{
		return m_enemySpawner && m_enemySpawner->HasPendingSpawns();
	}

	int WaveController::GetEnemySpawnPerFrame() const
	{
		const int spawnPerFrame = GameDebugSettingsStore::Get().enemySpawnPerFrame;
		return spawnPerFrame > 0 ? spawnPerFrame : 1;
	}

}
