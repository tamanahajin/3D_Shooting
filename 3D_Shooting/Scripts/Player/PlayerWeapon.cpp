#include "stdafx.h"
#include "Project.h"
#include "PlayerWeapon.h"

namespace shooting {

	namespace
	{
		const wchar_t* kPlayerBlasterModelKey = L"PLAYER_BLASTER_MODEL";
		const wchar_t* kPlayerBlasterMaterialPrefix = L"PLAYER_BLASTER_MAT_";
		const Col4 kPlayerBlasterColor(0.18f, 0.19f, 0.21f, 1.0f);
		const Vec3 kBlasterAttachScale(0.006f, 0.006f, 0.006f);
		const Vec3 kBlasterAttachEuler(0.0f, XMConvertToRadians(120.0f), 0.0f);
		const Vec3 kBlasterSocketOffset(-0.1f, 0.05f, 0.1f);
		const float kWeaponAttachMaxIdleSnapDistance = 0.25f;
		const char* kPlayerBlasterMuzzleSocketName = "MuzzleSocket";
		const Vec3 kBlasterMuzzleLocalPosition(0.0f, 5.204915f, 46.0f);
		const float kMuzzleForwardOffset = -0.5f;

		struct RightHandSocketTransform
		{
			Vec3 position;
			Vec3 right;
			Vec3 forward;
			Vec3 up;
		};

		Vec3 NormalizeOrFallback(Vec3 value, const Vec3& fallback)
		{
			if (value.length() <= 1e-6f || value.isNaN() || value.isInfinite())
			{
				return fallback;
			}

			value.normalize();
			return value;
		}

		Vec3 TransformPointByRowMatrix(const Vec3& point, const Mat4x4& matrix)
		{
			return Vec3(
				point.x * matrix._11 + point.y * matrix._21 + point.z * matrix._31 + matrix._41,
				point.x * matrix._12 + point.y * matrix._22 + point.z * matrix._32 + matrix._42,
				point.x * matrix._13 + point.y * matrix._23 + point.z * matrix._33 + matrix._43);
		}

		Vec3 TransformSocketPointToWorld(const Vec3& modelPosition, const Mat4x4& world)
		{
			// Assimpノードの変換は既存のVec3行列演算へ任せ、従来と同じ規約で座標変換する。
			Vec3 worldPosition = modelPosition;
			worldPosition *= world;
			return worldPosition;
		}

		Mat4x4 MakeBasisWorldMatrix(
			const Vec3& right,
			const Vec3& up,
			const Vec3& forward,
			const Vec3& position)
		{
			Mat4x4 world;
			world.identity();
			world._11 = right.x;
			world._12 = right.y;
			world._13 = right.z;
			world._21 = up.x;
			world._22 = up.y;
			world._23 = up.z;
			world._31 = forward.x;
			world._32 = forward.y;
			world._33 = forward.z;
			world._41 = position.x;
			world._42 = position.y;
			world._43 = position.z;
			return world;
		}

		RightHandSocketTransform ExtractSocketWorldTransform(
			const Mat4x4& socketModel,
			const Mat4x4& playerWorld,
			const std::shared_ptr<Transform>& playerTransform)
		{
			Mat4x4 socketWorld = socketModel;
			// Assimpのノード行列をDirectXの行ベクトル形式へ変換してから、
			// プレイヤーのワールド行列を合成する。
			socketWorld.transpose();
			socketWorld *= playerWorld;

			RightHandSocketTransform result;
			result.position = socketWorld.transInMatrix();
			result.right = NormalizeOrFallback(socketWorld.rotXInMatrix(), playerTransform->GetRight());
			result.forward = NormalizeOrFallback(socketWorld.rotZInMatrix(), playerTransform->GetForward());
			result.up = NormalizeOrFallback(socketWorld.rotYInMatrix(), playerTransform->GetUp());
			return result;
		}

		/*!
		@brief アニメーション済みの右手ソケットからワールド変換を取得する

		BcPNTBoneDrawが保持するノード行列はAssimp側の列ベクトル形式なので、
		DirectXの行ベクトル形式へ変換してから位置と各軸を取得する。
		*/
		bool TryGetRightHandWorldTransform(
			const std::shared_ptr<Player>& player,
			RightHandSocketTransform& outTransform)
		{
			auto boneDraw = player ? player->GetComponent<BcPNTBoneDraw>(false) : nullptr;
			auto playerTransform = player ? player->GetComponent<Transform>(false) : nullptr;
			if (!boneDraw || !playerTransform)
			{
				return false;
			}

			Mat4x4 socketModel;
			if (!boneDraw->TryGetNodeGlobalTransform("RightHandSocket", socketModel))
			{
				return false;
			}

			const auto& playerParam = playerTransform->GetTransParam();
			Mat4x4 playerWorld;
			playerWorld.affineTransformation(
				playerParam.scale,
				playerParam.rotateOrigin,
				playerParam.quaternion,
				playerParam.position + boneDraw->GetModelOffset());

			outTransform = ExtractSocketWorldTransform(socketModel, playerWorld, playerTransform);
			return true;
		}

