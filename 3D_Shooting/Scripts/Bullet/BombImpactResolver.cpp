/*!
@file BombImpactResolver.cpp
@brief BombImpactResolverの実装
*/

#include "stdafx.h"
#include "Project.h"
#include "BombImpactResolver.h"

namespace shooting {

	namespace
	{
		constexpr float kSurfaceProbeHeight = 5.0f;
		constexpr float kSurfaceProbeMinDistance = 20.0f;
		constexpr float kSurfaceProbeExtraDistance = 20.0f;
		constexpr float kSurfaceProbeRadius = 0.1f;
		constexpr float kGroundImpactTolerance = 0.08f;
	}

	BombImpactSurface BombImpactResolver::ResolveTargetSurface(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<GameObject>& ignoredObject,
		const Vec3& start,
		const Vec3& target,
		const Vec3& hitNormal,
		bool hasHit)
	{
		BombImpactSurface result;
		result.position = target;

		if (hasHit)
		{
			result.normal = SafeNormalize(hitNormal);
			result.hasSurface = true;
			return result;
		}

		float groundY = 0.0f;
		if (TryGetStageGroundHeight(stage, target, groundY))
		{
			result.position.y = groundY;
			result.normal = Vec3(0.0f, 1.0f, 0.0f);
			result.hasSurface = true;
			return result;
		}

		auto collisionManager = stage ? stage->GetCollisionManager() : nullptr;
		if (!collisionManager)
		{
			return result;
		}

		const float probeDistance = bsmUtil::Max(
			kSurfaceProbeMinDistance,
			std::fabs(start.y - target.y) + kSurfaceProbeExtraDistance);
		const Vec3 probeStart(
			target.x,
			bsmUtil::Max(start.y, target.y) + kSurfaceProbeHeight,
			target.z);

		RaycastHit hit;
		if (collisionManager->SphereCast(
			probeStart,
			Vec3(0.0f, -1.0f, 0.0f),
			probeDistance,
			kSurfaceProbeRadius,
			hit,
			ignoredObject,
			{ L"Bullet", L"Enemy", L"Item" }))
		{
			result.position = hit.m_Point;
			result.normal = SafeNormalize(hit.m_Normal);
			result.hasSurface = true;
		}

		return result;
	}

	bool BombImpactResolver::ShouldCheckGeneratedGroundImpact(
		bool hasTargetSurface,
		const Vec3& targetNormal) noexcept
	{
		return !hasTargetSurface || targetNormal.y > 0.45f;
	}

	bool BombImpactResolver::TryResolveGeneratedGroundImpact(
		const std::shared_ptr<Stage>& stage,
		const Vec3& previousPosition,
		const Vec3& currentPosition,
		Vec3& outImpactPosition) noexcept
	{
		float currentGroundY = 0.0f;
		if (!TryGetStageGroundHeight(stage, currentPosition, currentGroundY))
		{
			return false;
		}

		float previousGroundY = currentGroundY;
		TryGetStageGroundHeight(stage, previousPosition, previousGroundY);

		const float previousClearance = previousPosition.y - previousGroundY;
		const float currentClearance = currentPosition.y - currentGroundY;
		if (currentClearance > kGroundImpactTolerance)
		{
			return false;
		}
		if (previousClearance <= currentClearance && currentClearance > -kGroundImpactTolerance)
		{
			return false;
		}

		const float denom = previousClearance - currentClearance;
		float t = denom > 1e-5f ? previousClearance / denom : 1.0f;
		t = bsmUtil::Clamp(t, 0.0f, 1.0f);

		outImpactPosition = previousPosition + ((currentPosition - previousPosition) * t);
		float impactGroundY = currentGroundY;
		TryGetStageGroundHeight(stage, outImpactPosition, impactGroundY);
		outImpactPosition.y = impactGroundY;
		return true;
	}

	bool BombImpactResolver::TryGetStageGroundHeight(
		const std::shared_ptr<Stage>& stage,
		const Vec3& position,
		float& outHeight) noexcept
	{
		auto gameStage = std::dynamic_pointer_cast<GameStage>(stage);
		if (!gameStage)
		{
			return false;
		}

		return gameStage->TryGetSlopeGroundHeight(position, outHeight);
	}

	Vec3 BombImpactResolver::SafeNormalize(const Vec3& value) noexcept
	{
		const float lenSq = (value.x * value.x) + (value.y * value.y) + (value.z * value.z);
		if (lenSq < 1e-8f)
		{
			return Vec3(0.0f, 1.0f, 0.0f);
		}

		return value / std::sqrt(lenSq);
	}

}
