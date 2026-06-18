/*!
@file ItemSpawner.cpp
@brief アイテムの出現位置抽選と補充管理
*/

#include "stdafx.h"
#include "Project.h"
#include <cmath>

namespace shooting {

	namespace
	{
		const int kRecoveryItemTargetCount = 3;
		const float kHpRecoveryItemScale = 0.08f;
		const float kHpRecoveryItemDepthScale = 0.04f;
		const int kBombItemTargetCount = 2;
		const float kBombItemScale = 0.018f;
		const int kBombItemGiveCount = 5;
		const int kItemMaxSpawnAttempts = 240;
		const float kItemSpawnHalf = 24.0f;
		const float kItemSpawnRadius = 1.25f;
		const float kItemGroundOffset = 0.35f;

		enum class ItemSpawnPattern
		{
			WideArea,
			CenterRing,
			OuterRing,
			CrossLane,
			Diagonal,
			CornerPocket,
			Count
		};

		float RandomRange(std::mt19937& gen, float minValue, float maxValue)
		{
			std::uniform_real_distribution<float> dist(minValue, maxValue);
			return dist(gen);
		}

		float RandomSign(std::mt19937& gen)
		{
			std::uniform_int_distribution<int> dist(0, 1);
			return dist(gen) == 0 ? -1.0f : 1.0f;
		}

		float ClampItemSpawnCoord(float value)
		{
			if (value < -kItemSpawnHalf)
			{
				return -kItemSpawnHalf;
			}
			if (value > kItemSpawnHalf)
			{
				return kItemSpawnHalf;
			}
			return value;
		}

		Vec3 MakeItemSpawnCandidate(ItemSpawnPattern pattern, std::mt19937& gen)
		{
			switch (pattern)
			{
			case ItemSpawnPattern::CenterRing:
			{
				// 中央から少し離れたリング。プレイヤー初期位置を避けつつ、拾いに行きやすい範囲に出す。
				const float angle = RandomRange(gen, 0.0f, XM_2PI);
				const float radius = RandomRange(gen, 7.0f, 14.5f);
				return Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
			}
			case ItemSpawnPattern::OuterRing:
			{
				// 外周寄り。敵から逃げた先にも補給が出るように、端の近くを狙う。
				const float angle = RandomRange(gen, 0.0f, XM_2PI);
				const float radius = RandomRange(gen, 15.5f, 22.5f);
				return Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
			}
			case ItemSpawnPattern::CrossLane:
			{
				// X/Z方向の移動ライン上。完全ランダムだけだと偏るため、通り道にも候補を作る。
				const bool horizontal = RandomSign(gen) > 0.0f;
				const float longPos = RandomRange(gen, -kItemSpawnHalf * 0.9f, kItemSpawnHalf * 0.9f);
				const float laneOffset = RandomRange(gen, -4.0f, 4.0f);
				return horizontal
					? Vec3(longPos, 0.0f, laneOffset)
					: Vec3(laneOffset, 0.0f, longPos);
			}
			case ItemSpawnPattern::Diagonal:
			{
				// 対角線寄り。ステージを斜めに横切る時にも拾える位置を増やす。
				const float t = RandomRange(gen, -kItemSpawnHalf * 0.85f, kItemSpawnHalf * 0.85f);
				const float offset = RandomRange(gen, -3.5f, 3.5f);
				const bool rising = RandomSign(gen) > 0.0f;
				const float z = rising ? t + offset : -t + offset;
				return Vec3(ClampItemSpawnCoord(t), 0.0f, ClampItemSpawnCoord(z));
			}
			case ItemSpawnPattern::CornerPocket:
			{
				// 四隅寄り。外周リングだけでは角に出にくいので、角付近の候補を明示的に作る。
				const float x = RandomSign(gen) * RandomRange(gen, kItemSpawnHalf * 0.55f, kItemSpawnHalf * 0.92f);
				const float z = RandomSign(gen) * RandomRange(gen, kItemSpawnHalf * 0.55f, kItemSpawnHalf * 0.92f);
				return Vec3(x, 0.0f, z);
			}
			case ItemSpawnPattern::WideArea:
			default:
				// 従来と同じ正方形ランダム。追加パターンが失敗した時の汎用候補にもなる。
				return Vec3(
					RandomRange(gen, -kItemSpawnHalf, kItemSpawnHalf),
					0.0f,
					RandomRange(gen, -kItemSpawnHalf, kItemSpawnHalf));
			}
		}
	}

	void ItemSpawner::SetStage(const std::shared_ptr<Stage>& stage)
	{
		m_stage = stage;
		if (m_itemFactory)
		{
			m_itemFactory->SetStage(stage);
		}
	}

