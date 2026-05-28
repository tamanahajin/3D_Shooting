/*!
@file EnemyCollisionProxy.cpp
@brief 敵1体分の軽量コリジョンプロキシ

敵の本体状態はEnemyBatchControllerの配列で持つ。
ここでは衝突イベントだけを受けて、対象indexの敵へダメージや接地情報を転送する。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	EnemyCollisionProxy::EnemyCollisionProxy(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyBatchController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status) :
		GameObject(stage),
		m_Controller(controller),
		m_EnemyIndex(enemyIndex),
		m_StartPosition(startPosition),
		m_ModelScale(status.modelScale),
		m_CollisionRadius(status.collisionRadius),
		m_CollisionHeight(status.collisionHeight)
	{
		m_transParam.position = startPosition;
	}

	EnemyCollisionProxy::~EnemyCollisionProxy() {}

	void EnemyCollisionProxy::OnCreate()
	{
		SetBatchUpdateManaged(true);
		SetDrawActive(false);
		SetShadowActive(false);

		auto transform = GetComponent<Transform>();
		transform->SetPosition(m_StartPosition);
		transform->SetScale(m_ModelScale);
		transform->SetRotation(0.0f, 0.0f, 0.0f);

		auto collision = AddComponent<CollisionCapsule>();
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(m_CollisionRadius);
		collision->SetMakedHeight(m_CollisionHeight);
		collision->AddExcludeCollisionTag(L"Enemy");
		collision->AddExcludeCollisionTag(L"Floor");

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	void EnemyCollisionProxy::HandleCollision(const CollisionPair& pair)
	{
		auto otherCollision = pair.m_Dest.lock();
		if (!otherCollision)
		{
			return;
		}

		auto otherObject = otherCollision->GetGameObject();
		if (!otherObject)
		{
			return;
		}

		if (otherObject->FindTag(L"Floor"))
		{
			auto controller = m_Controller.lock();
			if (controller)
			{
				controller->NotifyGroundCollision(m_EnemyIndex, pair);
			}
			return;
		}

		if (!otherObject->FindTag(L"Bullet") && !otherObject->FindTag(L"Bomb"))
		{
			return;
		}

		auto bulletBase = std::dynamic_pointer_cast<IBullet>(otherObject);
		if (bulletBase && !bulletBase->IsActive())
		{
			return;
		}

		if (auto bomb = std::dynamic_pointer_cast<BombBullet>(otherObject))
		{
			CollisionPair swapped = pair;
			swapped.m_Src = pair.m_Dest;
			swapped.m_Dest = pair.m_Src;
			bomb->OnCollisionEnter(swapped);
			return;
		}

		auto damageDealer = otherObject->GetComponent<DamageDealer>(false);
		if (!damageDealer)
		{
			return;
		}

		DamageInfo info;
		info.m_Damage = damageDealer->GetDamage();
		info.m_Instigator = otherObject;
		ApplyDamage(info);

		if (damageDealer->DestroyOnHit() && bulletBase)
		{
			bulletBase->SetActive(false);
		}
	}

	void EnemyCollisionProxy::OnCollisionEnter(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	void EnemyCollisionProxy::OnCollisionExecute(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	bool EnemyCollisionProxy::ApplyDamage(const DamageInfo& info)
	{
		auto controller = m_Controller.lock();
		return controller ? controller->ApplyDamage(m_EnemyIndex, info) : false;
	}

	void EnemyCollisionProxy::AddKnockback(const Vec3& velocity)
	{
		auto controller = m_Controller.lock();
		if (controller)
		{
			controller->AddKnockback(m_EnemyIndex, velocity);
		}
	}

	bool EnemyCollisionProxy::IsAlive() const
	{
		auto controller = m_Controller.lock();
		return controller ? controller->IsEnemyAlive(m_EnemyIndex) : false;
	}
}

