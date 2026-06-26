/*!
@file BallisticTrajectory.cpp
@brief BallisticTrajectoryの実装
*/

#include "stdafx.h"
#include "BallisticTrajectory.h"

namespace shooting::BallisticTrajectory {

	float CalculateArcHeight(
		const bsm::Vec3& start,
		const bsm::Vec3& target,
		float baseHeight,
		float heightPerDistance) noexcept
	{
		const bsm::Vec3 deltaXZ(target.x - start.x, 0.0f, target.z - start.z);
		return baseHeight + (deltaXZ.length() * heightPerDistance);
	}

	bool TrySolveApexHeight(
		const bsm::Vec3& start,
		const bsm::Vec3& target,
		const bsm::Vec3& gravity,
		float arcHeight,
		BallisticTrajectorySolution& outSolution) noexcept
	{
		const float gravityStrength = -gravity.y;
		if (gravityStrength <= 1e-6f)
		{
			return false;
		}

		const float apexY = bsm::bsmUtil::Max(start.y, target.y) + arcHeight;
		const float startHeight = bsm::bsmUtil::Max(0.0f, apexY - start.y);
		const float targetHeight = bsm::bsmUtil::Max(0.0f, apexY - target.y);

		const float initialVelocityY = std::sqrt(2.0f * gravityStrength * startHeight);
		const float riseTime = initialVelocityY / gravityStrength;
		const float fallTime = std::sqrt(2.0f * targetHeight / gravityStrength);
		const float duration = bsm::bsmUtil::Max(0.001f, riseTime + fallTime);

		const bsm::Vec3 deltaXZ(target.x - start.x, 0.0f, target.z - start.z);
		const bsm::Vec3 horizontalVelocity = deltaXZ * (1.0f / duration);

		outSolution.initialVelocity =
			bsm::Vec3(horizontalVelocity.x, initialVelocityY, horizontalVelocity.z);
		outSolution.duration = duration;
		return true;
	}

	bsm::Vec3 SamplePosition(
		const bsm::Vec3& start,
		const bsm::Vec3& initialVelocity,
		const bsm::Vec3& gravity,
		float time) noexcept
	{
		return start
			+ (initialVelocity * time)
			+ (gravity * (0.5f * time * time));
	}

	bsm::Vec3 SampleVelocity(
		const bsm::Vec3& initialVelocity,
		const bsm::Vec3& gravity,
		float time) noexcept
	{
		return initialVelocity + (gravity * time);
	}

}
