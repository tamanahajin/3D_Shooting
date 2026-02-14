#pragma once
#include "stdafx.h"

namespace shooting {

	class DamageDealer : public Component
	{
	private:
		int  m_Damage = 1;
		bool m_DestroyOnHit = true;

	public:
		explicit DamageDealer(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~DamageDealer();
		void SetDamage(int d) { m_Damage = d; }
		int  GetDamage() const { return m_Damage; }

		// 1î≠Ç≈è¡Ç¶ÇÈíeÇ»ÇÁ true
		void SetDestroyOnHit(bool b) { m_DestroyOnHit = b; }
		bool DestroyOnHit() const { return m_DestroyOnHit; }
	};
}