#include "stdafx.h"
#include "Project.h"
#include "PlayerShotEffects.h"
#include "PlayerWeapon.h"

namespace shooting {

	namespace
	{
		const float kNormalShotMinVisualEffectDistance = 0.12f;
		const wchar_t* kMuzzleFlashMeshKeys[] = {
			L"MUZZLE_FLASH_MESH_0",
			L"MUZZLE_FLASH_MESH_1",
			L"MUZZLE_FLASH_MESH_2"
		};
		const size_t kMuzzleFlashMeshCount =
			sizeof(kMuzzleFlashMeshKeys) / sizeof(kMuzzleFlashMeshKeys[0]);
		const wchar_t* kShotEffectTextureKey = L"EXPLOSION_FIRE_TX";
		const float kMuzzleFlashLifeTime = 0.065f;
		const float kMuzzleFlashStartScale = 0.11f;
		const float kMuzzleFlashEndScale = 0.20f;
		const float kBulletTracerSpeed = 260.0f;
		const float kBulletTracerLifeTimeMin = 0.045f;
		const float kBulletTracerLifeTimeMax = 0.14f;
		const float kBulletTracerLength = 1.35f;
		const float kBulletTracerWidth = 0.035f;
		const float kBulletTracerStartOffset = 0.16f;
		const wchar_t* kBulletImpactSparkMeshKey = L"BULLET_IMPACT_SPARK_MESH";
		const float kBulletImpactSparkLifeTime = 0.12f;
		const float kBulletImpactSparkStartScale = 0.3f;
		const float kBulletImpactSparkEndScale = 0.6f;
		const float kBulletImpactSparkSurfaceOffset = 0.035f;

		Vec3 NormalizeOrFallback(Vec3 value, const Vec3& fallback)
		{
			if (value.length() <= 1e-6f || value.isNaN() || value.isInfinite())
			{
				return fallback;
			}

			value.normalize();
			return value;
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

		bool ResolveMuzzleTransform(
			const std::shared_ptr<Player>& player,
			const Vec3& fallbackPosition,
			const Vec3& fallbackForward,
			PlayerWeaponMuzzleTransform& outTransform)
		{
			if (!player)
			{
				return false;
			}

			if (!TryGetPlayerWeaponMuzzleTransform(player, outTransform))
			{
				auto playerTransform = player->GetComponent<Transform>(false);
				if (!playerTransform)
				{
					return false;
				}

				outTransform.position = fallbackPosition;
				outTransform.forward =
					NormalizeOrFallback(fallbackForward, playerTransform->GetForward());
				outTransform.right =
					NormalizeOrFallback(playerTransform->GetRight(), Vec3(1.0f, 0.0f, 0.0f));
				outTransform.up =
					NormalizeOrFallback(playerTransform->GetUp(), Vec3(0.0f, 1.0f, 0.0f));
			}

			outTransform.forward = NormalizeOrFallback(outTransform.forward, fallbackForward);
			outTransform.right = NormalizeOrFallback(outTransform.right, Vec3(1.0f, 0.0f, 0.0f));
			outTransform.up = NormalizeOrFallback(outTransform.up, Vec3(0.0f, 1.0f, 0.0f));
			return true;
		}

		TransParam MakeMuzzleFlashTransParam(
			const PlayerWeaponMuzzleTransform& muzzleTransform,
			float scale)
		{
			const Mat4x4 flashWorld = MakeBasisWorldMatrix(
				muzzleTransform.right,
				muzzleTransform.up,
				muzzleTransform.forward,
				muzzleTransform.position);

			Vec3 decomposedScale;
			Vec3 position;
			Quat rotation;
			flashWorld.decompose(decomposedScale, rotation, position);

			TransParam param;
			param.position = position;
			param.quaternion = rotation;
			param.scale = Vec3(scale, scale, scale);
			return param;
		}

		const wchar_t* SelectMuzzleFlashMeshKey()
		{
			// 連射時に同じ形状が続かないよう、登録済みの3パターンを順番に使う。
			static size_t nextPattern = 0;
			const wchar_t* key = kMuzzleFlashMeshKeys[nextPattern % kMuzzleFlashMeshCount];
			++nextPattern;
			return key;
		}

		class MuzzleFlashEffect : public GameObject
		{
		private:
			std::weak_ptr<Player> m_player;
			Vec3 m_fallbackPosition;
			Vec3 m_fallbackForward;
			std::wstring m_meshKey;
			float m_elapsed = 0.0f;
			float m_currentScale = kMuzzleFlashStartScale;

			void UpdateFollowTransform(float scale)
			{
				auto transform = GetComponent<Transform>(false);
				if (!transform)
				{
					return;
				}

				if (auto player = m_player.lock())
				{
					PlayerWeaponMuzzleTransform muzzleTransform;
					if (ResolveMuzzleTransform(
						player,
						m_fallbackPosition,
						m_fallbackForward,
						muzzleTransform))
					{
						const TransParam followParam =
							MakeMuzzleFlashTransParam(muzzleTransform, scale);
						transform->SetPosition(followParam.position);
						transform->SetQuaternion(followParam.quaternion);
					}
				}

				transform->SetScale(Vec3(scale, scale, scale));
			}

		public:
			MuzzleFlashEffect(
				const std::shared_ptr<Stage>& stage,
				const TransParam& param,
				const std::shared_ptr<Player>& player,
				const Vec3& fallbackPosition,
				const Vec3& fallbackForward,
				const std::wstring& meshKey) :
				GameObject(stage),
				m_player(player),
				m_fallbackPosition(fallbackPosition),
				m_fallbackForward(fallbackForward),
				m_meshKey(meshKey)
			{
				m_transParam = param;
			}

			void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				draw->AddBaseMesh(m_meshKey);
				draw->AddBaseTexture(kShotEffectTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(2.0f, 1.25f, 0.35f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.82f, 0.28f, 0.95f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));
			}

			void OnUpdate(double elapsedTime) override
			{
				m_elapsed += static_cast<float>(elapsedTime);
				const float t = bsmUtil::Clamp(m_elapsed / kMuzzleFlashLifeTime, 0.0f, 1.0f);
				const float scale = bsmUtil::Lerp(kMuzzleFlashStartScale, kMuzzleFlashEndScale, t);
				const float fade = 1.0f - t;
				m_currentScale = scale;
				UpdateFollowTransform(scale);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					draw->SetEmissive(Col4(2.0f * fade, 1.25f * fade, 0.35f * fade, 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.82f, 0.28f, 0.95f * fade));
				}

				if (m_elapsed >= kMuzzleFlashLifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}

			void OnUpdate2(double elapsedTime) override
			{
				UNREFERENCED_PARAMETER(elapsedTime);
				// 描画直前にも追従し、プレイヤー更新後の最新ソケット位置へ合わせる。
				UpdateFollowTransform(m_currentScale);
			}
		};

