#include "stdafx.h"
#include "EnemyFactory.h"
#include "Character.h"

namespace shooting {

	EnemyFactory::EnemyFactory(const std::shared_ptr<EnemyBatchController>& controller) :
		m_controller(controller),
		m_randomEngine(std::random_device{}())
	{
	}

	void EnemyFactory::SetController(const std::shared_ptr<EnemyBatchController>& controller)
	{
		m_controller = controller;
	}

	bool EnemyFactory::IsValid() const
	{
		return !m_controller.expired();
	}

	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position) const
	{
		auto controller = m_controller.lock();
		if (!controller)
		{
			return static_cast<size_t>(-1);
		}

		// 敵の実体はEnemyBatchControllerの配列に追加する。
		// 今後、敵種別ごとの初期HPやモデル差分が必要になったら、このswitchに分岐を追加する。
		switch (kind)
		{
		case EnemyKind::Default:
		default:
			return controller->AddEnemy(position);
		}
	}

	int EnemyFactory::CreateEnemiesAround(const SpawnBatchDesc& desc)
	{
		if (desc.count <= 0 || !IsValid())
		{
			return 0;
		}

		// 同じウェーブで生成する敵同士が重なりにくいよう、先に全スポーン位置を決める。
		std::vector<Vec3> positions;
		positions.reserve(static_cast<size_t>(desc.count));

		const int maxAttempts = desc.settings.maxAttempts > 0 ? desc.settings.maxAttempts : 1;
		for (int count = 0; count < desc.count; ++count)
		{
			Vec3 position(desc.center.x, desc.settings.spawnY, desc.center.z);

			for (int attempt = 0; attempt < maxAttempts; ++attempt)
			{
				position = CreateRandomPosition(desc.center, desc.settings);
				if (IsFarEnough(position, positions, desc.settings.minSpacing))
				{
					break;
				}
			}

			positions.push_back(position);
		}

		int createdCount = 0;
		for (const auto& position : positions)
		{
			if (CreateEnemy(desc.kind, position) != static_cast<size_t>(-1))
			{
				++createdCount;
			}
		}

		return createdCount;
	}

	Vec3 EnemyFactory::CreateRandomPosition(const Vec3& center, const SpawnSettings& settings)
	{
		// min/maxが逆に設定されても生成できるように、ここで正規化する。
		const float minDistance = settings.minDistance < settings.maxDistance ?
			settings.minDistance : settings.maxDistance;
		const float maxDistance = settings.minDistance < settings.maxDistance ?
			settings.maxDistance : settings.minDistance;

		std::uniform_real_distribution<float> distRadius(minDistance, maxDistance);
		std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);

		const float radius = distRadius(m_randomEngine);
		const float angle = distAngle(m_randomEngine);
		return Vec3(
			center.x + (radius * cosf(angle)),
			settings.spawnY,
			center.z + (radius * sinf(angle)));
	}

	bool EnemyFactory::IsFarEnough(const Vec3& position, const std::vector<Vec3>& existingPositions, float minSpacing) const
	{
		if (minSpacing <= 0.0f)
		{
			return true;
		}

		const float minSpacingSq = minSpacing * minSpacing;
		for (const auto& existingPosition : existingPositions)
		{
			if ((position - existingPosition).lengthSqr() < minSpacingSq)
			{
				return false;
			}
		}
		return true;
	}

}