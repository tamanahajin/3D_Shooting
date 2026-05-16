#pragma once
#include "stdafx.h"
#include "EnemyFactory.h"
#include <memory>

namespace shooting {

	class EnemyBatchController;

	struct WaveSettings
	{
		double intervalSeconds = 15.0;
		int firstWaveEnemyCount = 5;
		int addEnemyCountPerWave = 1;
		int speedUpEveryWaves = 5;
		float speedMultiplierAddPerStep = 0.08f;
		float spawnMinDistance = 5.0f;
		float spawnMaxDistance = 20.0f;
		float spawnY = 0.525f;
		float minSpawnSpacing = 2.5f;
		int maxSpawnAttempts = 50;
	};

	// ウェーブの状態管理と敵生成指示を担当するクラス。
	// GameStageはスポーン中心位置を渡すだけにし、何ウェーブ目か・何体出すか・速度倍率はここに集約する。
	class WaveController
	{
	public:
		WaveController();
		explicit WaveController(const std::shared_ptr<EnemyBatchController>& controller);

		void SetController(const std::shared_ptr<EnemyBatchController>& controller);
		bool IsValid() const;

		void Update(double elapsedTime, const Vec3& spawnCenter);
		void StartNextWave(const Vec3& spawnCenter);
		int CreateEnemyBatch(
			const Vec3& center,
			int count,
			const EnemyFactory::SpawnSettings& settings,
			EnemyKind kind = EnemyKind::Default);

		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		int GetCurrentWave() const { return m_currentWave; }
		double GetWaveTimeRemaining() const { return m_waveTimer; }
		WaveSettings& GetSettings() { return m_settings; }
		const WaveSettings& GetSettings() const { return m_settings; }

		int GetEnemyCountForWave(int wave) const;
		float GetEnemySpeedMultiplierForWave(int wave) const;

	private:
		WaveSettings m_settings;
		int m_totalEnemyCount = 0;
		int m_currentWave = 0;
		double m_waveTimer = 0.0;
		std::weak_ptr<EnemyBatchController> m_controller;
		std::shared_ptr<EnemyFactory> m_enemyFactory;

		std::shared_ptr<EnemyBatchController> GetController() const;
		void EnsureEnemyFactory(const std::shared_ptr<EnemyBatchController>& controller);
	};

}