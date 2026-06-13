#pragma once
#include "stdafx.h"

namespace shooting {

	class GameStage;
	struct CollisionPair;

	struct StageGroundResolveState
	{
		Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 previousPosition = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 gravityVelocity = Vec3(0.0f, 0.0f, 0.0f);
		float footOffset = 0.35f;
		float elapsedTime = 0.0f;
		bool wasGrounded = false;
		bool isGrounded = false;
	};

	bool TryResolveGroundHeight(float groundY, StageGroundResolveState& state);
	bool TryResolveStageGround(const GameStage& stage, StageGroundResolveState& state);
	bool TrySlideAgainstGeneratedTerrainStep(
		const GameStage& stage,
		StageGroundResolveState& state,
		float maxStepUp,
		bool& outBlockedX,
		bool& outBlockedZ);
	bool TryApplyGroundCollision(const CollisionPair& pair, Vec3& gravityVelocity, bool& isGrounded);
}
