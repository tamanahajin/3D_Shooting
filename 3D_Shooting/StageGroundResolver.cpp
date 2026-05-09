#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const float kGroundNormalMinY = 0.7f;
		const float kSurfaceTolerance = 0.08f;
		const float kGroundedSnapAbove = 0.8f;
		const float kGroundedSnapBelow = 0.85f;
		const float kAirborneBaseSnapBelow = 0.35f;
		const float kMaxSnapBelow = 1.2f;
		const float kMissedLandingPreviousBelow = 1.0f;
		const float kMissedLandingRecoveryDepth = 3.0f;
	}

	bool TryResolveGroundHeight(float groundY, StageGroundResolveState& state)
	{
		if (state.gravityVelocity.y > 0.0f)
		{
			state.isGrounded = false;
			return false;
		}

		const float feetY = state.position.y - state.footOffset;
		const float previousFeetY = state.previousPosition.y - state.footOffset;
		const bool crossedDown = previousFeetY >= groundY - kSurfaceTolerance &&
			feetY <= groundY + kSurfaceTolerance;

		const float snapAbove = state.wasGrounded ? kGroundedSnapAbove : kSurfaceTolerance;
		float snapBelow = state.wasGrounded
			? kGroundedSnapBelow
			: kAirborneBaseSnapBelow + (-state.gravityVelocity.y * state.elapsedTime);
		if (snapBelow > kMaxSnapBelow)
		{
			snapBelow = kMaxSnapBelow;
		}

		const bool closeEnough = feetY <= groundY + snapAbove && feetY >= groundY - snapBelow;
		const float penetrationDepth = groundY - feetY;
		const bool missedLandingRecovery =
			penetrationDepth > 0.0f &&
			penetrationDepth <= kMissedLandingRecoveryDepth &&
			previousFeetY >= groundY - kMissedLandingPreviousBelow;

		if (!crossedDown && !closeEnough && !missedLandingRecovery)
		{
			state.isGrounded = false;
			return false;
		}

		state.position.y = groundY + state.footOffset;
		state.isGrounded = true;
		if (state.gravityVelocity.y < 0.0f)
		{
			state.gravityVelocity.y = 0.0f;
		}
		return true;
	}

	bool TryResolveStageGround(const GameStage& stage, StageGroundResolveState& state)
	{
		float groundY = 0.0f;
		if (!stage.TryGetSlopeGroundHeight(state.position, groundY))
		{
			state.isGrounded = false;
			return false;
		}

		return TryResolveGroundHeight(groundY, state);
	}

	bool TryApplyGroundCollision(const CollisionPair& pair, Vec3& gravityVelocity, bool& isGrounded)
	{
		if (pair.m_SrcHitNormal.y <= kGroundNormalMinY)
		{
			return false;
		}

		isGrounded = true;
		if (gravityVelocity.y < 0.0f)
		{
			gravityVelocity.y = 0.0f;
		}
		return true;
	}
}
