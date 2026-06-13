#include "stdafx.h"
#include "Project.h"
#include "PlayerAimResolver.h"
#include "PlayerWeapon.h"

namespace shooting {

	namespace
	{
		const float kNormalShotCameraAimBacktrack = 0.25f;
		const float kNormalShotMuzzleBlockMargin = 0.03f;
		const float kBombStartBodyCenterHeight = 0.65f;

		bool TryGetCameraRay(
			const std::shared_ptr<MainCamera>& camera,
			Vec3& outOrigin,
			Vec3& outDirection)
		{
			if (!camera)
			{
				return false;
			}

			outOrigin = camera->GetEye();
			outDirection = camera->GetAt() - outOrigin;
			if (!bsmUtil::IsFiniteVec3(outOrigin) ||
				!bsmUtil::IsFiniteVec3(outDirection) ||
				outDirection.length() <= 1e-6f)
			{
				return false;
			}

			outDirection.normalize();
			return true;
		}

		Vec3 ResolveMuzzlePosition(const std::shared_ptr<Player>& player)
		{
			auto transform = player ? player->GetComponent<Transform>(false) : nullptr;
			if (!transform)
			{
				return Vec3(0.0f, 0.0f, 0.0f);
			}

			Vec3 muzzle = transform->GetPosition()
				+ transform->GetForward() * 0.2f
				+ Vec3(0.0f, 0.055f, 0.0f);

			PlayerWeaponMuzzleTransform weaponMuzzle;
			if (TryGetPlayerWeaponMuzzleTransform(player, weaponMuzzle))
			{
				muzzle = weaponMuzzle.position;
			}
			return muzzle;
		}
	}

	PlayerNormalShotAim PlayerAimResolver::ResolveNormalShot(
		const std::shared_ptr<Player>& player,
		const std::shared_ptr<MainCamera>& camera,
		const std::shared_ptr<CollisionManager>& collisionManager,
		float maxRange)
	{
		PlayerNormalShotAim result;
		if (!player || !collisionManager || maxRange <= 0.0f)
		{
			return result;
		}

		Vec3 rayOrigin;
		Vec3 rayDirection;
		if (!TryGetCameraRay(camera, rayOrigin, rayDirection))
		{
			return result;
		}

		result.muzzle = ResolveMuzzlePosition(player);
		Vec3 shotAimOrigin = rayOrigin;
		float shotAimRange = maxRange;

		// カメラが壁へ密着している場合でも、銃口から見えている対象を狙えるよう
		// 照準用レイの開始位置だけを銃口の奥行き付近まで進める。
		const float muzzleDepth = bsmUtil::dot(result.muzzle - rayOrigin, rayDirection);
		if (std::isfinite(muzzleDepth) && muzzleDepth > kNormalShotCameraAimBacktrack)
		{
			const float startOffset = bsmUtil::Clamp(
				muzzleDepth - kNormalShotCameraAimBacktrack,
				0.0f,
				maxRange - 0.1f);
			shotAimOrigin = rayOrigin + rayDirection * startOffset;
			shotAimRange = maxRange - startOffset;
		}

		result.aimPoint = shotAimOrigin + rayDirection * shotAimRange;
		if (collisionManager->Raycast(
			shotAimOrigin,
			rayDirection,
			shotAimRange,
			result.hit,
			player,
			{ L"Bullet" }))
		{
			result.hasHit = true;
			result.aimPoint = result.hit.m_Point;
		}

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(player->GetStage(false)))
		{
			Vec3 generatedPoint;
			Vec3 generatedNormal(0.0f, 1.0f, 0.0f);
			float generatedDistance = 0.0f;
			if (gameStage->TryRaycastGeneratedGround(
				shotAimOrigin,
				rayDirection,
				shotAimRange,
				generatedPoint,
				generatedNormal,
				generatedDistance))
			{
				// 通常コリジョンとCSV生成地形のうち、射線上で手前にある方を着弾点にする。
				const bool generatedIsNearest =
					!result.hasHit || generatedDistance < result.hit.m_Distance - 0.05f;
				if (generatedIsNearest)
				{
					result.hasHit = true;
					result.aimPoint = generatedPoint;
					result.hit = RaycastHit{};
					result.hit.m_Point = generatedPoint;
					result.hit.m_Normal = generatedNormal;
					result.hit.m_Distance = generatedDistance;
				}
			}
		}

