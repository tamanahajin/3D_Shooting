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
		const float kUpwardTerrainCatchDepth = 0.75f;
	}

	bool TryResolveGroundHeight(float groundY, StageGroundResolveState& state)
	{
		const float feetY = state.position.y - state.footOffset;
		const float previousFeetY = state.previousPosition.y - state.footOffset;

		if (state.gravityVelocity.y > 0.0f)
		{
			const float penetrationDepth = groundY - feetY;
			// ジャンプ中に坂へ向かうと、上昇速度のまま坂面を浅く跨いで次フレームで裏側へ抜けることがある。
			// 足元が坂面へ少しだけ入った場合は接地として拾い、深く入りすぎた場合は別の進入制限で止める。
			const bool shallowUpwardTerrainHit = penetrationDepth >= 0.0f &&
				penetrationDepth <= kUpwardTerrainCatchDepth &&
				previousFeetY <= groundY + kSurfaceTolerance;
			if (!shallowUpwardTerrainHit)
			{
				state.isGrounded = false;
				return false;
			}

			state.position.y = groundY + state.footOffset;
			state.isGrounded = true;
			state.gravityVelocity.y = 0.0f;
			return true;
		}
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

	bool IsGeneratedTerrainStepBlocked(
		const GameStage& stage,
		const StageGroundResolveState& state,
		const Vec3& candidatePosition,
		float maxStepUp)
	{
		float targetGroundY = 0.0f;
		if (!stage.TryGetSlopeGroundHeight(candidatePosition, targetGroundY))
		{
			return false;
		}

		float previousGroundY = 0.0f;
		stage.TryGetSlopeGroundHeight(state.previousPosition, previousGroundY);

		const float previousFeetY = state.previousPosition.y - state.footOffset;
		const float candidateFeetY = candidatePosition.y - state.footOffset;
		const bool comingFromLowerGround = targetGroundY > previousGroundY + maxStepUp;
		const bool feetAreBelowTarget = previousFeetY < targetGroundY - maxStepUp &&
			candidateFeetY < targetGroundY - maxStepUp;
		return comingFromLowerGround && feetAreBelowTarget;
	}

	bool TrySlideAgainstGeneratedTerrainStep(
		const GameStage& stage,
		StageGroundResolveState& state,
		float maxStepUp,
		bool& outBlockedX,
		bool& outBlockedZ)
	{
		outBlockedX = false;
		outBlockedZ = false;
		if (maxStepUp <= 0.0f)
		{
			return false;
		}

		if (!IsGeneratedTerrainStepBlocked(stage, state, state.position, maxStepUp))
		{
			return false;
		}

		Vec3 slideX = state.position;
		slideX.z = state.previousPosition.z;
		Vec3 slideZ = state.position;
		slideZ.x = state.previousPosition.x;

		// 片軸ずつ戻して、斜め入力でも坂や高台の角で完全停止しにくくする。
		if (!IsGeneratedTerrainStepBlocked(stage, state, slideX, maxStepUp))
		{
			state.position = slideX;
			outBlockedZ = true;
			return true;
		}
		if (!IsGeneratedTerrainStepBlocked(stage, state, slideZ, maxStepUp))
		{
			state.position = slideZ;
			outBlockedX = true;
			return true;
		}

		state.position.x = state.previousPosition.x;
		state.position.z = state.previousPosition.z;
		outBlockedX = true;
		outBlockedZ = true;
		return true;
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
