/*!
@file ExperienceOrbSpawner.h
@brief 経験値オーブの生成とプール管理
*/

#pragma once
#include "stdafx.h"
#include <vector>

namespace shooting {

	class ExperienceOrb;

	class ExperienceOrbSpawner : public GameObject
	{
	private:
		std::vector<std::shared_ptr<ExperienceOrb>> m_freeOrbs;
		std::vector<std::shared_ptr<ExperienceOrb>> m_allOrbs;

		std::shared_ptr<ExperienceOrb> AcquireOrb();

	public:
		explicit ExperienceOrbSpawner(const std::shared_ptr<Stage>& stage);
		virtual ~ExperienceOrbSpawner() {}

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}

		void Prewarm(int count);
		void Spawn(const Vec3& position, int experienceAmount);
		void ReleaseOrb(const std::shared_ptr<ExperienceOrb>& orb);
	};

}
