#include "stdafx.h"
#include "WaveController.h"
#include "Character.h"

namespace shooting {

	WaveController::WaveController(const std::shared_ptr<EnemyBatchController>& controller) :
		WaveController()
	{
		SetController(controller);
	}

	void WaveController::SetController(const std::shared_ptr<EnemyBatchController>& controller)
	{
		m_controller = controller;
		WaveEnemyFactory(controller);
	}

	bool WaveController::IsValid() const
	{
		return !m_controller.expired() && m_enemyFactory && m_enemyFactory->IsValid();
	}

	void WaveController::SetEnemyStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
		if (m_enemyFactory)
		{
			m_enemyFactory->SetStatus(kind, status);
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

	void WaveController::Update(double elapsedTime, const Vec3& spawnCenter)
	{
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

		WaveEnemyFactory(controller);
		if (!m_enemyFactory)
		{
			return;
		}

		++m_currentWave;
		m_waveTimer = m_settings.intervalSeconds;

		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.count = GetEnemyCountForWave(m_currentWave);
		spawnDesc.center = spawnCenter;
		spawnDesc.settings.minDistance = m_settings.spawnMinDistance;
		spawnDesc.settings.maxDistance = m_settings.spawnMaxDistance;
		spawnDesc.settings.spawnY = spawnCenter.y;
		spawnDesc.settings.minSpacing = m_settings.minSpawnSpacing;
		spawnDesc.settings.maxAttempts = m_settings.maxSpawnAttempts;

		controller->SetMoveSpeedMultiplier(GetAppliedEnemySpeedMultiplierForWave(m_currentWave));
		m_totalEnemyCount += m_enemyFactory->CreateEnemiesAround(spawnDesc);
	}

	void WaveController::SetNextWaveNumber(int wave)
	{
		m_currentWave = wave > 1 ? wave - 1 : 0;
		m_waveTimer = 0.0;
	}

	int WaveController::CreateEnemyBatch(
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

		WaveEnemyFactory(controller);
		if (!m_enemyFactory)
		{
			return 0;
		}

		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.kind = kind;
		spawnDesc.count = count;
		spawnDesc.center = center;
		spawnDesc.settings = settings;

		const int createdCount = m_enemyFactory->CreateEnemiesAround(spawnDesc);
		m_totalEnemyCount += createdCount;
		return createdCount;
	}

	std::shared_ptr<EnemyBatchController> WaveController::GetController() const
	{
		return m_controller.lock();
	}

	void WaveController::WaveEnemyFactory(const std::shared_ptr<EnemyBatchController>& controller)
	{
		if (!controller)
		{
			return;
		}

		if (!m_enemyFactory)
		{
			m_enemyFactory = std::make_shared<EnemyFactory>(controller);
		}
		else
		{
			m_enemyFactory->SetController(controller);
		}

		for (const auto& statusByKind : m_statusByKind)
		{
			m_enemyFactory->SetStatus(statusByKind.first, statusByKind.second);
		}
	}

}
