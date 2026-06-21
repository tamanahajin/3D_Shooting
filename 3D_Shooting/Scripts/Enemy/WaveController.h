/*!
@file WaveController.h
@brief 敵ウェーブの進行、敵数計算、速度倍率、生成キューを管理する
*/

#pragma once
#include "stdafx.h"
#include "DebugSettings.h"
#include "EnemySpawner.h"
#include <memory>
#include <map>

namespace shooting {

	class EnemyController;

	/*!
	@brief ウェーブ進行と生成位置抽選に使う調整値
	*/
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

	/*!
	@brief ウェーブ状態と敵生成指示を担当するクラス

	GameStage は生成中心位置を渡すだけにし、敵数・速度倍率・分割生成キューの進行をここへ集約する。
	*/
	class WaveController
	{
	public:
		WaveController()
		{
			m_statusByKind[EnemyKind::Default] = EnemyStatus();
		}
		explicit WaveController(const std::shared_ptr<EnemyController>& controller);

		void SetController(const std::shared_ptr<EnemyController>& controller);
		void SetEnemySpawnPositionResolver(
			const std::shared_ptr<EnemySpawnPositionResolver>& resolver);
		bool IsValid() const;
		void SetEnemyStatus(EnemyKind kind, const EnemyStatus& status);
		EnemyStatus GetEnemyStatus(EnemyKind kind) const;

		/*!
		@brief ウェーブタイマーと分割生成キューを進める

		生成キューが残っている間は次ウェーブのタイマーを止め、生成待ちの重なりを避ける。
		*/
		void Update(double elapsedTime, const Vec3& spawnCenter);
		/*!
		@brief 次ウェーブへ進め、敵生成バッチをキューへ積む
		*/
		void StartNextWave(const Vec3& spawnCenter);
		void SetNextWaveNumber(int wave);
		/*!
		@brief ウェーブ管理外から任意条件で敵生成を行う
		*/
		int CreateEnemy(
			const Vec3& center,
			int count,
			const EnemyFactory::SpawnSettings& settings,
			EnemyKind kind = EnemyKind::Default);

		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		int GetCurrentWave() const { return m_currentWave; }
		double GetWaveTimeRemaining() const { return m_waveTimer; }
		WaveSettings& GetSettings() { return m_settings; }
		const WaveSettings& GetSettings() const { return m_settings; }

		/*!
		@brief 指定ウェーブで生成する敵数を計算する

		DebugSettings の敵数上書きが有効な場合は、通常の増加式よりそちらを優先する。
		*/
		int GetEnemyCountForWave(int wave) const
		{
			if (wave <= 0)
			{
				return 0;
			}

			const auto& debug = GameDebugSettingsStore::Get();
			if (debug.overrideEnemyCount)
			{
				return debug.enemyCountOverride > 0 ? debug.enemyCountOverride : 0;
			}

			const int enemyCount = m_settings.firstWaveEnemyCount +
				((wave - 1) * m_settings.addEnemyCountPerWave);
			return enemyCount > 0 ? enemyCount : 0;
		}

		/*!
		@brief 指定ウェーブの基本速度倍率を計算する
		*/
		float GetEnemySpeedMultiplierForWave(int wave) const
		{
			if (wave <= 0 || m_settings.speedUpEveryWaves <= 0)
			{
				return 1.0f;
			}

			const int speedStep = wave / m_settings.speedUpEveryWaves;
			return 1.0f + (static_cast<float>(speedStep) * m_settings.speedMultiplierAddPerStep);
		}

		/*!
		@brief デバッグ倍率を含めた最終速度倍率を計算する
		*/
		float GetAppliedEnemySpeedMultiplierForWave(int wave) const
		{
			float debugMultiplier = GameDebugSettingsStore::Get().enemySpeedMultiplier;
			if (debugMultiplier < 0.1f)
			{
				debugMultiplier = 0.1f;
			}
			return GetEnemySpeedMultiplierForWave(wave) * debugMultiplier;
		}

	private:
		WaveSettings m_settings;
		int m_totalEnemyCount = 0;
		int m_currentWave = 0;
		double m_waveTimer = 0.0;
		std::weak_ptr<EnemyController> m_controller;
		std::weak_ptr<EnemySpawnPositionResolver> m_enemySpawnPositionResolver;
		std::shared_ptr<EnemySpawner> m_enemySpawner;
		std::map<EnemyKind, EnemyStatus> m_statusByKind;

		std::shared_ptr<EnemyController> GetController() const;
		/*!
		@brief EnemySpawner を現在の Controller と設定へ同期する
		*/
		void PrepareEnemySpawner(const std::shared_ptr<EnemyController>& controller);
		/*!
		@brief 敵生成バッチを分割生成キューへ積む

		同じウェーブ内で敵ステータスが途中変更されないよう、Spawner 側で設定を固定する。
		*/
		void QueueEnemy(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief DebugSettings の生成数上限に従ってキューを1フレーム分進める
		*/
		void ProcessPendingEnemySpawns();
		bool HasPendingEnemySpawns() const;
		int GetEnemySpawnPerFrame() const;
	};

}
