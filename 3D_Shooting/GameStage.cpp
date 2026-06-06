/*!
@file GameStage.cpp
@brief ゲームステージクラス　実体
*/

#include "stdafx.h"
#include "Project.h"
#include <map>

namespace shooting {

	// ダメージ表示は短命なUIデータだけを保持し、描画はUIManager側でまとめて行う。
	void GameStage::SpawnDamageNumber(const Vec3& position, int damage)
	{
		if (damage <= 0)
		{
			return;
		}

		DamageNumberEntry entry;
		entry.text = std::to_wstring(damage);
		entry.position = position;
		entry.life = 0.9;

		const int offsetIndex = static_cast<int>(m_damageNumbers.size() % 3) - 1;
		entry.velocity = Vec3(static_cast<float>(offsetIndex) * 0.08f, 0.75f, 0.0f);
		m_damageNumbers.push_back(entry);

		const size_t maxDamageNumbers = 64;
		if (m_damageNumbers.size() > maxDamageNumbers)
		{
			m_damageNumbers.erase(m_damageNumbers.begin());
		}
	}

	std::shared_ptr<EnemyBatchController> GameStage::GetEnemyController() const
	{
		auto controllerObject = GetSharedGameObject(L"EnemyBatchController", false);
		return std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
	}

	Vec3 GameStage::GetEnemySpawnCenter() const
	{
		Vec3 spawnCenter(0.0f, m_waveController.GetSettings().spawnY, 0.0f);
		auto player = GetSharedGameObject(L"Player", false);
		if (player)
		{
			auto playerTransform = player->GetComponent<Transform>(false);
			if (playerTransform)
			{
				spawnCenter = playerTransform->GetWorldPosition();
			}
		}
		return spawnCenter;
	}

	void GameStage::RequestHitStop(double duration, double timeScale)
	{
		m_HitStop.Request(duration, timeScale);
	}

	double GameStage::GetGameDeltaTime(double rawDeltaTime) const
	{
		return m_HitStop.Apply(rawDeltaTime);
	}

	void GameStage::OnUpdate2(double elapsedTime)
	{
		// ヒットストップの残り時間はゲーム内時間ではなく実時間で減らす。
		// ここで更新しておくと、敵・プレイヤー・弾は次フレームから GetGameDeltaTime() 経由で遅くなる。
		m_HitStop.Update(elapsedTime);
		ApplyDebugRuntimeSettings();

		if (m_WaitingInitialWaveUntilPlayerIntroEnds)
		{
			StartInitialWaveAfterPlayerIntro();
		}
		else
		{
			m_waveController.Update(elapsedTime, GetEnemySpawnCenter());
		}

		const float dt = static_cast<float>(elapsedTime);
		for (auto& damageNumber : m_damageNumbers)
		{
			damageNumber.age += elapsedTime;
			damageNumber.position.x += damageNumber.velocity.x * dt;
			damageNumber.position.y += damageNumber.velocity.y * dt;
			damageNumber.position.z += damageNumber.velocity.z * dt;
			damageNumber.velocity.y *= 0.96f;
		}

		m_damageNumbers.erase(
			std::remove_if(
				m_damageNumbers.begin(),
				m_damageNumbers.end(),
				[](const DamageNumberEntry& damageNumber)
				{
					return damageNumber.age >= damageNumber.life;
				}),
			m_damageNumbers.end());

		MaintainRecoveryItems();
		MaintainBombItems();
	}

	void GameStage::ApplyDebugRuntimeSettings()
	{
		const auto& debug = GameDebugSettingsStore::Get();

		Collision::SetGlobalDebugDraw(debug.showCollision);

		auto playerObject = GetSharedGameObject(L"Player", false);
		if (playerObject)
		{
			if (auto hp = playerObject->GetComponent<Health>(false))
			{
				hp->SetInvincible(debug.playerInvincible);
			}
		}

		auto controller = GetEnemyController();
		if (controller)
		{
			const int currentWave = m_waveController.GetCurrentWave();
			const int waveForSpeed = currentWave > 0 ? currentWave : 1;
			controller->SetMoveSpeedMultiplier(m_waveController.GetAppliedEnemySpeedMultiplierForWave(waveForSpeed));
		}
	}

	void GameStage::StartInitialWaveAfterPlayerIntro()
	{
		auto playerObject = GetSharedGameObject(L"Player", false);
		auto player = std::dynamic_pointer_cast<Player>(playerObject);

		// 登場演出中は敵を出さない。
		if (player && player->IsSpawnIntroActive())
		{
			return;
		}

		m_WaitingInitialWaveUntilPlayerIntroEnds = false;
		m_waveController.SetNextWaveNumber(GameDebugSettingsStore::Get().startWave);
		// インゲームBGMは登場演出が終わってから開始する。
		GameAudio::Instance().PlayBgm(GameBgmId::InGame);
		m_waveController.StartNextWave(GetEnemySpawnCenter());
	}

	//追いかけるオブジェクトの作成
	void GameStage::CreateSeekObject()
	{
		auto controller = GetEnemyController();
		if (!controller)
		{
			return;
		}

		m_waveController.SetController(controller);

		EnemyFactory::SpawnSettings settings;
		settings.minDistance = 5.0f;
		settings.maxDistance = 20.0f;
		settings.spawnY = 0.525f;
		settings.minSpacing = 2.5f;
		settings.maxAttempts = 50;

		m_waveController.CreateEnemyBatch(Vec3(0.0f, 0.0f, 0.0f), 40, settings);
	}

	int GameStage::GetAliveEnemyCount() const
	{
		auto controller = GetEnemyController();
		if (controller)
		{
			return controller->GetAliveEnemyCount();
		}
		return 0;
	}

	int GameStage::GetDefeatedEnemyCount() const
	{
		const int defeated = GetTotalEnemyCount() - GetAliveEnemyCount();
		return (defeated > 0) ? defeated : 0;
	}

	void GameStage::OnCreate()
	{
		//カメラとライトの設定
		m_camera = ObjectFactory::Create<MainCamera>(GetThis<Stage>());
		m_camera->SetEye(Vec3(0, 3.43f, -6.37f));
		m_camera->SetAt(Vec3(0, 0.125f, 0));
		m_lightSet = ObjectFactory::Create<LightSet>();
		// 地面
		CreateGround();
		// 壁
		CreateWalls();
		// スロープと高台
		CreateHeightVariationObjects();
		// 配置物
		CreateCoverObjects();
		// アイテム
		CreateItems();
		// 空
		AddGameObject<SkyDome>();
		// プレイヤー
		TransParam param;
		param.scale = Vec3(0.3f, 0.3f, 0.3f);
		param.quaternion = Quat();
		param.position = Vec3(0.0f, 0.525f, 0.0f);
		auto player = AddGameObject<Player>(param);
		AddGameObject<PlayerWeapon>(player);
		// 敵
		auto enemyController = AddGameObject<EnemyBatchController>();
		m_waveController.SetController(enemyController);
		AddGameObject<EnemyInstancedRenderer>();
		// 初回ウェーブはプレイヤー登場演出が終わってから開始する。
		// ここで即生成すると、演出中に敵が画面へ入り込んでしまう。
		m_WaitingInitialWaveUntilPlayerIntroEnds = true;

		// 弾管理
		AddGameObject<BulletManager>();
	}


}
