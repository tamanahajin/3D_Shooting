#include "stdafx.h"
#include "EnemyFactory.h"

namespace shooting {

	EnemyFactory::EnemyFactory(const std::shared_ptr<EnemyBatchController>& controller) :
		m_controller(controller),
		m_randomEngine(std::random_device{}())
	{
		m_StatusByKind[EnemyKind::Default] = EnemyStatus();
	}

	void EnemyFactory::SetController(const std::shared_ptr<EnemyBatchController>& controller)
	{
		m_controller = controller;
	}

	bool EnemyFactory::IsValid() const
	{
		return !m_controller.expired();
	}

	void EnemyFactory::SetStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_StatusByKind[kind] = status;
	}

	EnemyStatus EnemyFactory::GetStatus(EnemyKind kind) const
	{
		auto it = m_StatusByKind.find(kind);
		if (it != m_StatusByKind.end())
		{
			return it->second;
		}

		auto defaultIt = m_StatusByKind.find(EnemyKind::Default);
		if (defaultIt != m_StatusByKind.end())
		{
			return defaultIt->second;
		}

		return EnemyStatus();
	}
	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position) const
	{
		return CreateEnemy(kind, position, GetStatus(kind));
	}

	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position, const EnemyStatus& status) const
	{
		auto controller = m_controller.lock();
		if (!controller)
		{
			return static_cast<size_t>(-1);
		}

		// 敵の実体はEnemyBatchControllerの配列に追加する。
		// 今後、敵種別ごとのモデル差分が必要になったら、このswitchに分岐を追加する。
		switch (kind)
		{
		case EnemyKind::Default:
		default:
			return controller->AddEnemy(position, status);
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
		const EnemyStatus status = desc.overrideStatus ? desc.status : GetStatus(desc.kind);
		for (const auto& position : positions)
		{
			if (CreateEnemy(desc.kind, position, status) != static_cast<size_t>(-1))
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
