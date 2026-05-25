#pragma once
#include "stdafx.h"

namespace shooting
{
	class GameObject;

	struct DamageInfo
	{
		int m_Damage = 0;
		std::weak_ptr<GameObject> m_Instigator;
		Vec3 m_HitPoint{};
		Vec3 m_HitNormal{};
		bool m_DelayDeathUntilLanding = false;
	};
}
