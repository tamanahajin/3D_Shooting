#include "stdafx.h"
#include "WaveController.h"
#include "Character.h"

namespace shooting {

	WaveController::WaveController()
	{
	}

	WaveController::WaveController(const std::shared_ptr<EnemyBatchController>& controller)
	{
		SetController(controller);
	}

	void WaveController::SetController(const std::shared_ptr<EnemyBatchController>& controller)
	{
		m_controller = controller;
		EnsureEnemyFactory(controller);
	}

	bool WaveController::IsValid() const
	{
		return !m_controller.expired() && m_enemyFactory && m_enemyFactory->IsValid();
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

		EnsureEnemyFactory(controller);
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

		controller->SetMoveSpeedMultiplier(GetEnemySpeedMultiplierForWave(m_currentWave));
		m_totalEnemyCount += m_enemyFactory->CreateEnemiesAround(spawnDesc);
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

		EnsureEnemyFactory(controller);
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

	int WaveController::GetEnemyCountForWave(int wave) const
	{
		if (wave <= 0)
		{
			return 0;
		}

		const int enemyCount = m_settings.firstWaveEnemyCount +
			((wave - 1) * m_settings.addEnemyCountPerWave);
		return enemyCount > 0 ? enemyCount : 0;
	}

	float WaveController::GetEnemySpeedMultiplierForWave(int wave) const
	{
		if (wave <= 0 || m_settings.speedUpEveryWaves <= 0)
		{
			return 1.0f;
		}

		const int speedStep = wave / m_settings.speedUpEveryWaves;
		return 1.0f + (static_cast<float>(speedStep) * m_settings.speedMultiplierAddPerStep);
	}

	std::shared_ptr<EnemyBatchController> WaveController::GetController() const
	{
		return m_controller.lock();
	}

	void WaveController::EnsureEnemyFactory(const std::shared_ptr<EnemyBatchController>& controller)
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
	}

}