		class BulletTracerEffect : public GameObject
		{
		private:
			Vec3 m_start;
			Vec3 m_direction;
			float m_distance = 0.0f;
			float m_elapsed = 0.0f;
			float m_lifeTime = kBulletTracerLifeTimeMin;

			void UpdateTracerTransform(float t)
			{
				auto transform = GetComponent<Transform>(false);
				if (!transform)
				{
					return;
				}

				// 全距離を一本の線にせず、短い線分を前方へ高速移動させて弾速感を出す。
				const float segmentLength =
					bsmUtil::Min(kBulletTracerLength, bsmUtil::Max(0.05f, m_distance));
				const float startCenter =
					bsmUtil::Min(m_distance * 0.5f, kBulletTracerStartOffset + segmentLength * 0.5f);
				const float endCenter =
					bsmUtil::Max(startCenter, m_distance - segmentLength * 0.5f);
				const float centerDistance = bsmUtil::Lerp(startCenter, endCenter, t);
				const float width = kBulletTracerWidth * bsmUtil::Lerp(1.0f, 0.65f, t);

				transform->SetPosition(m_start + m_direction * centerDistance);
				transform->SetQuaternion(
					bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_direction));
				transform->SetScale(Vec3(width, width, segmentLength));
			}

		public:
			BulletTracerEffect(
				const std::shared_ptr<Stage>& stage,
				const Vec3& start,
				const Vec3& end,
				const Vec3& fallbackForward) :
				GameObject(stage),
				m_start(start)
			{
				Vec3 delta = end - start;
				if (delta.length() <= 1e-5f || delta.isNaN() || delta.isInfinite())
				{
					delta = NormalizeOrFallback(
						fallbackForward,
						Vec3(0.0f, 0.0f, 1.0f));
				}

				m_distance = bsmUtil::Max(0.05f, delta.length());
				m_direction = delta;
				m_direction.normalize();
				m_lifeTime = bsmUtil::Clamp(
					m_distance / kBulletTracerSpeed,
					kBulletTracerLifeTimeMin,
					kBulletTracerLifeTimeMax);
			}

			void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				draw->AddBaseMesh(L"DEFAULT_CUBE");
				draw->AddBaseTexture(kShotEffectTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(1.9f, 1.55f, 0.55f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.9f, 0.35f, 0.82f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));
				UpdateTracerTransform(0.0f);
			}

			void OnUpdate(double elapsedTime) override
			{
				m_elapsed += static_cast<float>(elapsedTime);
				const float t = bsmUtil::Clamp(m_elapsed / m_lifeTime, 0.0f, 1.0f);
				const float fade = 1.0f - t;
				UpdateTracerTransform(t);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					draw->SetEmissive(Col4(1.9f * fade, 1.55f * fade, 0.55f * fade, 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.9f, 0.35f, 0.82f * fade));
				}

