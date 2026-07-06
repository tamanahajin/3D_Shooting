#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const Col4 kExperienceOrbColor(0.0f, 0.28f, 1.0f, 1.0f);
		const Vec3 kPoolPosition(0.0f, -1000.0f, 0.0f);
	}

	ExperienceOrb::ExperienceOrb(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<ExperienceOrbSpawner>& spawner) :
		GameObject(stage),
		m_spawner(spawner)
	{
		m_transParam.position = kPoolPosition;
	}

	void ExperienceOrb::OnCreate()
	{
		AddTag(L"ExperienceOrb");
		SetShadowActive(false);

		auto draw = AddComponent<WaveEffectDraw>();
		draw->AddBaseMesh(L"DEFAULT_SPHERE");
		draw->SetColor(kExperienceOrbColor);
		draw->SetWave(0.0f, 1.0f, 0.0f);
		draw->SetEdgeMask(0.0f, 1.0f);

		DeactivateForPool();
	}

	void ExperienceOrb::Activate(const Vec3& position, int experienceAmount)
	{
		if (experienceAmount <= 0)
		{
			return;
		}

		const auto& tuning = GetPlayerTuning();
		m_basePosition = position;
		m_experienceAmount = experienceAmount;
		m_time = Util::RandZeroToOne(true) * XM_2PI;
		m_active = true;
		m_attracting = false;

		SetUpdateActive(true);
		SetDrawActive(true);
		SetShadowActive(false);

		if (auto transform = GetComponent<Transform>(false))
		{
			transform->SetPosition(position);
			transform->SetScale(Vec3(
				tuning.experienceOrbScale,
				tuning.experienceOrbScale,
				tuning.experienceOrbScale));
			transform->SetToBefore();
		}
	}

	void ExperienceOrb::DeactivateForPool()
	{
		m_active = false;
		m_attracting = false;
		m_experienceAmount = 0;
		m_player.reset();

		SetUpdateActive(false);
		SetDrawActive(false);
		SetShadowActive(false);

		if (auto transform = GetComponent<Transform>(false))
		{
			transform->SetPosition(kPoolPosition);
			transform->SetToBefore();
		}
	}

	std::shared_ptr<Player> ExperienceOrb::GetPlayer()
	{
		auto player = m_player.lock();
		if (player)
		{
			return player;
		}

		auto stage = GetStage(false);
		if (!stage)
		{
			return nullptr;
		}

		player = stage->GetSharedGameObjectEx<Player>(L"Player", false);
		m_player = player;
		return player;
	}

	void ExperienceOrb::Collect(const std::shared_ptr<Player>& player)
	{
		if (!m_active || !player)
		{
			return;
		}

		player->AddExperience(m_experienceAmount);
		GameAudio::Instance().PlaySound(GameSoundId::ItemPickup);

		auto spawner = m_spawner.lock();
		if (spawner)
		{
			spawner->ReleaseOrb(GetThis<ExperienceOrb>());
		}
		else
		{
			DeactivateForPool();
		}
	}

	void ExperienceOrb::OnUpdate(double elapsedTime)
	{
		if (!m_active)
		{
			return;
		}

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		m_time += elapsedTime;

		auto transform = GetComponent<Transform>(false);
		auto player = GetPlayer();
		if (!transform || !player || player->IsDead())
		{
			return;
		}

		auto playerTransform = player->GetComponent<Transform>(false);
		if (!playerTransform)
		{
			return;
		}

		const auto& tuning = GetPlayerTuning();
		const Vec3 playerPosition = playerTransform->GetWorldPosition();
		Vec3 currentPosition = transform->GetPosition();

		Vec3 toPlayer = playerPosition - currentPosition;
		const float distanceSq = bsmUtil::lengthSqr(toPlayer);
		const float pickupRadius = player->GetExperienceOrbPickupRadius();
		if (!m_attracting && distanceSq <= pickupRadius * pickupRadius)
		{
			m_attracting = true;
		}

		if (m_attracting)
		{
			Vec3 targetPosition = playerPosition;
			targetPosition.y += tuning.experienceOrbAttractHeight;
			Vec3 toTarget = targetPosition - currentPosition;
			const float distance = bsmUtil::length(toTarget);
			if (distance <= tuning.experienceOrbCollectRadius)
			{
				Collect(player);
				return;
			}

			if (distance > 1e-6f)
			{
				toTarget *= (1.0f / distance);
				const float moveDistance = bsmUtil::Min(
					distance,
					tuning.experienceOrbAttractSpeed * static_cast<float>(elapsedTime));
				currentPosition += toTarget * moveDistance;
			}
		}
		else
		{
			currentPosition = m_basePosition;
			currentPosition.y += static_cast<float>(
				std::sin(m_time * tuning.experienceOrbFloatSpeed)) *
				tuning.experienceOrbFloatAmplitude;
		}

		transform->SetPosition(currentPosition);
		transform->SetRotation(0.0f, static_cast<float>(m_time) * 1.8f, 0.0f);
	}

}
