#include "stdafx.h"
#include "Project.h"

namespace shooting {

	BulletManager::BulletManager(const std::shared_ptr<Stage>& stagePtr) :
		GameObject(stagePtr)
	{
	}

	void BulletManager::OnCreate()
	{
		// 共有登録
		GetStage()->SetSharedGameObject(L"BulletManager", GetThis<BulletManager>());

		// 事前にプール作成
		GetOrCreatePool<DefaultBullet>();
	}

	void BulletManager::OnUpdate(double elapsedTime)
	{
		for (auto& pool : m_Pools)
		{
			if (pool) { pool->OnUpdate(elapsedTime); }
		}
	}

	
}