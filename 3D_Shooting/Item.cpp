#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const wchar_t* kHpRecoveryModelKey = L"HP_RECOVERY_ITEM_MODEL";
		const wchar_t* kHpRecoveryMaterialPrefix = L"HP_RECOVERY_ITEM_MAT_";
		const Col4 kHpRecoveryItemColor(1.0f, 0.05f, 0.05f, 1.0f);
		const wchar_t* kBombItemModelKey = L"BOMB_MODEL";
		const wchar_t* kBombItemMaterialPrefix = L"BOMB_MAT_";
		const Col4 kBombItemFallbackColor(0.9f, 0.9f, 0.9f, 1.0f);
	}

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
		collision->SetFixed(false);
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

		GameAudio::Instance().PlaySound(GameSoundId::ItemPickup);

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

		const auto& meshes = BaseScene::Get()->GetModelMesh(kHpRecoveryModelKey);

		auto shadow = AddComponent<ShadowMap>();
		if (!meshes.empty())
		{
			shadow->AddBaseMesh(meshes[0]);
		}
		else
		{
			shadow->AddBaseMesh(L"DEFAULT_SPHERE");
		}

		auto draw = AddComponent<BcPNTStaticDraw>();
		draw->SetDiffuseColor(kHpRecoveryItemColor);
		draw->SetLightingEnabled(true);
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(true);

		if (!meshes.empty())
		{
			draw->AddBaseModelMesh(meshes);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				draw->AddBaseMaterial(std::wstring(kHpRecoveryMaterialPrefix) + std::to_wstring(i));
			}
		}
		else
		{
			draw->AddBaseMesh(L"DEFAULT_SPHERE");
			draw->SetDiffuseColor(kHpRecoveryItemColor);
			draw->SetLightingEnabled(false);
			draw->SetFogEnabled(false);
			draw->SetOwnShadowActive(false);
		}

		auto collision = GetComponent<CollisionSphere>();
		collision->SetMakedRadius(3.0f);
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

	BombItem::BombItem(
		const std::shared_ptr<Stage>& stagePtr,
		const TransParam& param,
		int bombGrantCount) :
		BaseItem(stagePtr, param),
		m_BombGrantCount(bombGrantCount)
	{
	}

	void BombItem::OnCreateItem()
	{
		AddTag(L"BombItem");

		const auto& meshes = BaseScene::Get()->GetModelMesh(kBombItemModelKey);

		auto shadow = AddComponent<ShadowMap>();
		if (!meshes.empty())
		{
			shadow->AddBaseMesh(meshes[0]);
		}
		else
		{
			shadow->AddBaseMesh(L"DEFAULT_SPHERE");
		}

		auto draw = AddComponent<BcPNTStaticDraw>();
		draw->SetLightingEnabled(true);
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(true);

		if (!meshes.empty())
		{
			draw->AddBaseModelMesh(meshes);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				draw->AddBaseMaterial(std::wstring(kBombItemMaterialPrefix) + std::to_wstring(i));
			}
		}
		else
		{
			draw->AddBaseMesh(L"DEFAULT_SPHERE");
			draw->SetDiffuseColor(kBombItemFallbackColor);
			draw->SetLightingEnabled(false);
			draw->SetFogEnabled(false);
			draw->SetOwnShadowActive(false);
		}

		auto collision = GetComponent<CollisionSphere>();
		collision->SetMakedRadius(15.0f);
	}

	bool BombItem::ApplyItemEffect(const std::shared_ptr<GameObject>& collector)
	{
		auto player = std::dynamic_pointer_cast<Player>(collector);
		if (!player || m_BombGrantCount <= 0)
		{
			return false;
		}

		player->AddBombAmmo(m_BombGrantCount);
		return true;
	}

}
