/*!
@file GameStage.cpp
@brief ゲームステージクラス　実体
*/

#include "stdafx.h"
#include "Project.h"
#include <random>

namespace shooting {

	//--------------------------------------------------------------------------------------
	// ゲームステージ
	//--------------------------------------------------------------------------------------

	//追いかけるオブジェクトの作成
	void GameStage::CreateSeekObject()
	{
		//オブジェクトのグループを作成する
		auto group = CreateSharedObjectGroup(L"SeekGroup");
		
		// 生成する敵の数
		const size_t enemyCount = 20;
		
		// ランダム配置のパラメータ
		const float minDistance = 5.0f;   // 最小距離
		const float maxDistance = 20.0f;  // 最大距離
		const float yPosition = 0.525f;   // Y座標（地面の高さ）
		
		std::vector<Vec3> positions;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distRadius(minDistance, maxDistance);
		std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);
		
		for (size_t count = 0; count < enemyCount; count++)
		{
			Vec3 position;
			bool validPosition = false;
			int attempts = 0;
			const int maxAttempts = 50;
			
			while (!validPosition && attempts < maxAttempts)
			{
				// 極座標でランダムな位置を生成
				float radius = distRadius(gen);
				float angle = distAngle(gen);
				
				position = Vec3(
					radius * cosf(angle),
					yPosition,
					radius * sinf(angle)
				);
				
				// 他のオブジェクトとの最小距離をチェック
				validPosition = true;
				for (const auto& existingPos : positions)
				{
					float dist = (position - existingPos).length();
					if (dist < minDistance * 0.5f)
					{
						validPosition = false;
						break;
					}
				}
				
				attempts++;
			}
			
			positions.push_back(position);
		}
		
		// 配置オブジェクトの作成
		for (const auto& pos : positions)
		{
			AddGameObject<SeekObject>(pos);
		}
	}

	// 空中浮遊敵の作成
	void GameStage::CreateFloatingEnemies()
	{
		std::vector<Vec3> positions = {
			{ 5.0f, 3.0f, 5.0f },
			{ -5.0f, 4.0f, 5.0f },
			{ 5.0f, 3.5f, -5.0f },
			{ -5.0f, 4.5f, -5.0f },
			{ 0.0f, 5.0f, 8.0f },
		};

		for (const auto& pos : positions)
		{
			AddGameObject<FloatingEnemy>(pos);
		}
	}

	void GameStage::OnCreate()
	{
		//カメラとライトの設定
		m_camera = ObjectFactory::Create<MainCamera>(GetThis<Stage>());
		//m_camera = ObjectFactory::Create<MainCamera>();
		m_camera->SetEye(Vec3(0, 3.43f, -6.37f));
		m_camera->SetAt(Vec3(0, 0.125f, 0));
		m_lightSet = ObjectFactory::Create<LightSet>();
		TransParam param;
		param.scale = Vec3(1.0f, 1.0f, 1.0f);
		//	param.rotOrigin = Vec3(0.0f, 0.0f, 0.0f);
		auto quat = XMQuaternionIdentity();
		param.quaternion = Quat(quat);
		param.position = Vec3(5.0f, 2.0f, 5.0f);
		AddGameObject<WallBox>(param);
		// 地面
		param.scale = Vec3(150.0f, 1.0f, 150.0f);
		param.position = Vec3(0.0f, -0.5, 0.0f);
		AddGameObject<FixedBox>(param);

		param.scale = Vec3(5.0f, 1.0f, 5.0f);
		param.position = Vec3(10.0f, 0.0, 10.0f);
		AddGameObject<FixedBox>(param);

		param.position = Vec3(10.0f, 0.0, 10.0f);
		param.quaternion = Quat(Vec3(-1, 0, 1), XM_PIDIV4);
		AddGameObject<FixedBox>(param);

		param.position = Vec3(-10.0f, 0.0, 10.0f);
		param.quaternion = Quat(Vec3(0, 1, 1), XM_PIDIV4);
		AddGameObject<FixedBox>(param);


		CreateSeekObject();
		CreateFloatingEnemies();  // 空中浮遊敵を追加

		param.scale = Vec3(0.4f, 0.4f, 0.4f);
		param.quaternion = Quat();
		param.position = Vec3(0.0f, 0.525f, 0.0f);
		AddGameObject<Player>(param);

		// 弾管理
		AddGameObject<BulletManager>();
	}


}
