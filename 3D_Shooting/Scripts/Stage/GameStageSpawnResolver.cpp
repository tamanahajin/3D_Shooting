/*!
@file GameStageSpawnResolver.cpp
@brief GameStage上の敵・アイテム生成位置解決
*/

#include "stdafx.h"
#include "Project.h"
#include <limits>

namespace shooting {

	void GameStage::ClearStageSpawnBlockers()
	{
		m_stageSpawnBlockers.clear();
	}

	void GameStage::AddStageSpawnBlocker(
		const Vec3& position,
		float radius,
		bool blocksItems,
		bool blocksEnemies)
	{
		if (radius <= 0.0f || (!blocksItems && !blocksEnemies))
		{
			return;
		}

		StageSpawnBlocker blocker;
		blocker.position = position;
		blocker.radius = radius;
		blocker.blocksItems = blocksItems;
		blocker.blocksEnemies = blocksEnemies;
		m_stageSpawnBlockers.push_back(blocker);
	}

	bool GameStage::IsStageSpawnPositionFree(
		const Vec3& position,
		float radius,
		StageSpawnTarget target) const
	{
		for (const auto& blocker : m_stageSpawnBlockers)
		{
			const bool blocksTarget =
				target == StageSpawnTarget::Item
				? blocker.blocksItems
				: blocker.blocksEnemies;
			if (!blocksTarget)
			{
				continue;
			}

			const float dx = position.x - blocker.position.x;
			const float dz = position.z - blocker.position.z;
			const float minDistance = radius + blocker.radius;
			if ((dx * dx + dz * dz) < (minDistance * minDistance))
			{
				return false;
			}
		}
		return true;
	}

	bool GameStage::TryResolveStageSpawnGroundHeight(
		const Vec3& position,
		float clearanceRadius,
		float& outHeight) const
	{
		const float sampleRadius = clearanceRadius > 0.05f ? clearanceRadius : 0.05f;
		const float diagonalOffset = sampleRadius * 0.70710678f;
		const Vec3 sampleOffsets[] =
		{
			Vec3(0.0f, 0.0f, 0.0f),
			Vec3(sampleRadius, 0.0f, 0.0f),
			Vec3(-sampleRadius, 0.0f, 0.0f),
			Vec3(0.0f, 0.0f, sampleRadius),
			Vec3(0.0f, 0.0f, -sampleRadius),
			Vec3(diagonalOffset, 0.0f, diagonalOffset),
			Vec3(-diagonalOffset, 0.0f, diagonalOffset),
			Vec3(diagonalOffset, 0.0f, -diagonalOffset),
			Vec3(-diagonalOffset, 0.0f, -diagonalOffset),
		};

		float centerHeight = 0.0f;
		float minHeight = (std::numeric_limits<float>::max)();
		float maxHeight = -(std::numeric_limits<float>::max)();
		for (size_t i = 0; i < _countof(sampleOffsets); ++i)
		{
			const Vec3 samplePosition = position + sampleOffsets[i];
			float sampleHeight = 0.0f;
			TryGetSlopeGroundHeight(samplePosition, sampleHeight);
			if (i == 0)
			{
				centerHeight = sampleHeight;
			}
			if (sampleHeight < minHeight)
			{
				minHeight = sampleHeight;
			}
			if (sampleHeight > maxHeight)
			{
				maxHeight = sampleHeight;
			}
		}

		// 坂の連続した高さ変化は許可し、高台の端や側面のような急な高低差だけを棄却する。
		// 生成対象の足元全体を確認することで、中心点だけが表面にある状態で内部へ食い込むのを防ぐ。
		const float maxFootprintHeightDifference = 0.75f;
		if (maxHeight - minHeight > maxFootprintHeightDifference)
		{
			return false;
		}

		outHeight = centerHeight;
		return true;
	}

	bool GameStage::IsInsideStageSpawnBounds(const Vec3& position, float radius) const
	{
		if (!m_stageSpawnBounds.valid)
		{
			return true;
		}

		// 敵の中心だけで判定すると、中心が内側でもカプセル側面が壁へ埋まる。
		// 境界を敵半径ぶん狭め、敵全体が外周壁の内側へ収まることを保証する。
		return position.x >= m_stageSpawnBounds.minX + radius &&
			position.x <= m_stageSpawnBounds.maxX - radius &&
			position.z >= m_stageSpawnBounds.minZ + radius &&
			position.z <= m_stageSpawnBounds.maxZ - radius;
	}

	std::optional<Vec3> GameStage::ResolveStageSpawnPosition(
		const StageSpawnPositionRequest& request) const
	{
		// 生成候補を「地形表面に乗る位置」へ補正してから、
		// 外周壁の内側とステージ配置物の占有範囲を確認する。
		if (!bsmUtil::IsFiniteVec3(request.candidatePosition) ||
			request.clearanceRadius < 0.0f ||
			request.groundFootOffset < 0.0f)
		{
			return std::nullopt;
		}

		float groundHeight = 0.0f;
		if (!TryResolveStageSpawnGroundHeight(
				request.candidatePosition,
				request.clearanceRadius,
				groundHeight))
		{
			return std::nullopt;
		}

		Vec3 resolvedPosition = request.candidatePosition;
		resolvedPosition.y = groundHeight + request.groundFootOffset;
		if (!IsInsideStageSpawnBounds(resolvedPosition, request.clearanceRadius))
		{
			return std::nullopt;
		}
		if (!IsStageSpawnPositionFree(
				resolvedPosition,
				request.clearanceRadius,
				request.target))
		{
			return std::nullopt;
		}

		return resolvedPosition;
	}

	bool GameStage::TryResolveEnemySpawnPosition(
		const EnemySpawnPositionRequest& request,
		Vec3& outPosition) const
	{
		StageSpawnPositionRequest stageRequest;
		stageRequest.candidatePosition = request.candidatePosition;
		stageRequest.clearanceRadius = request.clearanceRadius;
		stageRequest.groundFootOffset = request.groundFootOffset;
		stageRequest.target = StageSpawnTarget::Enemy;

		if (auto resolvedPosition = ResolveStageSpawnPosition(stageRequest))
		{
			outPosition = *resolvedPosition;
			return true;
		}

		return false;
	}

	std::optional<Vec3> GameStage::ResolveItemSpawnPosition(
		const ItemSpawnPositionRequest& request) const
	{
		StageSpawnPositionRequest stageRequest;
		stageRequest.candidatePosition = request.candidatePosition;
		stageRequest.clearanceRadius = request.clearanceRadius;
		stageRequest.groundFootOffset = request.groundFootOffset;
		stageRequest.target = StageSpawnTarget::Item;

		return ResolveStageSpawnPosition(stageRequest);
	}

}
