// BombTuning.h
#pragma once
#include "stdafx.h"

namespace shooting {

	struct BombTuning
	{
		float arcHeightBase = 1.5f;
		float arcHeightPerDistXZ = 0.1f;
		Vec3  gravity = Vec3(0, -20.0f, 0);
		float explosionRadius = 2.0f;
		// カメラシェイクは爆発地点との距離で減衰し、有効距離より遠い場合は発生しない。
		float cameraShakeIntensity = 0.32f;
		float cameraShakeDuration = 0.28f;
		float cameraShakeMaxDistance = 24.0f;
	};

	const BombTuning& GetBombTuning();
}