		bool TryGetPlayerWeaponWorldMatrix(
			const std::shared_ptr<Player>& player,
			Mat4x4& outWeaponWorld)
		{
			RightHandSocketTransform socketTransform;
			if (!TryGetRightHandWorldTransform(player, socketTransform))
			{
				return false;
			}

			Vec3 position = socketTransform.position;
			position += socketTransform.right * kBlasterSocketOffset.x;
			position += socketTransform.forward * kBlasterSocketOffset.y;
			position += socketTransform.up * kBlasterSocketOffset.z;

			Quat attachRotation;
			attachRotation.rotationRollPitchYawFromVector(kBlasterAttachEuler);

			Mat4x4 attachLocal;
			attachLocal.affineTransformation(
				kBlasterAttachScale,
				Vec3(0.0f, 0.0f, 0.0f),
				attachRotation,
				Vec3(0.0f, 0.0f, 0.0f));

			const Mat4x4 handWorld = MakeBasisWorldMatrix(
				socketTransform.right,
				socketTransform.up,
				socketTransform.forward,
				position);
			outWeaponWorld = attachLocal * handWorld;
			return true;
		}

		PlayerWeaponMuzzleTransform MakeFallbackMuzzleTransform(const Mat4x4& weaponWorld)
		{
			PlayerWeaponMuzzleTransform result;
			result.position = TransformPointByRowMatrix(kBlasterMuzzleLocalPosition, weaponWorld);
			result.right = NormalizeOrFallback(weaponWorld.rotXInMatrix(), Vec3(1.0f, 0.0f, 0.0f));
			result.forward = NormalizeOrFallback(weaponWorld.rotZInMatrix(), Vec3(0.0f, 0.0f, 1.0f));
			result.up = NormalizeOrFallback(weaponWorld.rotYInMatrix(), Vec3(0.0f, 1.0f, 0.0f));
			return result;
		}

		PlayerWeaponMuzzleTransform ExtractMuzzleWorldTransform(
			const Mat4x4& socketModel,
			const Mat4x4& weaponWorld)
		{
			const Vec3 rowModelPosition = socketModel.transInMatrix();
			const Vec3 columnModelPosition(socketModel._14, socketModel._24, socketModel._34);
			const bool useColumnMajorTransform =
				rowModelPosition.length() <= 1e-5f && columnModelPosition.length() > 1e-5f;

			Mat4x4 socketWorld = socketModel;
			if (useColumnMajorTransform)
			{
				socketWorld.transpose();
			}
			socketWorld *= weaponWorld;

			PlayerWeaponMuzzleTransform result;
			result.position = socketWorld.transInMatrix();
			if (result.position.isNaN() || result.position.isInfinite())
			{
				result.position = TransformSocketPointToWorld(
					useColumnMajorTransform ? columnModelPosition : rowModelPosition,
					weaponWorld);
			}
			result.right = NormalizeOrFallback(socketWorld.rotXInMatrix(), weaponWorld.rotXInMatrix());
			result.forward = NormalizeOrFallback(socketWorld.rotZInMatrix(), weaponWorld.rotZInMatrix());
			result.up = NormalizeOrFallback(socketWorld.rotYInMatrix(), weaponWorld.rotYInMatrix());
			return result;
		}
	}

	bool TryGetPlayerWeaponMuzzleTransform(
		const std::shared_ptr<Player>& player,
		PlayerWeaponMuzzleTransform& outTransform)
	{
		Mat4x4 weaponWorld;
		if (!TryGetPlayerWeaponWorldMatrix(player, weaponWorld))
		{
			return false;
		}

		const PlayerWeaponMuzzleTransform fallback = MakeFallbackMuzzleTransform(weaponWorld);
		outTransform = fallback;

		const auto& meshes = BaseScene::Get()->GetModelMesh(kPlayerBlasterModelKey);
		if (!meshes.empty() && meshes[0])
		{
			auto assimp = meshes[0]->GetBaseAssimp();
			Mat4x4 socketModel;
			if (assimp && assimp->TryGetNodeGlobalTransform(kPlayerBlasterMuzzleSocketName, socketModel))
			{
				outTransform = ExtractMuzzleWorldTransform(socketModel, weaponWorld);
				const Vec3 weaponOrigin = weaponWorld.transInMatrix();
				if ((outTransform.position - weaponOrigin).length() < 0.03f)
				{
					outTransform = fallback;
				}
			}
		}

		outTransform.position += outTransform.forward * kMuzzleForwardOffset;
		return true;
	}

