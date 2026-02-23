/*!
@file GameStage.cpp
@brief ゲームステージクラス　実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	//--------------------------------------------------------------------------------------
	// ゲームステージ
	//--------------------------------------------------------------------------------------

	//追いかけるオブジェクトの作成
	void GameStage::CreateSeekObject()
	{
		//オブジェクトのグループを作成する
		auto group = CreateSharedObjectGroup(L"SeekGroup");
		//配列の初期化
		//配列の初期化
		std::vector<Vec3> vec = {
			{ 0, 0.525f, 10.0f },
			{ 10.0f, 0.525f, 0.0f },
			{ -10.0f, 0.525f, 0.0f },
			{ 0, 0.525f, -15.0f },
			{ 0, 0.525f, -16.0f },
			{ 0, 0.525f, -17.0f },
			{ 15, 0.525f, 0.0f },
			{ 20, 0.525f, -10.0f },
			{ -15, 0.525f, -10.0f },
			{ -20, 0.525f, -10.0f },
			{ 0, 0.525f, -7.0f },
		};

		//配置オブジェクトの作成
		for (size_t count = 0; count < vec.size(); count++)
		{
			AddGameObject<SeekObject>(vec[count]);
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
		param.scale = Vec3(50.0f, 1.0f, 50.0f);
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
