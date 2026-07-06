#include "stdafx.h"
#include "Project.h"

namespace shooting {

	PlayerLevel::PlayerLevel()
	{
		m_level = GetPlayerTuning().initialLevel;
	}

	int PlayerLevel::AddExperience(int amount)
	{
		if (amount <= 0)
		{
			return 0;
		}

		m_experience += amount;

		int levelUpCount = 0;
		while (m_experience >= GetRequiredExperience())
		{
			m_experience -= GetRequiredExperience();
			++m_level;
			++levelUpCount;
		}

		return levelUpCount;
	}

	int PlayerLevel::GetRequiredExperience() const
	{
		const auto& tuning = GetPlayerTuning();
		const int levelOffset = bsmUtil::Max(0, m_level - tuning.initialLevel);
		return bsmUtil::Max(
			1,
			tuning.requiredExperienceBase + levelOffset * tuning.requiredExperienceIncrease);
	}

	float PlayerLevel::GetExperienceRatio() const
	{
		const int requiredExperience = GetRequiredExperience();
		if (requiredExperience <= 0)
		{
			return 0.0f;
		}
		return bsmUtil::Clamp(
			static_cast<float>(m_experience) / static_cast<float>(requiredExperience),
			0.0f,
			1.0f);
	}

	int PlayerLevel::GetGunDamageBonus() const
	{
		const auto& tuning = GetPlayerTuning();
		const int levelOffset = bsmUtil::Max(0, m_level - tuning.initialLevel);
		return levelOffset * tuning.gunDamageBonusPerLevel;
	}

}
