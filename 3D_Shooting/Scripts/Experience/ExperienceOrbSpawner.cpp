#include "stdafx.h"
#include "Project.h"

namespace shooting {

	ExperienceOrbSpawner::ExperienceOrbSpawner(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	void ExperienceOrbSpawner::OnCreate()
	{
		SetDrawActive(false);
		SetShadowActive(false);
		AddTag(L"ExperienceOrbSpawner");

		if (auto stage = GetStage(false))
		{
			stage->SetSharedGameObject(L"ExperienceOrbSpawner", GetThis<GameObject>());
		}
	}

	void ExperienceOrbSpawner::Prewarm(int count)
	{
		auto stage = GetStage(false);
		if (!stage || count <= 0)
		{
			return;
		}

		const int missingCount = count - static_cast<int>(m_allOrbs.size());
		for (int i = 0; i < missingCount; ++i)
		{
			auto orb = stage->AddGameObject<ExperienceOrb>(GetThis<ExperienceOrbSpawner>());
			orb->DeactivateForPool();
			m_allOrbs.push_back(orb);
			m_freeOrbs.push_back(orb);
		}
	}

	std::shared_ptr<ExperienceOrb> ExperienceOrbSpawner::AcquireOrb()
	{
		while (!m_freeOrbs.empty())
		{
			auto orb = m_freeOrbs.back();
			m_freeOrbs.pop_back();
			if (orb)
			{
				return orb;
			}
		}

		auto stage = GetStage(false);
		if (!stage)
		{
			return nullptr;
		}

		auto orb = stage->AddGameObject<ExperienceOrb>(GetThis<ExperienceOrbSpawner>());
		m_allOrbs.push_back(orb);
		return orb;
	}

	void ExperienceOrbSpawner::Spawn(const Vec3& position, int experienceAmount, const Quat& rotation)
	{
		if (experienceAmount <= 0)
		{
			return;
		}

		auto orb = AcquireOrb();
		if (!orb)
		{
			return;
		}

		Vec3 spawnPosition = position;
		spawnPosition.y += GetPlayerTuning().experienceOrbDropHeightOffset;
		orb->Activate(spawnPosition, experienceAmount, rotation);
	}

	void ExperienceOrbSpawner::ReleaseOrb(const std::shared_ptr<ExperienceOrb>& orb)
	{
		if (!orb)
		{
			return;
		}

		orb->DeactivateForPool();
		m_freeOrbs.push_back(orb);
	}

}
