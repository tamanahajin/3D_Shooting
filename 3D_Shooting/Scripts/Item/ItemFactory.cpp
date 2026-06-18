#include "stdafx.h"
#include "Project.h"

namespace shooting {

	ItemFactory::ItemFactory(const std::shared_ptr<Stage>& stage) :
		m_stage(stage)
	{
	}

	void ItemFactory::SetStage(const std::shared_ptr<Stage>& stage)
	{
		m_stage = stage;
	}

	bool ItemFactory::IsValid() const
	{
		return !m_stage.expired();
	}

	std::shared_ptr<BaseItem> ItemFactory::CreateItem(const SpawnDesc& desc) const
	{
		auto stage = m_stage.lock();
		if (!stage)
		{
			return nullptr;
		}

		TransParam itemParam;
		itemParam.position = desc.position;
		itemParam.quaternion = desc.rotation;
		itemParam.scale = desc.scale;

		// アイテム種類
		switch (desc.kind)
		{
		case ItemKind::Bomb:
			return stage->AddGameObject<BombItem>(itemParam, desc.bombGiveCount);
		case ItemKind::HpRecovery:
		default:
			return stage->AddGameObject<HpRecoveryItem>(itemParam, desc.healRate);
		}
	}

	int ItemFactory::CountActiveItems(ItemKind kind) const
	{
		auto stage = m_stage.lock();
		if (!stage)
		{
			return 0;
		}

		int count = 0;
		for (const auto& obj : stage->GetGameObjectVec())
		{
			auto item = std::dynamic_pointer_cast<BaseItem>(obj);
			if (!item || item->IsConsumed() || !item->IsUpdateActive())
			{
				continue;
			}

			if (MatchesKind(item, kind))
			{
				++count;
			}
		}
		return count;
	}

	bool ItemFactory::MatchesKind(const std::shared_ptr<BaseItem>& item, ItemKind kind) const
	{
		switch (kind)
		{
		case ItemKind::Bomb:
			return std::dynamic_pointer_cast<BombItem>(item) != nullptr;
		case ItemKind::HpRecovery:
		default:
			return std::dynamic_pointer_cast<HpRecoveryItem>(item) != nullptr;
		}
	}

}