				if (m_elapsed >= m_lifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}
		};

		class BulletImpactSparkEffect : public GameObject
		{
		private:
			Vec3 m_point;
			Vec3 m_normal;
			float m_elapsed = 0.0f;

			void UpdateSparkTransform(float t)
			{
				auto transform = GetComponent<Transform>(false);
				if (!transform)
				{
					return;
				}

				transform->SetPosition(m_point + m_normal * kBulletImpactSparkSurfaceOffset);
				// メッシュのローカル+Zを着弾面法線へ向ける。
				transform->SetQuaternion(
					bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_normal));
				const float scale = bsmUtil::Lerp(
					kBulletImpactSparkStartScale,
					kBulletImpactSparkEndScale,
					t);
				transform->SetScale(Vec3(scale, scale, scale));
			}

		public:
			BulletImpactSparkEffect(
				const std::shared_ptr<Stage>& stage,
				const Vec3& point,
				const Vec3& normal) :
				GameObject(stage),
				m_point(point),
				m_normal(NormalizeOrFallback(normal, Vec3(0.0f, 1.0f, 0.0f)))
			{
				m_transParam.position = m_point + m_normal * kBulletImpactSparkSurfaceOffset;
				m_transParam.scale = Vec3(
					kBulletImpactSparkStartScale,
					kBulletImpactSparkStartScale,
					kBulletImpactSparkStartScale);
				m_transParam.quaternion =
					bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_normal);
			}

			void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				draw->AddBaseMesh(kBulletImpactSparkMeshKey);
				draw->AddBaseTexture(kShotEffectTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(1.8f, 1.2f, 0.25f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.78f, 0.22f, 0.82f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));
				UpdateSparkTransform(0.0f);
			}

			void OnUpdate(double elapsedTime) override
			{
				m_elapsed += static_cast<float>(elapsedTime);
				const float t =
					bsmUtil::Clamp(m_elapsed / kBulletImpactSparkLifeTime, 0.0f, 1.0f);
				const float fade = 1.0f - t;
				UpdateSparkTransform(t);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					draw->SetEmissive(Col4(1.8f * fade, 1.2f * fade, 0.25f * fade, 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.78f, 0.22f, 0.82f * fade));
				}

				if (m_elapsed >= kBulletImpactSparkLifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}
		};
	}

	void SpawnPlayerMuzzleFlash(
		const std::shared_ptr<Player>& player,
		const Vec3& fallbackPosition,
		const Vec3& fallbackForward)
	{
		if (!player)
		{
			return;
		}

		PlayerWeaponMuzzleTransform muzzleTransform;
		if (!ResolveMuzzleTransform(player, fallbackPosition, fallbackForward, muzzleTransform))
		{
			return;
		}

		const TransParam param =
			MakeMuzzleFlashTransParam(muzzleTransform, kMuzzleFlashStartScale);
		player->GetStage()->AddGameObject<MuzzleFlashEffect>(
			param,
			player,
			fallbackPosition,
			fallbackForward,
			SelectMuzzleFlashMeshKey());
	}

	void SpawnPlayerBulletTracer(
		const std::shared_ptr<Stage>& stage,
		const Vec3& start,
		const Vec3& end,
		const Vec3& fallbackForward)
	{
		if (!stage ||
			!bsmUtil::IsFiniteVec3(start) ||
			!bsmUtil::IsFiniteVec3(end))
		{
			return;
		}

		const Vec3 delta = end - start;
		if (!bsmUtil::IsFiniteVec3(delta) ||
			delta.length() < kNormalShotMinVisualEffectDistance)
		{
			return;
		}

		stage->AddGameObject<BulletTracerEffect>(start, end, fallbackForward);
	}

	void SpawnPlayerBulletImpactSpark(
		const std::shared_ptr<Stage>& stage,
		const Vec3& shotStart,
		const RaycastHit& hit,
		const Vec3& shotForward)
	{
		if (!stage ||
			!bsmUtil::IsFiniteVec3(shotStart) ||
			!bsmUtil::IsFiniteVec3(hit.m_Point) ||
			(hit.m_Point - shotStart).length() < kNormalShotMinVisualEffectDistance)
		{
			return;
		}

		// 法線が取得できない相手では、弾の進行方向の逆を予備法線として使用する。
		const Vec3 fallbackNormal =
			NormalizeOrFallback(shotForward * -1.0f, Vec3(0.0f, 1.0f, 0.0f));
		const Vec3 normal = NormalizeOrFallback(hit.m_Normal, fallbackNormal);
		if (bsmUtil::IsFiniteVec3(normal))
		{
			stage->AddGameObject<BulletImpactSparkEffect>(hit.m_Point, normal);
		}
	}

}
