#include "stdafx.h"
#include "Project.h"

namespace shooting {

	BaseItem::BaseItem(
		const std::shared_ptr<Stage>& stagePtr,
		const TransParam& param) :
		GameObject(stagePtr),
		m_BasePosition(param.position)
	{
		m_transParam = param;
	}

	void BaseItem::OnCreate()
	{
		AddTag(L"Item");

		SetAlphaActive(false);
		SetShadowActive(false);

		auto collision = AddComponent<CollisionSphere>();
		collision->SetFixed(true);
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(1.0f);
		collision->SetAfterCollision(AfterCollision::None);

		OnCreateItem();
	}

	void BaseItem::OnUpdate(double elapsedTime)
	{
		m_Time += static_cast<float>(elapsedTime);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			Vec3 position = m_BasePosition;
			position.y += std::sin(m_Time * 2.2f) * 0.05f;
			transform->SetPosition(position);
			transform->SetRotation(0.0f, m_Time * 1.6f, 0.0f);
		}

		OnUpdateItem(elapsedTime);
	}

	void BaseItem::OnCollisionEnter(const CollisionPair& pair)
	{
		TryPickup(pair);
	}

	void BaseItem::OnCollisionExecute(const CollisionPair& pair)
	{
		TryPickup(pair);
	}

	void BaseItem::TryPickup(const CollisionPair& pair)
	{
		auto otherCollision = pair.m_Dest.lock();
		if (!otherCollision)
		{
			return;
		}

		TryPickupBy(otherCollision->GetGameObject());
	}

	bool BaseItem::TryPickupBy(const std::shared_ptr<GameObject>& collector)
	{
		if (m_Consumed || !CanPickupBy(collector))
		{
			return false;
		}

		if (!ApplyItemEffect(collector))
		{
			return false;
		}

		Consume();
		return true;
	}

	bool BaseItem::CanPickupBy(const std::shared_ptr<GameObject>& collector) const
	{
		return collector && collector->FindTag(L"Player");
	}

	void BaseItem::Consume()
	{
		m_Consumed = true;
		SetDrawActive(false);
		SetUpdateActive(false);
		SetShadowActive(false);

		if (auto collision = GetComponent<Collision>(false))
		{
			collision->SetUpdateActive(false);
			collision->SetDrawActive(false);
		}

		if (auto stage = GetStage(false))
		{
			stage->RemoveGameObject(GetThis<GameObject>());
		}
	}

	HpRecoveryItem::HpRecoveryItem(
		const std::shared_ptr<Stage>& stagePtr,
		const TransParam& param,
		float healRate) :
		BaseItem(stagePtr, param),
		m_HealRate(healRate)
	{
	}

	void HpRecoveryItem::OnCreateItem()
	{
		AddTag(L"HpRecoveryItem");

		auto draw = AddComponent<BcPNTStaticDraw>();
		draw->AddBaseMesh(L"DEFAULT_SPHERE");
		draw->SetDiffuseColor(Col4(0.15f, 0.95f, 0.35f, 1.0f));
		draw->SetLightingEnabled(false);
		draw->SetFogEnabled(false);
		draw->SetOwnShadowActive(false);
	}

	bool HpRecoveryItem::ApplyItemEffect(const std::shared_ptr<GameObject>& collector)
	{
		auto health = collector->GetComponent<Health>(false);
		if (!health)
		{
			return false;
		}

		int healAmount = static_cast<int>(std::ceil(static_cast<float>(health->GetMaxHP()) * m_HealRate));
		if (healAmount < 1)
		{
			healAmount = 1;
		}

		return health->Heal(healAmount);
	}

}
