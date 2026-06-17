#include "stdafx.h"
#include "WaveController.h"

namespace shooting {

	/*!
	@brief 敵バッチコントローラを指定して生成する
	@param controller 敵生成先のバッチコントローラ
	*/
	WaveController::WaveController(const std::shared_ptr<EnemyController>& controller) :
		WaveController()
	{
		SetController(controller);
	}

	/*!
	@brief 敵生成先のバッチコントローラを設定する
	@param controller 敵生成先のバッチコントローラ
	*/
	void WaveController::SetController(const std::shared_ptr<EnemyController>& controller)
	{
		m_controller = controller;
		WaveEnemyFactory(controller);
	}

	void WaveController::SetEnemySpawnPositionResolver(
		const std::shared_ptr<EnemySpawnPositionResolver>& resolver)
	{
		m_enemySpawnPositionResolver = resolver;
		if (m_enemyFactory)
		{
			m_enemyFactory->SetSpawnPositionResolver(resolver);
		}
	}

	/*!
	@brief ウェーブが敵を生成できる状態かを判定する
	@return 敵生成先と EnemyFactory が有効なら true
	*/
	bool WaveController::IsValid() const
	{
		return !m_controller.expired() && m_enemyFactory && m_enemyFactory->IsValid();
	}

	/*!
	@brief 敵種別ごとのステータスを設定する
	@param kind 敵種別
	@param status 適用する敵ステータス
	*/
	void WaveController::SetEnemyStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
		if (m_enemyFactory)
		{
			m_enemyFactory->SetStatus(kind, status);
		}
	}

	/*!
	@brief 敵種別ごとのステータスを取得する
	@param kind 敵種別
	@return 指定種別の設定。なければ Default、さらに無ければ既定値
	*/
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

	/*!
	@brief ウェーブタイマーを進め、時間切れなら次ウェーブを開始する
	@param elapsedTime 経過時間
	@param spawnCenter 敵生成の中心位置
	*/
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

	/*!
	@brief 次のウェーブ番号へ進めて敵を生成する
	@param spawnCenter 敵生成の中心位置

	ウェーブ番号から敵数と速度倍率を計算し、敵生成キューへ積む。
	*/
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
		QueueEnemy(spawnDesc);
		ProcessPendingEnemySpawns();
	}

	/*!
	@brief 次に開始するウェーブ番号を設定する
	@param wave 次に開始したいウェーブ番号
	*/
	void WaveController::SetNextWaveNumber(int wave)
	{
		m_currentWave = wave > 1 ? wave - 1 : 0;
		m_waveTimer = 0.0;
	}

	/*!
	@brief 任意の敵数と生成設定で敵バッチを生成する
	@param center 生成中心
	@param count 生成数
	@param settings 配置ルール
	@param kind 敵種別
	@return 実際に生成できた敵数
	*/
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

	/*!
	@brief 現在の敵バッチコントローラを取得する
	@return 有効なら EnemyController、無効なら nullptr
	*/
	std::shared_ptr<EnemyController> WaveController::GetController() const
	{
		return m_controller.lock();
	}

	/*!
	@brief EnemyFactory を作成または更新する
	@param controller 敵生成先のバッチコントローラ

	保持している敵種別ごとのステータスも、Factory 側へ再同期する。
	*/
	void WaveController::WaveEnemyFactory(const std::shared_ptr<EnemyController>& controller)
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
		m_enemyFactory->SetSpawnPositionResolver(m_enemySpawnPositionResolver.lock());

		for (const auto& statusByKind : m_statusByKind)
		{
			m_enemyFactory->SetStatus(statusByKind.first, statusByKind.second);
		}
	}

	/*!
	@brief 敵生成バッチを分割生成キューへ積む
	@param desc 生成バッチ情報

	ステータスをここで固定しておくと、生成が複数フレームに分かれても
	同じウェーブ内の敵設定が途中で変わらない。
	*/
	void WaveController::QueueEnemy(const EnemyFactory::SpawnBatchDesc& desc)
	{
		if (desc.count <= 0)
		{
			return;
		}

		PendingSpawnBatch batch;
		batch.desc = desc;
		if (!batch.desc.overrideStatus)
		{
			batch.desc.overrideStatus = true;
			batch.desc.status = GetEnemyStatus(batch.desc.kind);
		}
		batch.acceptedPositions.reserve(static_cast<size_t>(desc.count));
		m_pendingSpawnBatches.push_back(batch);
	}

	/*!
	@brief 分割生成キューを1フレームぶん進める

	DebugSettings の enemySpawnPerFrame 体まで生成し、残りは次フレームへ回す。
	*/
	void WaveController::ProcessPendingEnemySpawns()
	{
		if (!m_enemyFactory || !m_enemyFactory->IsValid())
		{
			return;
		}

		int remainingBudget = GetEnemySpawnPerFrame();
		while (remainingBudget > 0 && !m_pendingSpawnBatches.empty())
		{
			auto& batch = m_pendingSpawnBatches.front();
			const int processedBefore = batch.processedCount;
			const int createdCount = m_enemyFactory->CreateEnemiesAroundStep(
				batch.desc,
				remainingBudget,
				batch.acceptedPositions,
				batch.processedCount);
			const int processedThisFrame = batch.processedCount - processedBefore;

			m_totalEnemyCount += createdCount;

			if (processedThisFrame <= 0)
			{
				// 何も進まない場合は無限ループを避ける。Factoryが無効化された場合などの保険。
				break;
			}

			remainingBudget -= processedThisFrame;
			if (batch.processedCount >= batch.desc.count)
			{
				m_pendingSpawnBatches.pop_front();
			}
		}
	}

	/*!
	@brief 1フレームに生成する敵数を取得する
	@return 1以上の生成数
	*/
	int WaveController::GetEnemySpawnPerFrame() const
	{
		const int spawnPerFrame = GameDebugSettingsStore::Get().enemySpawnPerFrame;
		return spawnPerFrame > 0 ? spawnPerFrame : 1;
	}

}
