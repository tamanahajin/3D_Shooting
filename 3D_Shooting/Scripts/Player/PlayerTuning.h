/*!
@file PlayerTuning.h
@brief プレイヤー用チューニング設定
*/
#pragma once
#include "stdafx.h"

namespace shooting {

	struct PlayerTuning
	{
		int maxHp = 20;
		int initialBombAmmo = 0;

		float moveSpeed = 6.0f;
		float jumpSpeed = 4.0f;

		float modelScale = 0.01f;
		float collisionCapsuleRadius = 0.2f;
		float collisionCapsuleSegmentHeight = 0.3f;
		float cameraTargetHeight = 1.0f;

		// 接触中に衝突開始が再発しても連続被弾しないよう、被弾後だけ再ダメージを拒否する時間。
		double damageInvincibleTime = 0.8;

		int initialLevel = 1;
		int requiredExperienceBase = 8;
		int requiredExperienceIncrease = 4;
		int gunDamageBonusPerLevel = 1;
		float experienceOrbPickupRadius = 3.2f;
		float experienceOrbCollectRadius = 0.35f;
		float experienceOrbAttractSpeed = 12.0f;
		float experienceOrbAttractHeight = 0.65f;
		float experienceOrbFloatAmplitude = 0.08f;
		float experienceOrbFloatSpeed = 3.0f;
		float experienceOrbScale = 0.085f;
		float experienceOrbDropHeightOffset = 0.20f;
		int experienceOrbPoolInitialSize = 128;

		double deathHitStopDuration = 0.18;
		double deathHitStopTimeScale = 0.03;
		double deathSoundDelay = 1.1;
		double deathAnimationTimeScale = 0.25;

		Vec3 spawnIntroWalkDirection = Vec3(0.0f, 0.0f, 1.0f);
		float spawnIntroWalkDistance = 2.4f;
		double spawnIntroPortalOnlyDuration = 1.0;
		double spawnIntroDuration = 1.05;
		float spawnIntroPortalBackOffset = 0.25f;
		float spawnIntroPortalHeight = 0.85f;
		float spawnIntroPortalScale = 1.15f;
		float spawnIntroCameraDistance = 4.2f;
		float spawnIntroCameraHeight = 1.35f;
		float spawnIntroCameraLookHeight = 1.0f;
	};

	const PlayerTuning& GetPlayerTuning();
}
