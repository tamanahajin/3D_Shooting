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
	};

	const BombTuning& GetBombTuning();
}
