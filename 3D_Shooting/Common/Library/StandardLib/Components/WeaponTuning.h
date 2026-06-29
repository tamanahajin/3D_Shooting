// WeaponTuning.h
#pragma once
#include "stdafx.h"

namespace shooting {

	struct WeaponTuning
	{
		float normalShotRange = 60.0f;
		int normalShotDamage = 1;
		double normalShotCooldown = 0.12;

		float defaultBulletSpeed = 15.0f;
		int defaultBulletDamage = 3;
		double defaultBulletLifeTime = 5.0;

		int bulletPoolInitialSize = 100;

		float bombMaxRange = 20.0f;
		float bombStartBodyCenterHeight = 0.65f;
		float bombSpeed = 10.0f;
		double bombFuseTime = 3.0;
		double bombShotCooldown = 1.0;
		double explosionDuration = 0.08;
		int explosionDamage = 10;
		Vec3 bombProjectileScale = Vec3(0.01f, 0.01f, 0.01f);

		float arcHeightBase = 1.5f;
		float arcHeightPerDistXZ = 0.1f;
		Vec3  gravity = Vec3(0, -20.0f, 0);
		float explosionRadius = 2.0f;
		// カメラシェイクは爆発地点との距離で減衰し、有効距離より遠い場合は発生しない。
		float cameraShakeIntensity = 0.32f;
		float cameraShakeDuration = 0.28f;
		float cameraShakeMaxDistance = 24.0f;
	};

	const WeaponTuning& GetWeaponTuning();
}