		// 照準はカメラ基準だが実弾の始点は銃口なので、銃口と狙い点の間を再検査する。
		// これにより、カメラからは見えていても銃口前に壁がある場合の壁越し射撃を防ぐ。
		Vec3 muzzleRay = result.aimPoint - result.muzzle;
		const float muzzleRayLength = muzzleRay.length();
		if (muzzleRayLength > 1e-4f)
		{
			muzzleRay.normalize();

			RaycastHit muzzleHit;
			if (collisionManager->Raycast(
				result.muzzle,
				muzzleRay,
				muzzleRayLength + kNormalShotMuzzleBlockMargin,
				muzzleHit,
				player,
				{ L"Bullet" }))
			{
				result.hasHit = true;
				result.hit = muzzleHit;
				result.aimPoint = muzzleHit.m_Point;
			}
		}

		result.isValid =
			bsmUtil::IsFiniteVec3(result.muzzle) &&
			bsmUtil::IsFiniteVec3(result.aimPoint);
		return result;
	}

	PlayerBombAim PlayerAimResolver::ResolveBombAim(
		const std::shared_ptr<Player>& player,
		const std::shared_ptr<MainCamera>& camera,
		const std::shared_ptr<CollisionManager>& collisionManager,
		float maxRange)
	{
		PlayerBombAim result;
		auto transform = player ? player->GetComponent<Transform>(false) : nullptr;
		if (!transform || !collisionManager || maxRange <= 0.0f)
		{
			return result;
		}

		Vec3 rayOrigin;
		Vec3 rayDirection;
		if (!TryGetCameraRay(camera, rayOrigin, rayDirection))
		{
			return result;
		}

		result.start = transform->GetPosition() + Vec3(0.0f, kBombStartBodyCenterHeight, 0.0f);
		result.aimPoint = rayOrigin + rayDirection * maxRange;

		RaycastHit physicalHit;
		if (collisionManager->Raycast(
			rayOrigin,
			rayDirection,
			maxRange,
			physicalHit,
			player,
			{ L"Bullet", L"Enemy", L"Item" }))
		{
			result.hasHit = true;
			result.aimPoint = physicalHit.m_Point;
			result.hitNormal = physicalHit.m_Normal;
		}

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(player->GetStage(false)))
		{
			Vec3 generatedPoint;
			Vec3 generatedNormal(0.0f, 1.0f, 0.0f);
			float generatedDistance = 0.0f;
			if (gameStage->TryRaycastGeneratedGround(
				rayOrigin,
				rayDirection,
				maxRange,
				generatedPoint,
				generatedNormal,
				generatedDistance))
			{
				const bool physicalHitIsWall = result.hasHit && result.hitNormal.y < 0.45f;
				const bool wallIsInFront =
					physicalHitIsWall && physicalHit.m_Distance <= generatedDistance + 0.1f;
				const bool generatedCanReplacePhysical =
					!result.hasHit ||
					generatedDistance <= physicalHit.m_Distance + 0.25f ||
					result.hitNormal.y > 0.45f;

				if (!wallIsInFront && generatedCanReplacePhysical)
				{
					result.aimPoint = generatedPoint;
					result.hitNormal = generatedNormal;
					result.hasHit = true;
				}
			}
		}

		Vec3 shotDirection = result.aimPoint - result.start;
		if (shotDirection.length() > 1e-6f)
		{
			shotDirection.normalize();
			result.rotation = bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), shotDirection);
		}

		result.isValid =
			bsmUtil::IsFiniteVec3(result.start) &&
			bsmUtil::IsFiniteVec3(result.aimPoint) &&
			bsmUtil::IsFiniteVec3(result.hitNormal);
		return result;
	}

}
