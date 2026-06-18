/*!
@file ItemFactory.h
@brief どの Item クラスを作るかを担当
@brief 種類が増えたら、FactoryMethodパターンにしてもいいかも
*/
#pragma once
#include "stdafx.h"
#include <memory>


namespace shooting {

	class BaseItem;
	class Stage;

	// アイテムの種類。新しいアイテムを増やすときはここを入口に分岐させる。
	enum class ItemKind
	{
		HpRecovery,
		Bomb
	};

	// アイテム生成の責務を持つクラス。
	// GameStageは「どこに・何個維持するか」だけを決め、具体的なアイテムクラスの選択はここに集約する。
	class ItemFactory
	{
	public:
		// 1個のアイテム生成に必要な情報。
		// 種類ごとの固有パラメータは、該当するItemKindのときだけ使用する。
		struct SpawnDesc
		{
			ItemKind kind = ItemKind::HpRecovery;
			Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
			Quat rotation = Quat();
			Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
			float healRate = 0.25f;
			int bombGiveCount = 5;
		};

		explicit ItemFactory(const std::shared_ptr<Stage>& stage);

		void SetStage(const std::shared_ptr<Stage>& stage);
		bool IsValid() const;

		// アイテム1個を生成する。種類ごとの差分はこの関数に集める。
		std::shared_ptr<BaseItem> CreateItem(const SpawnDesc& desc) const;

		// 指定した種類の、まだ取得されていない有効アイテム数を返す。
		int CountActiveItems(ItemKind kind) const;

	private:
		std::weak_ptr<Stage> m_stage;

		bool MatchesKind(const std::shared_ptr<BaseItem>& item, ItemKind kind) const;
	};

}