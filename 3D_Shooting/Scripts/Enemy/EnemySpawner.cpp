/*!
@file EnemySpawner.cpp
@brief 敵の生成位置抽選と分割生成キュー
*/

#include "stdafx.h"
#include "EnemySpawner.h"
#include <cmath>

namespace shooting {

	EnemySpawner::EnemySpawner() :
		m_randomEngine(std::random_device{}())
	{
		m_statusByKind[EnemyKind::Default] = EnemyStatus();
	}

	EnemySpawner::EnemySpawner(const std::shared_ptr<EnemyController>& controller) :
		EnemySpawner()
	{
		SetController(controller);
	}

	void EnemySpawner::SetController(const std::shared_ptr<EnemyController>& controller)
	{
		m_controller = controller;
		if (m_enemyFactory)
		{
			m_enemyFactory->SetController(controller);
		}
		PrepareEnemyFactory();
	}

	void EnemySpawner::SetSpawnPositionResolver(
		const std::shared_ptr<EnemySpawnPositionResolver>& resolver)
	{
		m_spawnPositionResolver = resolver;
	}

	bool EnemySpawner::IsValid() const
	{
		return !m_controller.expired() && m_enemyFactory && m_enemyFactory->IsValid();
	}

	void EnemySpawner::SetStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
		if (m_enemyFactory)
		{
			m_enemyFactory->SetStatus(kind, status);
		}
	}

	EnemyStatus EnemySpawner::GetStatus(EnemyKind kind) const
	{
		auto it = m_statusByKind.find(kind);
		if (it != m_statusByKind.end())
		{
			return it->second;
		}

		auto defaultIt = m_statusByKind.find(EnemyKind::Default);
		if (defaultIt != m_statusByKind.end())
		{
			return defaultIt->second;
		}

		return EnemyStatus();
	}

	int EnemySpawner::CreateEnemiesAround(const EnemyFactory::SpawnBatchDesc& desc)
	{
		if (desc.count <= 0)
		{
			return 0;
		}

		auto fixedDesc = MakeFixedStatusDesc(desc);
		std::vector<Vec3> acceptedPositions;
		acceptedPositions.reserve(static_cast<size_t>(fixedDesc.count));
		int processedCount = 0;
		return ProcessSpawnBatchStep(
			fixedDesc,
			fixedDesc.count,
			acceptedPositions,
			processedCount);
	}

	void EnemySpawner::QueueEnemies(const EnemyFactory::SpawnBatchDesc& desc)
	{
		if (desc.count <= 0)
		{
			return;
		}

		PendingSpawnBatch batch;
		batch.desc = MakeFixedStatusDesc(desc);
		batch.acceptedPositions.reserve(static_cast<size_t>(desc.count));
		m_pendingSpawnBatches.push_back(batch);
	}

	int EnemySpawner::ProcessPendingSpawns(int maxProcessCount)
	{
		if (maxProcessCount <= 0)
		{
			return 0;
		}

		PrepareEnemyFactory();
		if (!IsValid())
		{
			return 0;
		}

		int createdTotal = 0;
		int remainingBudget = maxProcessCount;
		while (remainingBudget > 0 && !m_pendingSpawnBatches.empty())
		{
			auto& batch = m_pendingSpawnBatches.front();
			const int processedBefore = batch.processedCount;
			const int createdCount = ProcessSpawnBatchStep(
				batch.desc,
				remainingBudget,
				batch.acceptedPositions,
				batch.processedCount);
			const int processedThisFrame = batch.processedCount - processedBefore;

			createdTotal += createdCount;
			if (processedThisFrame <= 0)
			{
				// Factoryやステージ参照が一時的に無効な場合、同じフレームで回り続けないようにする。
				break;
			}

			remainingBudget -= processedThisFrame;
			if (batch.processedCount >= batch.desc.count)
			{
				m_pendingSpawnBatches.pop_front();
			}
		}

		return createdTotal;
	}

	void EnemySpawner::ClearPendingSpawns()
	{
		m_pendingSpawnBatches.clear();
	}

	void EnemySpawner::PrepareEnemyFactory()
	{
		auto controller = m_controller.lock();
		if (!controller)
		{
			m_enemyFactory.reset();
			return;
		}

		if (!m_enemyFactory)
		{
			m_enemyFactory = std::make_shared<EnemyFactory>(controller);

			for (const auto& statusByKind : m_statusByKind)
			{
				m_enemyFactory->SetStatus(statusByKind.first, statusByKind.second);
			}
		}
	}

	EnemyFactory::SpawnBatchDesc EnemySpawner::MakeFixedStatusDesc(
		const EnemyFactory::SpawnBatchDesc& desc) const
	{
		auto fixedDesc = desc;
		if (!fixedDesc.overrideStatus)
		{
			// 分割生成中にデバッグUIなどで敵ステータスが変わっても、
			// 同じ生成バッチ内では開始時点の設定を使い続ける。
			fixedDesc.overrideStatus = true;
			fixedDesc.status = GetStatus(fixedDesc.kind);
		}
		return fixedDesc;
	}

	int EnemySpawner::ProcessSpawnBatchStep(
		const EnemyFactory::SpawnBatchDesc& desc,
		int maxProcessCount,
		std::vector<Vec3>& acceptedPositions,
		int& processedCount)
	{
		PrepareEnemyFactory();
		if (desc.count <= 0 || maxProcessCount <= 0 || !IsValid())
		{
			return 0;
		}

		if (processedCount < 0)
		{
			processedCount = 0;
		}
		if (processedCount >= desc.count)
		{
			return 0;
		}

		if (acceptedPositions.capacity() < static_cast<size_t>(desc.count))
		{
			acceptedPositions.reserve(static_cast<size_t>(desc.count));
		}

		const EnemyStatus status = desc.overrideStatus ? desc.status : GetStatus(desc.kind);
		int createdCount = 0;
		int processedThisStep = 0;
		while (processedCount < desc.count && processedThisStep < maxProcessCount)
		{
			// 生成候補をランダムに作る
			Vec3 position(desc.center.x, desc.settings.spawnY, desc.center.z);
			const bool foundPosition = TryFindSpawnPosition(
				desc,
				status,
				acceptedPositions,
				position);

			if (foundPosition &&
				m_enemyFactory->CreateEnemy(desc.kind, position, status) != static_cast<size_t>(-1))
			{
				acceptedPositions.push_back(position);
				++createdCount;
			}

			// 有効位置が見つからない敵も処理済みにし、混雑時に生成キューが停止し続けることを防ぐ。
			++processedCount;
			++processedThisStep;
		}

		return createdCount;
	}

	bool EnemySpawner::TryFindSpawnPosition(
		const EnemyFactory::SpawnBatchDesc& desc,
		const EnemyStatus& status,
		const std::vector<Vec3>& acceptedPositions,
		Vec3& outPosition)
	{
		// 同じウェーブの敵、地形、配置物のすべてを通過した候補だけを採用する。
		const int maxAttempts = desc.settings.maxAttempts > 0 ? desc.settings.maxAttempts : 1;
		for (int attempt = 0; attempt < maxAttempts; ++attempt)
		{
			const Vec3 candidate = CreateRandomPosition(desc.center, desc.settings);
			Vec3 resolvedPosition;
			if (!TryResolveSpawnPosition(candidate, status, resolvedPosition))
			{
				continue;
			}
			if (!IsFarEnough(resolvedPosition, acceptedPositions, desc.settings.minSpacing))
			{
				continue;
			}

			outPosition = resolvedPosition;
			return true;
		}

		return false;
	}

	bool EnemySpawner::TryResolveSpawnPosition(
		const Vec3& candidatePosition,
		const EnemyStatus& status,
		Vec3& outPosition) const
	{
		auto resolver = m_spawnPositionResolver.lock();
		if (!resolver)
		{
			outPosition = candidatePosition;
			return true;
		}

		EnemySpawnPositionRequest request;
		request.candidatePosition = candidatePosition;
		request.clearanceRadius = status.collisionRadius;
		request.groundFootOffset = status.groundFootOffset;
		return resolver->TryResolveEnemySpawnPosition(request, outPosition);
	}

	Vec3 EnemySpawner::CreateRandomPosition(
		const Vec3& center,
		const EnemyFactory::SpawnSettings& settings)
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
			center.x + (radius * std::cos(angle)),
			settings.spawnY,
			center.z + (radius * std::sin(angle)));
	}

	bool EnemySpawner::IsFarEnough(
		const Vec3& position,
		const std::vector<Vec3>& existingPositions,
		float minSpacing) const
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
