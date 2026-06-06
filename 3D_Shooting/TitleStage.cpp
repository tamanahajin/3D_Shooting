#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const Vec3 kTitleCameraAtBase(0.0f, 2.0f, 1.0f);
		const Vec3 kTitleCameraEyeBase(0.0f, 10.5f, -23.0f);
		const float kTitleCameraSwayWidth = 1.15f;
		const float kTitleCameraSwaySpeed = 0.28f;
		const float kTitleCharacterModelDown = -0.35f;

		TransParam MakeTitleTransform(const Vec3& position, const Vec3& scale, float yRotation)
		{
			TransParam param;
			param.position = position;
			param.scale = scale;
			Quat rotation;
			rotation.rotationRollPitchYawFromVector(Vec3(0.0f, yRotation, 0.0f));
			param.quaternion = rotation;
			return param;
		}
	}

	TitleStaticModel::TitleStaticModel(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& modelKey,
		const std::wstring& materialPrefix,
		const Col4& fallbackColor,
		float rotationSpeed) :
		GameObject(stage),
		m_ModelKey(modelKey),
		m_MaterialPrefix(materialPrefix),
		m_FallbackColor(fallbackColor),
		m_RotationSpeed(rotationSpeed)
	{
		m_transParam = param;
	}

	void TitleStaticModel::OnCreate()
	{
		SetAlphaActive(false);
		SetShadowActive(true);

		auto draw = AddComponent<BcPNTStaticDraw>();
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(true);
		draw->SetLightingEnabled(true);

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_ModelKey);
		if (!meshes.empty())
		{
			draw->AddBaseModelMesh(meshes);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				draw->AddBaseMaterial(m_MaterialPrefix + std::to_wstring(i));
			}
		}
		else
		{
			draw->AddBaseMesh(L"DEFAULT_SPHERE");
			draw->SetDiffuseColor(m_FallbackColor);
			draw->SetLightingEnabled(false);
			draw->SetOwnShadowActive(false);
		}
	}

	void TitleStaticModel::OnUpdate(double elapsedTime)
	{
		if (std::fabs(m_RotationSpeed) <= 0.0001f)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		const Vec3 rotation = transform->GetRotation();
		transform->SetRotation(rotation.x, rotation.y + (m_RotationSpeed * static_cast<float>(elapsedTime)), rotation.z);
	}

	TitleSkinnedModel::TitleSkinnedModel(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& meshKey,
		const std::wstring& textureKey,
		const Vec3& modelOffset,
		AnimState animState,
		float rotationSpeed) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_TextureKey(textureKey),
		m_ModelOffset(modelOffset),
		m_AnimState(animState),
		m_RotationSpeed(rotationSpeed)
	{
		m_transParam = param;
	}

	void TitleSkinnedModel::OnCreate()
	{
		SetAlphaActive(false);
		SetShadowActive(true);

		auto draw = AddComponent<BcPNTBoneDraw>();
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(true);
		draw->AddBaseMesh(m_MeshKey);
		draw->AddBaseTexture(m_TextureKey);
		draw->SetModelOffset(m_ModelOffset);

		auto shadow = AddComponent<ShadowMap>();
		shadow->AddBaseMesh(m_MeshKey);
		shadow->SetModelOffset(m_ModelOffset);

		auto anim = GetBehavior<AnimationStateBehavior>();
		anim->SetFallbackMeshKey(m_MeshKey);
		anim->ChangeAnimation(m_AnimState, true);
	}

	void TitleSkinnedModel::OnUpdate(double elapsedTime)
	{
		if (std::fabs(m_RotationSpeed) <= 0.0001f)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		const Vec3 rotation = transform->GetRotation();
		transform->SetRotation(rotation.x, rotation.y + (m_RotationSpeed * static_cast<float>(elapsedTime)), rotation.z);
	}

	void TitleStage::OnCreate()
	{
		m_camera = ObjectFactory::Create<PerspecCamera>();
		m_camera->SetEye(kTitleCameraEyeBase);
		m_camera->SetAt(kTitleCameraAtBase);
		m_lightSet = ObjectFactory::Create<LightSet>();

		CreateGround();
		CreateWalls();
		CreateHeightVariationObjects();
		CreateCoverObjects();
		AddGameObject<SkyDome>();
		CreateTitleActors();

		GameAudio::Instance().PlayBgm(GameBgmId::Title);
	}

	void TitleStage::CreateTitleActors()
	{
		AddGameObject<TitleSkinnedModel>(
			MakeTitleTransform(Vec3(-1.5f, 0.52f, 0.0f), Vec3(0.01f, 0.01f, 0.01f), XM_PIDIV4),
			L"PLAYER_MODEL_SKINNED",
			L"CHARACTER_TEXTURE_SKINNED",
			Vec3(0.0f, kTitleCharacterModelDown, 0.0f),
			AnimState::Idle,
			0.0f);

		AddGameObject<TitleSkinnedModel>(
			MakeTitleTransform(Vec3(3.3f, 0.52f, 1.4f), Vec3(0.01f, 0.01f, 0.01f), -XM_PIDIV4),
			L"ENEMY_MODEL_SKINNED",
			L"CHARACTER_TEXTURE_SKINNED",
			Vec3(0.0f, kTitleCharacterModelDown, 0.0f),
			AnimState::Idle,
			0.0f);

		AddGameObject<TitleStaticModel>(
			MakeTitleTransform(Vec3(0.8f, 0.9f, -0.8f), Vec3(0.18f, 0.18f, 0.18f), 0.0f),
			L"BOMB_MODEL",
			L"BOMB_MAT_",
			Col4(0.9f, 0.9f, 0.9f, 1.0f),
			1.4f);
	}

	void TitleStage::OnUpdate(double elapsedTime)
	{
		m_Time += elapsedTime;

		const float sway = std::sin(static_cast<float>(m_Time) * kTitleCameraSwaySpeed) * kTitleCameraSwayWidth;
		m_camera->SetAt(kTitleCameraAtBase + Vec3(sway * 0.25f, 0.0f, 0.0f));
		m_camera->SetEye(kTitleCameraEyeBase + Vec3(sway, 0.0f, 0.0f));
	}

	void TitleStage::UpdateCollision()
	{
		// タイトル背景は見た目専用なので、静的コリジョンを生成していても毎フレームの判定は行わない。
	}

}
