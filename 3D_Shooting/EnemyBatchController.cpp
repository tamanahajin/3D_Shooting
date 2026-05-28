/*!
@file EnemyBatchController.cpp
@brief 謨ｵ繝舌ャ繝∫ｮ｡逅・・蝓ｺ譛ｬ繝ｩ繧､繝輔し繧､繧ｯ繝ｫ

謨ｵ縺ｯ螟ｧ驥冗函謌舌＆繧後ｋ縺溘ａ縲・菴薙＃縺ｨ縺ｮGameObject譖ｴ譁ｰ繧帝∩縺代※縲√％縺ｮ繧ｯ繝ｩ繧ｹ縺ｮ驟榊・縺ｧ迥ｶ諷九ｒ縺ｾ縺ｨ繧√※譖ｴ譁ｰ縺吶ｋ縲・縺薙・繝輔ぃ繧､繝ｫ縺ｫ縺ｯ逕滓・縲∝・譛臥匳骭ｲ縲√・繝ｭ繧ｭ繧ｷ蜷梧悄縲∵焚縺ｮ蝠上＞蜷医ｏ縺帙↑縺ｩ縲∽ｸｭ蠢・→縺ｪ繧狗ｮ｡逅・・逅・□縺代ｒ鄂ｮ縺上・*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	EnemyBatchController::EnemyBatchController(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyBatchController::~EnemyBatchController() {}

	void EnemyBatchController::OnCreate()
	{
		SetDrawActive(false);
		SetShadowActive(false);
		GetStage()->SetSharedGameObject(L"EnemyBatchController", GetThis<GameObject>());
		AddTag(L"EnemyBatchController");
	}

	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition)
	{
		return AddEnemy(startPosition, EnemyStatus());
	}

	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition, const EnemyStatus& status)
	{
		EnemyState enemy;
		enemy.status = status;
		enemy.position = startPosition;
		enemy.previousPosition = startPosition;
		enemy.rotation = Quat();
		enemy.steeringTimer = static_cast<double>(m_Enemies.size() & 3) * 0.0125;
		enemy.steeringInterval = status.steeringInterval;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = status.maxHp > 0 ? status.maxHp : 1;
		enemy.hp = enemy.maxHp;

		const size_t index = m_Enemies.size();
		m_Enemies.push_back(enemy);

		// 当たり判定だけは軽量なGameObjectとして残し、描画やAI状態はm_Enemies側でまとめて扱う。
		auto proxy = GetStage()->AddGameObject<EnemyCollisionProxy>(GetThis<EnemyBatchController>(), index, startPosition, enemy.status);
		m_Enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
	}

	void EnemyBatchController::SetMoveSpeedMultiplier(float multiplier)
	{
		m_MoveSpeedMultiplier = bsmUtil::Max(0.1f, multiplier);
	}

	void EnemyBatchController::SyncProxyTransform(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (!proxy)
		{
			return;
		}

		auto transform = proxy->GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		transform->SetPosition(m_Enemies[index].position);
		transform->SetQuaternion(m_Enemies[index].rotation);
		transform->SetScale(m_Enemies[index].status.modelScale);
	}

	void EnemyBatchController::RemoveEnemyProxy(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (proxy)
		{
			proxy->RemoveTag(L"Enemy");
			proxy->SetDrawActive(false);
			proxy->SetUpdateActive(false);
			GetStage()->RemoveGameObject(proxy);
		}

		m_Enemies[index].active = false;
		m_Enemies[index].deathAnimFinished = true;
		m_Enemies[index].proxy.reset();
	}

	bool EnemyBatchController::IsEnemyAlive(size_t index) const
	{
		if (index >= m_Enemies.size())
		{
			return false;
		}

		const auto& enemy = m_Enemies[index];
		return enemy.active && !enemy.isDead && enemy.hp > 0;
	}

	int EnemyBatchController::GetAliveEnemyCount() const
	{
		int count = 0;
		for (const auto& enemy : m_Enemies)
		{
			if (enemy.active && !enemy.isDead && enemy.hp > 0)
			{
				++count;
			}
		}
		return count;
	}

	int EnemyBatchController::GetTotalEnemyCount() const
	{
		return static_cast<int>(m_Enemies.size());
	}
}


