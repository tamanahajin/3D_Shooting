#include "stdafx.h"
#include "DamageInfo.h"

namespace shooting {

	Health::Health(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr)
	{
	}

	Health::~Health() {}

	// ダメージ適用（戻り値：死亡したか）
	bool Health::ApplyDamage(const DamageInfo& info)
	{
		if (m_Invincible || IsDead()) return false;

		m_Hp -= bsmUtil::Clamp(info.m_Damage, 0, info.m_Damage);
		if (m_Hp <= 0)
		{
			m_Hp = 0;
			if (m_OnDeath) m_OnDeath(info);
			return true;
		}
		if (m_OnDamaged) m_OnDamaged(info);
		return false;
	}
}