	PlayerWeapon::PlayerWeapon(
		const std::shared_ptr<Stage>& stagePtr,
		const std::shared_ptr<Player>& player) :
		GameObject(stagePtr),
		m_Player(player)
	{
		m_transParam.position = Vec3(0.0f, -100.0f, 0.0f);
	}

	void PlayerWeapon::OnCreate()
	{
		SetAlphaActive(false);
		SetShadowActive(false);

		auto draw = AddComponent<BcPNTStaticDraw>();
		draw->SetFogEnabled(true);
		draw->SetOwnShadowActive(false);
		draw->SetDiffuseColor(kPlayerBlasterColor);
		draw->SetLightingEnabled(true);

		const auto& meshes = BaseScene::Get()->GetModelMesh(kPlayerBlasterModelKey);
		draw->AddBaseModelMesh(meshes);
		for (size_t i = 0; i < meshes.size(); ++i)
		{
			draw->AddBaseMaterial(std::wstring(kPlayerBlasterMaterialPrefix) + std::to_wstring(i));
		}

		SetDrawActive(TryUpdateFromPlayerHand());
	}

	void PlayerWeapon::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);
		SetDrawActive(TryUpdateFromPlayerHand());
	}

	bool PlayerWeapon::TryUpdateFromPlayerHand()
	{
		auto player = m_Player.lock();
		if (!player || !player->IsUpdateActive() || player->IsDead())
		{
			// 死亡モーション中は手のソケットを更新せず、武器の見た目だけを非表示にする。
			m_HasStableTransform = false;
			return false;
		}

		if (!player->IsSpawnIntroCharacterVisible())
		{
			// キャラ本体を消している登場待機中は、武器だけが先に見えないようにする。
			m_HasStableTransform = false;
			return false;
		}

		auto weaponTransform = GetComponent<Transform>(false);
		if (!weaponTransform)
		{
			return false;
		}

		Mat4x4 weaponWorld;
		if (!TryGetPlayerWeaponWorldMatrix(player, weaponWorld))
		{
			return false;
		}

		Vec3 candidateScale;
		Vec3 candidatePosition;
		Quat candidateRotation;
		weaponWorld.decompose(candidateScale, candidateRotation, candidatePosition);

		const bool invalidCandidate =
			candidatePosition.isNaN() || candidatePosition.isInfinite() ||
			candidateScale.isNaN() || candidateScale.isInfinite();

		auto playerTransform = player->GetComponent<Transform>(false);
		Vec3 playerDelta(0.0f, 0.0f, 0.0f);
		Vec3 expectedStablePosition = m_StablePosition;
		if (m_HasStableTransform && playerTransform)
		{
			playerDelta = playerTransform->GetPosition() - playerTransform->GetBeforePosition();
			expectedStablePosition += playerDelta;
		}

		auto anim = player->GetBehavior<AnimationStateBehavior>();
		const bool isIdle = anim && anim->GetCurrentState() == AnimState::Idle;
		const bool playerTeleported = playerDelta.length() > 1.0f;
		const bool jumpedInIdle =
			m_HasStableTransform &&
			m_StableTransformIsIdle &&
			isIdle &&
			!playerTeleported &&
			(invalidCandidate ||
				(candidatePosition - expectedStablePosition).length() > kWeaponAttachMaxIdleSnapDistance);

		if (jumpedInIdle)
		{
			// Idleモーションの不安定なソケット値を採用せず、前フレーム位置へ移動量だけ加える。
			weaponTransform->SetScale(m_StableScale);
			weaponTransform->SetQuaternion(m_StableRotation);
			weaponTransform->SetPosition(expectedStablePosition);
			m_StablePosition = expectedStablePosition;
			return true;
		}

		if (invalidCandidate)
		{
			return false;
		}

		weaponTransform->SetScale(candidateScale);
		weaponTransform->SetQuaternion(candidateRotation);
		weaponTransform->SetPosition(candidatePosition);

		m_HasStableTransform = true;
		m_StableTransformIsIdle = isIdle;
		m_StableScale = candidateScale;
		m_StableRotation = candidateRotation;
		m_StablePosition = candidatePosition;
		return true;
	}

}
