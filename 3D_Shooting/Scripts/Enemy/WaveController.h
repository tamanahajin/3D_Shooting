/*!
@file WaveController.h
@brief 敵ウェーブ管理
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
	@brief ウェーブ進行と敵生成数に関する設定

	ウェーブ間隔、初期敵数、ウェーブごとの増加数、速度上昇間隔、
	スポーン距離など、敵ウェーブ全体の調整値をまとめる。
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
	@brief ウェーブの状態管理と敵生成指示を担当するクラス

	GameStage はスポーン中心位置を渡すだけにし、
	何ウェーブ目か、何体出すか、速度倍率をここに集約する。
	生成位置抽選と分割生成は EnemySpawner に任せる。
	*/
	class WaveController
	{
	public:
		/*!
		@brief 既定設定でウェーブコントローラを生成する
		*/
		WaveController()
		{
			m_statusByKind[EnemyKind::Default] = EnemyStatus();
		}
		/*!
		@brief 敵バッチコントローラを指定して生成する
		@param controller 敵生成先のバッチコントローラ
		*/
		explicit WaveController(const std::shared_ptr<EnemyController>& controller);

		/*!
		@brief 敵生成先のバッチコントローラを設定する
		@param controller 敵生成先のバッチコントローラ
		*/
		void SetController(const std::shared_ptr<EnemyController>& controller);
		/*!
		@brief 敵生成位置の検証先を設定する
		@param resolver ステージ固有の生成位置解決処理
		*/
		void SetEnemySpawnPositionResolver(
			const std::shared_ptr<EnemySpawnPositionResolver>& resolver);
		/*!
		@brief ウェーブが敵を生成できる状態かを判定する
		@return 敵生成先と EnemySpawner が有効なら true
		*/
		bool IsValid() const;
		/*!
		@brief 敵種別ごとのステータスを設定する
		@param kind 敵種別
		@param status 適用する敵ステータス
		*/
		void SetEnemyStatus(EnemyKind kind, const EnemyStatus& status);
		/*!
		@brief 敵種別ごとのステータスを取得する
		@param kind 敵種別
		@return 指定種別の設定。なければ Default、さらに無ければ既定値
		*/
		EnemyStatus GetEnemyStatus(EnemyKind kind) const;

		/*!
		@brief ウェーブタイマーを進め、時間切れなら次ウェーブを開始する
		@param elapsedTime 経過時間
		@param spawnCenter 敵生成の中心位置
		*/
		void Update(double elapsedTime, const Vec3& spawnCenter);
		/*!
		@brief 次のウェーブ番号へ進めて敵を生成する
		@param spawnCenter 敵生成の中心位置
		*/
		void StartNextWave(const Vec3& spawnCenter);
		/*!
		@brief 次に開始するウェーブ番号を設定する
		@param wave 次に開始したいウェーブ番号
		*/
		void SetNextWaveNumber(int wave);
		/*!
		@brief 任意の敵数と生成設定で敵バッチを生成する
		@param center 生成中心
		@param count 生成数
		@param settings 配置ルール
		@param kind 敵種別
		@return 実際に生成できた敵数
		*/
		int CreateEnemy(
			const Vec3& center,
			int count,
			const EnemyFactory::SpawnSettings& settings,
			EnemyKind kind = EnemyKind::Default);

		/*!
		@brief これまでのウェーブ生成で作った敵総数を取得する
		@return 敵総生成数
		*/
		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		/*!
		@brief 現在のウェーブ番号を取得する
		@return 現在ウェーブ番号
		*/
		int GetCurrentWave() const { return m_currentWave; }
		/*!
		@brief 次ウェーブまでの残り時間を取得する
		@return 秒単位の残り時間
		*/
		double GetWaveTimeRemaining() const { return m_waveTimer; }
		/*!
		@brief ウェーブ設定を編集用に取得する
		@return ウェーブ設定
		*/
		WaveSettings& GetSettings() { return m_settings; }
		/*!
		@brief ウェーブ設定を参照用に取得する
		@return ウェーブ設定
		*/
		const WaveSettings& GetSettings() const { return m_settings; }

		/*!
		@brief 指定ウェーブで生成する敵数を計算する
		@param wave ウェーブ番号
		@return 生成する敵数
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
		@param wave ウェーブ番号
		@return デバッグ倍率を含まない速度倍率
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
		@brief 指定ウェーブの最終速度倍率を計算する
		@param wave ウェーブ番号
		@return デバッグ設定を掛けた速度倍率
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

		/*!
		@brief 現在の敵バッチコントローラを取得する
		@return 有効なら EnemyController、無効なら nullptr
		*/
		std::shared_ptr<EnemyController> GetController() const;
		/*!
		@brief EnemySpawner を作成または更新する
		@param controller 敵生成先のバッチコントローラ
		*/
		void PrepareEnemySpawner(const std::shared_ptr<EnemyController>& controller);
		/*!
		@brief 敵生成バッチを分割生成キューへ積む
		@param desc 生成バッチ情報
		*/
		void QueueEnemy(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief 分割生成キューを1フレームぶん進める
		*/
		void ProcessPendingEnemySpawns();
		/*!
		@brief 分割生成キューに未生成分が残っているかを判定する
		@return 未生成分がある場合は true
		*/
		bool HasPendingEnemySpawns() const;
		/*!
		@brief 1フレームに生成する敵数を取得する
		@return 1以上の生成数
		*/
		int GetEnemySpawnPerFrame() const;
	};

}
