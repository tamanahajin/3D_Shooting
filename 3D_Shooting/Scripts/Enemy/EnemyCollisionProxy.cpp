#include "stdafx.h"
#include "Project.h"

namespace shooting {

	EnemyCollisionProxy::EnemyCollisionProxy(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status) :
		GameObject(stage),
		m_controller(controller),
		m_enemyIndex(enemyIndex),
		m_startPosition(startPosition),
		m_modelScale(status.modelScale),
		m_collisionRadius(status.collisionRadius),
		m_collisionHeight(status.collisionHeight),
		m_inUse(true)
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
		transform->SetPosition(m_startPosition);
		transform->SetScale(m_modelScale);
		transform->SetRotation(0.0f, 0.0f, 0.0f);

		auto collision = AddComponent<CollisionCapsule>();
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(m_collisionRadius);
		collision->SetMakedHeight(m_collisionHeight);
		collision->AddExcludeCollisionTag(L"Enemy");
		collision->AddExcludeCollisionTag(L"Floor");

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	void EnemyCollisionProxy::ResetForEnemy(
		const std::shared_ptr<EnemyController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status)
	{
		// 敵生成時にGameObjectとCollisionCapsuleを毎回作り直すとスパイクになりやすい。
		m_controller = controller;
		m_enemyIndex = enemyIndex;
		m_startPosition = startPosition;
		m_modelScale = status.modelScale;
		m_collisionRadius = status.collisionRadius;
		m_collisionHeight = status.collisionHeight;
		m_inUse = true;

		SetUpdateActive(true);
		SetDrawActive(false);
		SetShadowActive(false);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			transform->SetPosition(m_startPosition);
			transform->SetScale(m_modelScale);
			transform->SetRotation(0.0f, 0.0f, 0.0f);
			transform->SetToBefore();
		}

		auto collision = GetComponent<CollisionCapsule>(false);
		if (collision)
		{
			collision->SetUpdateActive(true);
			collision->SetDebugDraw(false);
			collision->SetMakedRadius(m_collisionRadius);
			collision->SetMakedHeight(m_collisionHeight);
			collision->WakeUp();
		}

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	void EnemyCollisionProxy::DeactivateForPool()
	{
		m_inUse = false;
		m_controller.reset();
		m_enemyIndex = 0;

		RemoveTag(L"Enemy");
		SetUpdateActive(false);
		SetDrawActive(false);
		SetShadowActive(false);

		if (auto collision = GetComponent<CollisionCapsule>(false))
		{
			// ステージ上に残したまま再利用するため、CollisionManagerの判定対象からだけ外す。
			collision->SetUpdateActive(false);
			collision->SetDrawActive(false);
		}
	}

	void EnemyCollisionProxy::HandleCollision(const CollisionPair& pair)
	{
		if (!m_inUse)
		{
			return;
		}

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
			auto controller = m_controller.lock();
			if (controller)
			{
				controller->NotifyGroundCollision(m_enemyIndex, pair);
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
		if (!m_inUse)
		{
			return false;
		}

		auto controller = m_controller.lock();
		return controller ? controller->ApplyDamage(m_enemyIndex, info) : false;
	}

	void EnemyCollisionProxy::AddKnockback(const Vec3& velocity)
	{
		if (!m_inUse)
		{
			return;
		}

		auto controller = m_controller.lock();
		if (controller)
		{
			controller->AddKnockback(m_enemyIndex, velocity);
		}
	}

	bool EnemyCollisionProxy::IsAlive() const
	{
		if (!m_inUse)
		{
			return false;
		}

		auto controller = m_controller.lock();
		return controller ? controller->IsEnemyAlive(m_enemyIndex) : false;
	}
}

