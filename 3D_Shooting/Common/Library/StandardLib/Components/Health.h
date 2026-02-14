#pragma once
#include "stdafx.h"
#include "DamageInfo.h"

namespace shooting {

	class Health : public Component
	{
	private:
		int  m_MaxHP = 10;
		int  m_Hp = 10;
		bool m_Invincible = false;

	public:
		explicit Health(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~Health();
		void SetMaxHP(int v) { m_MaxHP = v; m_Hp = std::min(m_Hp, m_MaxHP); }
		void SetHP(int v) { m_Hp = bsmUtil::Clamp(v, 0, m_MaxHP); }
		int  GetHP() const { return m_Hp; }
		bool IsDead() const { return m_Hp <= 0; }

		// ダメージ適用（戻り値：死亡したか）
		bool ApplyDamage(const DamageInfo& info);

		void SetInvincible(bool b) { m_Invincible = b; }

		// 簡易イベント（好みで）
		std::function<void(const DamageInfo&)> m_OnDamaged;
		std::function<void(const DamageInfo&)> m_OnDeath;
	};
}