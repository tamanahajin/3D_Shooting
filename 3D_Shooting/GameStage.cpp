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

	int GameStage::GetEnemyCountForWave(int wave) const
	{
		if (wave <= 0)
		{
			return 0;
		}

		const int enemyCount = m_waveSettings.firstWaveEnemyCount +
			((wave - 1) * m_waveSettings.addEnemyCountPerWave);
		return enemyCount > 0 ? enemyCount : 0;
	}

	float GameStage::GetEnemySpeedMultiplierForWave(int wave) const
	{
		if (wave <= 0 || m_waveSettings.speedUpEveryWaves <= 0)
		{
			return 1.0f;
		}

		const int speedStep = wave / m_waveSettings.speedUpEveryWaves;
		return 1.0f + (static_cast<float>(speedStep) * m_waveSettings.speedMultiplierAddPerStep);
	}

	std::shared_ptr<EnemyBatchController> GameStage::GetEnemyController() const
	{
		auto controllerObject = GetSharedGameObject(L"EnemyBatchController", false);
		return std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
	}

	Vec3 GameStage::GetEnemySpawnCenter() const
	{
		Vec3 spawnCenter(0.0f, m_waveSettings.spawnY, 0.0f);
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

	void GameStage::StartNextWave()
	{
		auto controller = GetEnemyController();
		if (!controller || !m_enemyFactory)
		{
			return;
		}

		m_enemyFactory->SetController(controller);

		++m_currentWave;
		m_waveTimer = m_waveSettings.intervalSeconds;

		const Vec3 spawnCenter = GetEnemySpawnCenter();
		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.count = GetEnemyCountForWave(m_currentWave);
		spawnDesc.center = spawnCenter;
		spawnDesc.settings.minDistance = m_waveSettings.spawnMinDistance;
		spawnDesc.settings.maxDistance = m_waveSettings.spawnMaxDistance;
		spawnDesc.settings.spawnY = spawnCenter.y;
		spawnDesc.settings.minSpacing = m_waveSettings.minSpawnSpacing;
		spawnDesc.settings.maxAttempts = m_waveSettings.maxSpawnAttempts;

		controller->SetMoveSpeedMultiplier(GetEnemySpeedMultiplierForWave(m_currentWave));
		m_totalEnemyCount += m_enemyFactory->CreateEnemiesAround(spawnDesc);
	}

	void GameStage::UpdateWaves(double elapsedTime)
	{
		if (m_currentWave <= 0 || m_waveSettings.intervalSeconds <= 0.0)
		{
			return;
		}

		m_waveTimer -= elapsedTime;
		if (m_waveTimer <= 0.0)
		{
			StartNextWave();
		}
	}

	void GameStage::OnUpdate2(double elapsedTime)
	{
		UpdateWaves(elapsedTime);

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

	//追いかけるオブジェクトの作成
	void GameStage::CreateSeekObject()
	{
		auto controller = GetEnemyController();
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

		EnemyFactory::SpawnBatchDesc spawnDesc;
		spawnDesc.count = 40;
		spawnDesc.center = Vec3(0.0f, 0.0f, 0.0f);
		spawnDesc.settings.minDistance = 5.0f;
		spawnDesc.settings.maxDistance = 20.0f;
		spawnDesc.settings.spawnY = 0.525f;
		spawnDesc.settings.minSpacing = 2.5f;
		spawnDesc.settings.maxAttempts = 50;

		m_totalEnemyCount += m_enemyFactory->CreateEnemiesAround(spawnDesc);
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
		const int defeated = m_totalEnemyCount - GetAliveEnemyCount();
		return (defeated > 0) ? defeated : 0;
	}

	void GameStage::OnCreate()
	{
		//カメラとライトの設定
		m_camera = ObjectFactory::Create<MainCamera>(GetThis<Stage>());
		//m_camera = ObjectFactory::Create<MainCamera>();
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
		m_enemyFactory = std::make_shared<EnemyFactory>(enemyController);
		AddGameObject<EnemyInstancedRenderer>();
		StartNextWave();

		// 弾管理
		AddGameObject<BulletManager>();
	}


}
