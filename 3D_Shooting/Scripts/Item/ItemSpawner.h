/*!
@file ItemSpawner.h
@brief アイテムの出現位置抽選と補充管理
*/

#pragma once
#include "stdafx.h"
#include <memory>
#include <optional>
#include <random>
#include "Scripts/Item/ItemFactory.h"

namespace shooting {

	class Stage;

	/*!
	@brief アイテム生成候補を検証するための入力情報
	*/
	struct ItemSpawnPositionRequest
	{
		Vec3 candidatePosition = Vec3(0.0f, 0.0f, 0.0f);
		float clearanceRadius = 0.0f;
		float groundFootOffset = 0.35f;
	};

	/*!
	@brief アイテム生成候補をステージ上の有効な位置へ解決するインターフェース
	*/
	class ItemSpawnPositionResolver
	{
	public:
		virtual ~ItemSpawnPositionResolver() = default;

		/*!
		@brief アイテム生成候補を検証し、採用可能な位置へ補正する
		@param request 生成候補とアイテムの占有情報
		@return 採用可能な場合は地形表面に補正済みの位置
		*/
		virtual std::optional<Vec3> ResolveItemSpawnPosition(
			const ItemSpawnPositionRequest& request) const = 0;
	};

	/*!
	@brief アイテムの出現位置抽選と必要数までの補充を担当する

	GameStageは地形・壁・配置物を使った位置判定だけを提供し、
	アイテムの種類ごとの補充数や生成処理はこのクラスへ集約する。
	*/
	class ItemSpawner
	{
	public:
		ItemSpawner() = default;

		/*!
		@brief アイテムを配置するステージを設定する
		@param stage 配置先ステージ
		*/
		void SetStage(const std::shared_ptr<Stage>& stage);

		/*!
		@brief ステージ固有の生成位置解決処理を設定する
		@param resolver 生成候補を検証するインターフェース。所有権は持たない。
		*/
		void SetSpawnPositionResolver(const ItemSpawnPositionResolver* resolver);

		void CreateInitialItems();

		void ReplenishItems();

	private:
		struct ReplenishRule
		{
			ItemKind kind = ItemKind::HpRecovery;
			int targetCount = 0;
			Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
			int bombGiveCount = 0;
		};

		std::weak_ptr<Stage> m_stage;
		std::shared_ptr<ItemFactory> m_itemFactory;
		const ItemSpawnPositionResolver* m_spawnPositionResolver = nullptr;
		std::mt19937 m_random;

		void SeedRandom();
		void PrepareItemFactory();
		void ReplenishItems(const ReplenishRule& rule);
		std::optional<Vec3> FindSpawnPosition();
		bool HasActiveItemSpawnOverlap(const Vec3& position, float radius) const;
	};

}
