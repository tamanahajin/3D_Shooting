/*!
@file PlayerLevel.h
@brief プレイヤーの経験値とレベルを管理する
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	class PlayerLevel
	{
	private:
		int m_level = 1;
		int m_experience = 0;

	public:
		PlayerLevel();

		/*! @brief 経験値を加算し、必要量を超えた分だけレベルアップする */
		int AddExperience(int amount);

		int GetLevel() const { return m_level; }
		int GetExperience() const { return m_experience; }
		int GetRequiredExperience() const;
		float GetExperienceRatio() const;
		int GetGunDamageBonus() const;
	};

}
