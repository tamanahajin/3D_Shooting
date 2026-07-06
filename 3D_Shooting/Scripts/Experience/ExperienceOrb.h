/*!
@file ExperienceOrb.h
@brief 敵撃破時に落ちる経験値オーブ
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	class ExperienceOrbSpawner;
	class Player;

	class ExperienceOrb : public GameObject
	{
	private:
		std::weak_ptr<ExperienceOrbSpawner> m_spawner;
		std::weak_ptr<Player> m_player;
		Vec3 m_basePosition = Vec3(0.0f, 0.0f, 0.0f);
		int m_experienceAmount = 0;
		double m_time = 0.0;
		bool m_active = false;
		bool m_attracting = false;

		std::shared_ptr<Player> GetPlayer();
		void Collect(const std::shared_ptr<Player>& player);

	public:
		ExperienceOrb(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<ExperienceOrbSpawner>& spawner);
		virtual ~ExperienceOrb() {}

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;

		void Activate(const Vec3& position, int experienceAmount);
		void DeactivateForPool();
		bool IsActiveOrb() const { return m_active; }
	};

}
