#include "stdafx.h"
#include "Project.h"

namespace shooting {

	BulletManager::BulletManager(const std::shared_ptr<Stage>& stagePtr)
		: GameObject(stagePtr)
	{
	}

	void BulletManager::OnCreate()
	{
		// 共有登録
		GetStage()->SetSharedGameObject(L"BulletManager", GetThis<BulletManager>());

		// 通常射撃はヒットスキャン方式なので、実体を生成する爆弾のプールだけを準備する。
		GetOrCreatePool<BombBullet>();
	}

	void BulletManager::OnUpdate(double elapsedTime)
	{
		for (auto& pool : m_pools)
		{
			if (pool) { pool->OnUpdate(elapsedTime); } // Poolは回収だけ担当
		}
	}

}
