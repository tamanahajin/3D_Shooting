#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const wchar_t* kExperienceOrbModelKey = L"EXPERIENCE_ORB_MODEL";
		const wchar_t* kExperienceOrbMaterialPrefix = L"EXPERIENCE_ORB_MAT_";
		const Col4 kExperienceOrbColor(0.0f, 0.28f, 1.0f, 1.0f);
		const Col4 kExperienceOrbEmissiveBase(0.0f, 0.32f, 1.05f, 1.0f);
		const Col4 kExperienceOrbEmissivePulse(0.0f, 0.16f, 0.35f, 0.0f);
		const float kExperienceOrbGlowPulseSpeed = 3.0f;
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

		const auto& meshes = BaseScene::Get()->GetModelMesh(kExperienceOrbModelKey);

		auto draw = AddComponent<BcPNTStaticDraw>();
		m_draw = draw;
		draw->SetDiffuseColor(kExperienceOrbColor);
		draw->SetEmissiveColor(kExperienceOrbEmissiveBase);
		draw->SetLightingEnabled(true);
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(false);

		if (!meshes.empty())
		{
			draw->AddBaseModelMesh(meshes);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				draw->AddBaseMaterial(std::wstring(kExperienceOrbMaterialPrefix) + std::to_wstring(i));
			}
		}
		else
		{
			draw->AddBaseMesh(L"DEFAULT_SPHERE");
		}

		DeactivateForPool();
	}

	void ExperienceOrb::Activate(const Vec3& position, int experienceAmount, const Quat& rotation)
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

		if (auto draw = m_draw.lock())
		{
			draw->SetDiffuseColor(kExperienceOrbColor);
			draw->SetEmissiveColor(kExperienceOrbEmissiveBase);
		}

		SetUpdateActive(true);
		SetDrawActive(true);
		SetShadowActive(false);

		if (auto transform = GetComponent<Transform>(false))
		{
			transform->SetPosition(position);
			transform->SetQuaternion(rotation);
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

		if (auto draw = m_draw.lock())
		{
			draw->SetEmissiveColor(kExperienceOrbEmissiveBase);
		}

		if (auto transform = GetComponent<Transform>(false))
		{
			transform->SetPosition(kPoolPosition);
			transform->SetRotation(0.0f, 0.0f, 0.0f);
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

		if (auto draw = m_draw.lock())
		{
			const float glow = 0.5f + 0.5f * std::sin(static_cast<float>(m_time) * kExperienceOrbGlowPulseSpeed);
			draw->SetEmissiveColor(Col4(
				kExperienceOrbEmissiveBase.x + kExperienceOrbEmissivePulse.x * glow,
				kExperienceOrbEmissiveBase.y + kExperienceOrbEmissivePulse.y * glow,
				kExperienceOrbEmissiveBase.z + kExperienceOrbEmissivePulse.z * glow,
				1.0f));
		}

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
	}

}