	void ItemSpawner::SetSpawnPositionResolver(const ItemSpawnPositionResolver* resolver)
	{
		m_spawnPositionResolver = resolver;
	}

	void ItemSpawner::CreateInitialItems()
	{
		SeedRandom();
		ReplenishItems();
	}

	void ItemSpawner::ReplenishItems()
	{
		const ReplenishRule recoveryRule{
			ItemKind::HpRecovery,
			kRecoveryItemTargetCount,
			Vec3(kHpRecoveryItemScale, kHpRecoveryItemScale, kHpRecoveryItemDepthScale)
		};
		const ReplenishRule bombRule{
			ItemKind::Bomb,
			kBombItemTargetCount,
			Vec3(kBombItemScale, kBombItemScale, kBombItemScale),
			kBombItemGiveCount
		};

		ReplenishItems(recoveryRule);
		ReplenishItems(bombRule);
	}

	void ItemSpawner::SeedRandom()
	{
		std::random_device seedSource;
		std::seed_seq seed{
			seedSource(),
			seedSource(),
			seedSource(),
			seedSource()
		};
		m_random.seed(seed);
	}

	void ItemSpawner::PrepareItemFactory()
	{
		auto stage = m_stage.lock();
		if (!stage)
		{
			m_itemFactory.reset();
			return;
		}

		if (!m_itemFactory)
		{
			m_itemFactory = std::make_shared<ItemFactory>(stage);
		}
	}

	void ItemSpawner::ReplenishItems(const ReplenishRule& rule)
	{
		PrepareItemFactory();
		if (!m_itemFactory || !m_itemFactory->IsValid())
		{
			return;
		}

		int activeCount = m_itemFactory->CountActiveItems(rule.kind);
		while (activeCount < rule.targetCount)
		{
			auto spawnPosition = FindSpawnPosition();
			if (!spawnPosition)
			{
				break;
			}

			ItemFactory::SpawnDesc spawnDesc;
			spawnDesc.kind = rule.kind;
			spawnDesc.position = *spawnPosition;
			spawnDesc.scale = rule.scale;
			spawnDesc.bombGiveCount = rule.bombGiveCount;

			if (!m_itemFactory->CreateItem(spawnDesc))
			{
				break;
			}

			++activeCount;
		}
	}

	std::optional<Vec3> ItemSpawner::FindSpawnPosition()
	{
		if (!m_spawnPositionResolver)
		{
			return std::nullopt;
		}

		const int patternCount = static_cast<int>(ItemSpawnPattern::Count);
		std::uniform_int_distribution<int> patternOffsetDist(0, patternCount - 1);
		const int patternOffset = patternOffsetDist(m_random);

		for (int attempt = 0; attempt < kItemMaxSpawnAttempts; ++attempt)
		{
			// 毎回同じパターン順にすると配置が固定化しやすいので、開始位置だけ乱数でずらす。
			const auto pattern = static_cast<ItemSpawnPattern>((attempt + patternOffset) % patternCount);
			Vec3 candidate = MakeItemSpawnCandidate(pattern, m_random);

			ItemSpawnPositionRequest request;
			request.candidatePosition = candidate;
			request.clearanceRadius = kItemSpawnRadius;
			request.groundFootOffset = kItemGroundOffset;

			auto resolvedPosition = m_spawnPositionResolver->ResolveItemSpawnPosition(request);
			if (!resolvedPosition)
			{
				continue;
			}

			if (HasActiveItemSpawnOverlap(*resolvedPosition, kItemSpawnRadius))
			{
				continue;
			}

			return resolvedPosition;
		}

		return std::nullopt;
	}

	bool ItemSpawner::HasActiveItemSpawnOverlap(const Vec3& position, float radius) const
	{
		auto stage = m_stage.lock();
		if (!stage)
		{
			return false;
		}

		auto overlaps = [&](const Vec3& otherPosition, float otherRadius)
		{
			const float dx = position.x - otherPosition.x;
			const float dz = position.z - otherPosition.z;
			const float minDistance = radius + otherRadius;
			return (dx * dx + dz * dz) < (minDistance * minDistance);
		};

		for (const auto& obj : stage->GetGameObjectVec())
		{
			auto item = std::dynamic_pointer_cast<BaseItem>(obj);
			if (!item || item->IsConsumed())
			{
				continue;
			}

			auto transform = item->GetComponent<Transform>(false);
			if (transform && overlaps(transform->GetWorldPosition(), kItemSpawnRadius))
			{
				return true;
			}
		}

		return false;
	}

}
