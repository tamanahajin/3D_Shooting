#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		// 通常射撃の最大射程。Raycastの終点計算に使う。
		const float kNormalShotRange = 60.0f;
		// 通常射撃が1発で与えるダメージ量。
		const int kNormalShotDamage = 1;
		// カメラが壁に近い時、その壁を通常射撃の照準対象にしないため、銃口より少し手前からカメラレイを始める。
		const float kNormalShotCameraAimBacktrack = 0.25f;
		// 銃口から狙い点までの再チェックで、ほぼ同じ位置のヒットを安定して扱うための余白。
		const float kNormalShotMuzzleBlockMargin = 0.03f;
		// 銃口のすぐ近くに当たった場合は、トレーサーや着弾スパークを出さない。
		// 近すぎるエフェクトは極端な回転・スケールになりやすく、D3D12のdevice removed要因になるため。
		const float kNormalShotMinVisualEffectDistance = 0.12f;
		// 通常射撃の発射間隔。小さいほど連射が速くなる。
		const double kNormalShotCooldown = 0.12;
		// Scene.cppで登録しているプレイヤー銃モデルのキー。
		const wchar_t* kPlayerBlasterModelKey = L"PLAYER_BLASTER_MODEL";
		// 銃モデルの各メッシュに割り当てるマテリアルキーの接頭辞。
		const wchar_t* kPlayerBlasterMaterialPrefix = L"PLAYER_BLASTER_MAT_";
		// 銃モデルに強制適用する基本色。
		const Col4 kPlayerBlasterColor(0.18f, 0.19f, 0.21f, 1.0f);
		// 銃モデルを手に持たせるときの表示スケール。
		const Vec3 kBlasterAttachScale(0.006f, 0.006f, 0.006f);
		// 銃モデルを手のソケット基準でY軸回転させる角度。
		const float kBlasterAttachYawDegrees = 120.0f;
		// 銃モデル装着時のローカル回転。上の角度をラジアンにして使う。
		const Vec3 kBlasterAttachEuler(0.0f, XMConvertToRadians(kBlasterAttachYawDegrees), 0.0f);
		// 手のソケットから銃モデルを微調整するローカル位置オフセット。
		const Vec3 kBlasterSocketOffset(-0.1f, 0.05f, 0.1f);
		// Idle中にソケット位置が大きく飛んだと判断する距離。武器の急なスナップ抑制に使う。
		const float kWeaponAttachMaxIdleSnapDistance = 0.25f;
		// 銃モデル内に作った銃口ソケット名。
		const char* kPlayerBlasterMuzzleSocketName = "MuzzleSocket";
		// マズルフラッシュ用のコード生成メッシュキー。発射ごとに順番に切り替える。
		const wchar_t* kMuzzleFlashMeshKeys[] = { L"MUZZLE_FLASH_MESH_0", L"MUZZLE_FLASH_MESH_1", L"MUZZLE_FLASH_MESH_2" };
		// マズルフラッシュメッシュの登録数。配列サイズから自動計算する。
		const size_t kMuzzleFlashMeshCount = sizeof(kMuzzleFlashMeshKeys) / sizeof(kMuzzleFlashMeshKeys[0]);
		// マズルフラッシュ、トレーサー、着弾スパークで流用する発光テクスチャキー。
		const wchar_t* kMuzzleFlashTextureKey = L"EXPLOSION_FIRE_TX";
		// マズルフラッシュの表示時間。
		const float kMuzzleFlashLifeTime = 0.065f;
		// マズルフラッシュ出現直後の大きさ。
		const float kMuzzleFlashStartScale = 0.11f;
		// マズルフラッシュが消える直前の大きさ。
		const float kMuzzleFlashEndScale = 0.20f;
		// 銃口ソケットからマズルフラッシュを前後にずらす量。
		const float kMuzzleFlashForwardOffset = -0.5f;
		// 見た目だけの弾道が進む速度。ヒットスキャン判定には影響しない。
		const float kBulletTracerSpeed = 260.0f;
		// 弾道表示の最短寿命。近距離でも一瞬は見えるようにする。
		const float kBulletTracerLifeTimeMin = 0.045f;
		// 弾道表示の最長寿命。遠距離でも残りすぎないようにする。
		const float kBulletTracerLifeTimeMax = 0.14f;
		// 弾道表示の線分長。長くすると弾の軌跡が伸びて見える。
		const float kBulletTracerLength = 1.35f;
		// 弾道表示の太さ。
		const float kBulletTracerWidth = 0.035f;
		// 銃口から弾道表示を少し前に出して、手元に重ならないようにする距離。
		const float kBulletTracerStartOffset = 0.16f;
		// 着弾スパーク用のコード生成メッシュキー。
		const wchar_t* kBulletImpactSparkMeshKey = L"BULLET_IMPACT_SPARK_MESH";
		// 着弾スパークの表示時間。
		const float kBulletImpactSparkLifeTime = 0.12f;
		// 着弾スパーク出現直後の大きさ。
		const float kBulletImpactSparkStartScale = 0.3f;
		// 着弾スパークが消える直前の大きさ。
		const float kBulletImpactSparkEndScale = 0.6f;
		// 着弾面からスパークを少し浮かせる距離。面とのちらつき防止に使う。
		const float kBulletImpactSparkSurfaceOffset = 0.035f;
		// 銃口ソケットが見つからない場合に使う、銃モデルローカル空間の予備銃口位置。
		const Vec3 kBlasterMuzzleLocalPosition(0.0f, 5.204915f, 46.0f);
		// ボム弾モデルの表示スケール。
		const Vec3 kBombProjectileScale(0.01f, 0.01f, 0.01f);

		struct RightHandSocketTransform
		{
			Vec3 position;
			Vec3 right;
			Vec3 forward;
			Vec3 up;
		};

		struct MuzzleSocketWorldTransform
		{
			Vec3 position;
			Vec3 right;
			Vec3 forward;
			Vec3 up;
		};

		Vec3 TransformSocketPointToWorld(const Vec3& modelPosition, const Mat4x4& playerWorld)
		{
			Vec3 worldPosition = modelPosition;
			worldPosition *= playerWorld;
			return worldPosition;
		}

		Vec3 TransformPointByRowMatrix(const Vec3& point, const Mat4x4& matrix)
		{
			return Vec3(
				point.x * matrix._11 + point.y * matrix._21 + point.z * matrix._31 + matrix._41,
				point.x * matrix._12 + point.y * matrix._22 + point.z * matrix._32 + matrix._42,
				point.x * matrix._13 + point.y * matrix._23 + point.z * matrix._33 + matrix._43
			);
		}

		Vec3 NormalizeOrFallback(Vec3 value, const Vec3& fallback)
		{
			if (value.length() <= 1e-6f || value.isNaN() || value.isInfinite())
			{
				return fallback;
			}

			value.normalize();
			return value;
		}

		RightHandSocketTransform ExtractSocketWorldTransform(
			const Mat4x4& socketModel,
			const Mat4x4& playerWorld,
			const std::shared_ptr<Transform>& playerTransform,
			bool useColumnMajorTransform)
		{
			const Vec3 rowModelPosition = socketModel.transInMatrix();
			const Vec3 columnModelPosition(socketModel._14, socketModel._24, socketModel._34);

			Mat4x4 socketWorld = socketModel;
			if (useColumnMajorTransform)
			{
				socketWorld.transpose();
			}
			socketWorld *= playerWorld;

			RightHandSocketTransform socketTransform;
			socketTransform.position = TransformSocketPointToWorld(
				useColumnMajorTransform ? columnModelPosition : rowModelPosition,
				playerWorld);
			socketTransform.right = NormalizeOrFallback(socketWorld.rotXInMatrix(), playerTransform->GetRight());
			socketTransform.forward = NormalizeOrFallback(socketWorld.rotZInMatrix(), playerTransform->GetForward());
			socketTransform.up = NormalizeOrFallback(socketWorld.rotYInMatrix(), playerTransform->GetUp());
			return socketTransform;
		}

		bool TryGetRightHandWorldTransform(
			const std::shared_ptr<Player>& player,
			RightHandSocketTransform& outTransform)
		{
			auto boneDraw = player->GetComponent<BcPNTBoneDraw>(false);
			auto playerTransform = player->GetComponent<Transform>(false);
			if (!boneDraw || !playerTransform)
			{
				return false;
			}

			Mat4x4 socketModel;
			if (!boneDraw->TryGetNodeGlobalTransform("RightHandSocket", socketModel))
			{
				return false;
			}

			auto& playerParam = playerTransform->GetTransParam();
			Mat4x4 playerWorld;
			playerWorld.affineTransformation(
				playerParam.scale,
				playerParam.rotateOrigin,
				playerParam.quaternion,
				playerParam.position + boneDraw->GetModelOffset()
			);

			const RightHandSocketTransform rowSocket = ExtractSocketWorldTransform(
				socketModel,
				playerWorld,
				playerTransform,
				false);
			const RightHandSocketTransform columnSocket = ExtractSocketWorldTransform(
				socketModel,
				playerWorld,
				playerTransform,
				true);

			const float rowDistance = (rowSocket.position - playerTransform->GetPosition()).length();
			const float columnDistance = (columnSocket.position - playerTransform->GetPosition()).length();
			outTransform = columnDistance < rowDistance ? columnSocket : rowSocket;
			return true;
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
			world._14 = 0.0f;
			world._21 = up.x;
			world._22 = up.y;
			world._23 = up.z;
			world._24 = 0.0f;
			world._31 = forward.x;
			world._32 = forward.y;
			world._33 = forward.z;
			world._34 = 0.0f;
			world._41 = position.x;
			world._42 = position.y;
			world._43 = position.z;
			world._44 = 1.0f;
			return world;
		}

		Mat4x4 MakeSocketBasisWorldMatrix(
			const RightHandSocketTransform& socketTransform,
			const Vec3& position)
		{
			return MakeBasisWorldMatrix(
				socketTransform.right,
				socketTransform.up,
				socketTransform.forward,
				position);
		}

		bool TryGetPlayerBlasterWorldMatrix(
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
				Vec3(0.0f, 0.0f, 0.0f)
			);

			Mat4x4 playerWorld = MakeSocketBasisWorldMatrix(socketTransform, position);

			outWeaponWorld = attachLocal * playerWorld;
			return true;
		}

		bool ApplyPlayerSpaceWeaponTransform(
			const std::shared_ptr<Player>& player,
			const std::shared_ptr<Transform>& weaponTransform)
		{
			if (!weaponTransform)
			{
				return false;
			}

			Mat4x4 weaponWorld;
			if (!TryGetPlayerBlasterWorldMatrix(player, weaponWorld))
			{
				return false;
			}

			Vec3 scale, decomposedPosition;
			Quat rotation;
			weaponWorld.decompose(scale, rotation, decomposedPosition);

			weaponTransform->SetScale(scale);
			weaponTransform->SetQuaternion(rotation);
			weaponTransform->SetPosition(decomposedPosition);
			return true;
		}

		MuzzleSocketWorldTransform ExtractMuzzleWorldTransform(
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

			const Vec3 transformedModelPosition = TransformSocketPointToWorld(
				useColumnMajorTransform ? columnModelPosition : rowModelPosition,
				weaponWorld);

			MuzzleSocketWorldTransform socketTransform;
			socketTransform.position = socketWorld.transInMatrix();
			if (socketTransform.position.isNaN() || socketTransform.position.isInfinite())
			{
				socketTransform.position = transformedModelPosition;
			}
			socketTransform.right = NormalizeOrFallback(socketWorld.rotXInMatrix(), weaponWorld.rotXInMatrix());
			socketTransform.forward = NormalizeOrFallback(socketWorld.rotZInMatrix(), weaponWorld.rotZInMatrix());
			socketTransform.up = NormalizeOrFallback(socketWorld.rotYInMatrix(), weaponWorld.rotYInMatrix());
			return socketTransform;
		}

		MuzzleSocketWorldTransform MakeFallbackMuzzleWorldTransform(const Mat4x4& weaponWorld)
		{
			MuzzleSocketWorldTransform socketTransform;
			socketTransform.position = TransformPointByRowMatrix(kBlasterMuzzleLocalPosition, weaponWorld);
			socketTransform.right = NormalizeOrFallback(weaponWorld.rotXInMatrix(), Vec3(1.0f, 0.0f, 0.0f));
			socketTransform.forward = NormalizeOrFallback(weaponWorld.rotZInMatrix(), Vec3(0.0f, 0.0f, 1.0f));
			socketTransform.up = NormalizeOrFallback(weaponWorld.rotYInMatrix(), Vec3(0.0f, 1.0f, 0.0f));
			return socketTransform;
		}

		bool TryGetPlayerBlasterMuzzleWorldTransform(
			const std::shared_ptr<Player>& player,
			MuzzleSocketWorldTransform& outTransform)
		{
			Mat4x4 weaponWorld;
			if (!TryGetPlayerBlasterWorldMatrix(player, weaponWorld))
			{
				return false;
			}

			const MuzzleSocketWorldTransform fallbackTransform = MakeFallbackMuzzleWorldTransform(weaponWorld);

			const auto& meshes = BaseScene::Get()->GetModelMesh(kPlayerBlasterModelKey);
			if (meshes.empty() || !meshes[0])
			{
				outTransform = fallbackTransform;
				outTransform.position += outTransform.forward * kMuzzleFlashForwardOffset;
				return true;
			}

			auto assimp = meshes[0]->GetBaseAssimp();
			if (!assimp)
			{
				outTransform = fallbackTransform;
				outTransform.position += outTransform.forward * kMuzzleFlashForwardOffset;
				return true;
			}

			Mat4x4 socketModel;
			if (!assimp->TryGetNodeGlobalTransform(kPlayerBlasterMuzzleSocketName, socketModel))
			{
				outTransform = fallbackTransform;
				outTransform.position += outTransform.forward * kMuzzleFlashForwardOffset;
				return true;
			}

			outTransform = ExtractMuzzleWorldTransform(socketModel, weaponWorld);
			const Vec3 weaponOrigin = weaponWorld.transInMatrix();
			if ((outTransform.position - weaponOrigin).length() < 0.03f)
			{
				outTransform = fallbackTransform;
			}
			outTransform.position += outTransform.forward * kMuzzleFlashForwardOffset;
			return true;
		}

		bool TryResolveMuzzleFlashTransform(
			const std::shared_ptr<Player>& player,
			const Vec3& fallbackPosition,
			const Vec3& fallbackForward,
			MuzzleSocketWorldTransform& outTransform)
		{
			if (!player)
			{
				return false;
			}

			if (!TryGetPlayerBlasterMuzzleWorldTransform(player, outTransform))
			{
				auto playerTransform = player->GetComponent<Transform>(false);
				if (!playerTransform)
				{
					return false;
				}

				outTransform.position = fallbackPosition;
				outTransform.forward = NormalizeOrFallback(fallbackForward, playerTransform->GetForward());
				outTransform.right = NormalizeOrFallback(playerTransform->GetRight(), Vec3(1.0f, 0.0f, 0.0f));
				outTransform.up = NormalizeOrFallback(playerTransform->GetUp(), Vec3(0.0f, 1.0f, 0.0f));
			}

			outTransform.forward = NormalizeOrFallback(outTransform.forward, fallbackForward);
			outTransform.right = NormalizeOrFallback(outTransform.right, Vec3(1.0f, 0.0f, 0.0f));
			outTransform.up = NormalizeOrFallback(outTransform.up, Vec3(0.0f, 1.0f, 0.0f));
			return true;
		}

		TransParam MakeMuzzleFlashTransParam(
			const MuzzleSocketWorldTransform& muzzleTransform,
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

		class MuzzleFlashEffect : public GameObject
		{
		private:
			std::weak_ptr<Player> m_Player;
			Vec3 m_FallbackPosition;
			Vec3 m_FallbackForward;
			// この発射で選ばれたマズルフラッシュ形状。
			std::wstring m_MeshKey;
			float m_Elapsed = 0.0f;
			float m_CurrentScale = kMuzzleFlashStartScale;

			// 銃口ソケットを追従して、移動中もエフェクトが置き去りにならないようにする。
			void UpdateFollowTransform(float scale)
			{
				if (auto transform = GetComponent<Transform>(false))
				{
					if (auto player = m_Player.lock())
					{
						MuzzleSocketWorldTransform muzzleTransform;
						if (TryResolveMuzzleFlashTransform(player, m_FallbackPosition, m_FallbackForward, muzzleTransform))
						{
							const TransParam followParam = MakeMuzzleFlashTransParam(muzzleTransform, scale);
							transform->SetPosition(followParam.position);
							transform->SetQuaternion(followParam.quaternion);
						}
					}

					transform->SetScale(Vec3(scale, scale, scale));
				}
			}

		public:
			MuzzleFlashEffect(
				const std::shared_ptr<Stage>& stagePtr,
				const TransParam& param,
				const std::shared_ptr<Player>& player,
				const Vec3& fallbackPosition,
				const Vec3& fallbackForward,
				const std::wstring& meshKey) :
				GameObject(stagePtr)
			{
				m_transParam = param;
				m_Player = player;
				m_FallbackPosition = fallbackPosition;
				m_FallbackForward = fallbackForward;
				m_MeshKey = meshKey;
			}

			virtual ~MuzzleFlashEffect() = default;

			virtual void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				draw->AddBaseMesh(m_MeshKey);
				draw->AddBaseTexture(kMuzzleFlashTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(2.0f, 1.25f, 0.35f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.82f, 0.28f, 0.95f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));
			}

			virtual void OnUpdate(double elapsedTime) override
			{
				m_Elapsed += static_cast<float>(elapsedTime);

				const float rawT = kMuzzleFlashLifeTime > 0.0f
					? m_Elapsed / kMuzzleFlashLifeTime
					: 1.0f;
				const float t = bsmUtil::Max(0.0f, bsmUtil::Min(rawT, 1.0f));
				const float scale = bsmUtil::Lerp(kMuzzleFlashStartScale, kMuzzleFlashEndScale, t);
				const float alpha = (1.0f - t) * 0.95f;
				m_CurrentScale = scale;

				UpdateFollowTransform(scale);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					draw->SetEmissive(Col4(2.0f * (1.0f - t), 1.25f * (1.0f - t), 0.35f * (1.0f - t), 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.82f, 0.28f, alpha));
				}

				if (m_Elapsed >= kMuzzleFlashLifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}

			virtual void OnUpdate2(double elapsedTime) override
			{
				UNREFERENCED_PARAMETER(elapsedTime);
				UpdateFollowTransform(m_CurrentScale);
			}
		};
		// 連射時に同じ形が続かないよう、3パターンを順番に使う。
		const wchar_t* SelectMuzzleFlashMeshKey()
		{
			static size_t nextPattern = 0;
			const wchar_t* key = kMuzzleFlashMeshKeys[nextPattern % kMuzzleFlashMeshCount];
			++nextPattern;
			return key;
		}
		void SpawnMuzzleFlash(
			const std::shared_ptr<Player>& player,
			const Vec3& fallbackPosition,
			const Vec3& fallbackForward)
		{
			if (!player)
			{
				return;
			}

			MuzzleSocketWorldTransform muzzleTransform;
			if (!TryResolveMuzzleFlashTransform(player, fallbackPosition, fallbackForward, muzzleTransform))
			{
				return;
			}

			const TransParam param = MakeMuzzleFlashTransParam(muzzleTransform, kMuzzleFlashStartScale);
			player->GetStage()->AddGameObject<MuzzleFlashEffect>(
				param,
				player,
				fallbackPosition,
				fallbackForward,
				SelectMuzzleFlashMeshKey());
		}

		// ヒット判定を持たない表示専用の弾道。短い発光ロッドを高速で流して弾速感を出す。
		class BulletTracerEffect : public GameObject
		{
		private:
			Vec3 m_Start;
			Vec3 m_Direction;
			float m_Distance = 0.0f;
			float m_Elapsed = 0.0f;
			float m_LifeTime = kBulletTracerLifeTimeMin;

			void UpdateTracerTransform(float t)
			{
				auto transform = GetComponent<Transform>(false);
				if (!transform)
				{
					return;
				}

				// 全距離を1本の長い線にせず、短い線分を前方へ移動させる。
				const float segmentLength = bsmUtil::Min(kBulletTracerLength, bsmUtil::Max(0.05f, m_Distance));
				const float startCenter = bsmUtil::Min(m_Distance * 0.5f, kBulletTracerStartOffset + (segmentLength * 0.5f));
				const float endCenter = bsmUtil::Max(startCenter, m_Distance - (segmentLength * 0.5f));
				const float centerDistance = bsmUtil::Lerp(startCenter, endCenter, t);
				const float width = kBulletTracerWidth * bsmUtil::Lerp(1.0f, 0.65f, t);

				transform->SetPosition(m_Start + (m_Direction * centerDistance));
				transform->SetQuaternion(bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_Direction));
				transform->SetScale(Vec3(width, width, segmentLength));
			}

		public:
			BulletTracerEffect(
				const std::shared_ptr<Stage>& stagePtr,
				const Vec3& start,
				const Vec3& end,
				const Vec3& fallbackForward) :
				GameObject(stagePtr),
				m_Start(start)
			{
				Vec3 delta = end - start;
				if (delta.length() <= 1e-5f || delta.isNaN() || delta.isInfinite())
				{
					delta = NormalizeOrFallback(fallbackForward, Vec3(0.0f, 0.0f, 1.0f)) * kNormalShotRange;
				}

				m_Distance = bsmUtil::Max(0.05f, delta.length());
				m_Direction = delta;
				m_Direction.normalize();
				m_LifeTime = bsmUtil::Clamp(m_Distance / kBulletTracerSpeed, kBulletTracerLifeTimeMin, kBulletTracerLifeTimeMax);
			}

			virtual ~BulletTracerEffect() = default;

			virtual void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				// 専用メッシュを増やさず、DEFAULT_CUBEを細長く伸ばして弾筋として描く。
				draw->AddBaseMesh(L"DEFAULT_CUBE");
				draw->AddBaseTexture(kMuzzleFlashTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(1.9f, 1.55f, 0.55f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.9f, 0.35f, 0.82f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));

				UpdateTracerTransform(0.0f);
			}

			virtual void OnUpdate(double elapsedTime) override
			{
				m_Elapsed += static_cast<float>(elapsedTime);
				const float t = bsmUtil::Clamp(m_Elapsed / m_LifeTime, 0.0f, 1.0f);
				UpdateTracerTransform(t);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					// 寿命後半ほど透明・低発光にして、残像がすぐ消えるようにする。
					const float fade = 1.0f - t;
					draw->SetEmissive(Col4(1.9f * fade, 1.55f * fade, 0.55f * fade, 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.9f, 0.35f, 0.82f * fade));
				}

				if (m_Elapsed >= m_LifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}
		};

		// 射撃処理側からは開始点と終点だけ渡す。ダメージ判定はApplyHitscanDamage側で完了済み。
		void SpawnBulletTracer(
			const std::shared_ptr<Stage>& stage,
			const Vec3& start,
			const Vec3& end,
			const Vec3& fallbackForward)
		{
			if (!stage)
			{
				return;
			}
			if (!bsmUtil::IsFiniteVec3(start) || !bsmUtil::IsFiniteVec3(end))
			{
				return;
			}

			const Vec3 delta = end - start;
			if (!bsmUtil::IsFiniteVec3(delta) || delta.length() < kNormalShotMinVisualEffectDistance)
			{
				return;
			}

			stage->AddGameObject<BulletTracerEffect>(start, end, fallbackForward);
		}

		// 着弾位置に一瞬だけ出す火花。ダメージや当たり判定は持たず、見た目だけを担当する。
		class BulletImpactSparkEffect : public GameObject
		{
		private:
			Vec3 m_Point;
			Vec3 m_Normal;
			float m_Elapsed = 0.0f;

			void UpdateSparkTransform(float t)
			{
				auto transform = GetComponent<Transform>(false);
				if (!transform)
				{
					return;
				}

				// 面の内側に埋まるとちらつくため、法線方向へ少しだけ浮かせる。
				transform->SetPosition(m_Point + (m_Normal * kBulletImpactSparkSurfaceOffset));
				// メッシュはローカルXY平面なので、ローカル+Zを着弾面の法線に合わせる。
				transform->SetQuaternion(bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_Normal));
				// 出現直後は小さく、消えながら少し広がる火花にする。
				const float scale = bsmUtil::Lerp(kBulletImpactSparkStartScale, kBulletImpactSparkEndScale, t);
				transform->SetScale(Vec3(scale, scale, scale));
			}

		public:
			BulletImpactSparkEffect(
				const std::shared_ptr<Stage>& stagePtr,
				const Vec3& point,
				const Vec3& normal) :
				GameObject(stagePtr),
				m_Point(point),
				m_Normal(NormalizeOrFallback(normal, Vec3(0.0f, 1.0f, 0.0f)))
			{
				m_transParam.position = m_Point + (m_Normal * kBulletImpactSparkSurfaceOffset);
				m_transParam.scale = Vec3(kBulletImpactSparkStartScale, kBulletImpactSparkStartScale, kBulletImpactSparkStartScale);
				m_transParam.quaternion = bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), m_Normal);
			}

			virtual ~BulletImpactSparkEffect() = default;

			virtual void OnCreate() override
			{
				SetAlphaActive(true);
				SetDrawActive(true);
				SetUpdateActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<SpPNTStaticDraw>();
				// Scene.cppで登録した三角片メッシュを使い、テクスチャ素材は既存の炎パーティクルを流用する。
				draw->AddBaseMesh(kBulletImpactSparkMeshKey);
				draw->AddBaseTexture(kMuzzleFlashTextureKey);
				draw->SetOwnShadowActive(false);
				draw->SetEmissive(Col4(1.8f, 1.2f, 0.25f, 1.0f));
				draw->SetDiffuse(Col4(1.0f, 0.78f, 0.22f, 0.82f));
				draw->SetSpecular(Col4(0.0f, 0.0f, 0.0f, 1.0f));

				UpdateSparkTransform(0.0f);
			}

			virtual void OnUpdate(double elapsedTime) override
			{
				m_Elapsed += static_cast<float>(elapsedTime);
				const float t = bsmUtil::Clamp(m_Elapsed / kBulletImpactSparkLifeTime, 0.0f, 1.0f);
				UpdateSparkTransform(t);

				if (auto draw = GetComponent<SpPNTStaticDraw>(false))
				{
					// フェードアウト時は発光も同時に弱め、画面に残像が残りすぎないようにする。
					const float fade = 1.0f - t;
					draw->SetEmissive(Col4(1.8f * fade, 1.2f * fade, 0.25f * fade, 1.0f));
					draw->SetDiffuse(Col4(1.0f, 0.78f, 0.22f, 0.82f * fade));
				}

				if (m_Elapsed >= kBulletImpactSparkLifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}
		};

		void SpawnBulletImpactSpark(
			const std::shared_ptr<Stage>& stage,
			const RaycastHit& hit,
			const Vec3& shotForward)
		{
			if (!stage)
			{
				return;
			}
			if (!bsmUtil::IsFiniteVec3(hit.m_Point))
			{
				return;
			}

			// Raycastの法線が取れない相手でも表示できるよう、弾の進行方向の逆を予備の法線にする。
			const Vec3 fallbackNormal = NormalizeOrFallback(shotForward * -1.0f, Vec3(0.0f, 1.0f, 0.0f));
			const Vec3 normal = NormalizeOrFallback(hit.m_Normal, fallbackNormal);
			if (!bsmUtil::IsFiniteVec3(normal))
			{
				return;
			}

			stage->AddGameObject<BulletImpactSparkEffect>(hit.m_Point, normal);
		}
		void ApplyHitscanDamage(
			const std::shared_ptr<GameObject>& shooter,
			const RaycastHit& hit,
			int damage)
		{
			if (damage <= 0)
			{
				return;
			}

			auto target = hit.m_Object.lock();
			if (!target || target->FindTag(L"Player"))
			{
				return;
			}

			DamageInfo info;
			info.m_Damage = damage;
			info.m_Instigator = shooter;
			info.m_HitPoint = hit.m_Point;
			info.m_HitNormal = hit.m_Normal;

			if (auto enemyProxy = std::dynamic_pointer_cast<EnemyCollisionProxy>(target))
			{
				enemyProxy->ApplyDamage(info);
				return;
			}

			if (auto hp = target->GetComponent<Health>(false))
			{
				hp->ApplyDamage(info);
			}
		}
	}

	PlayerWeapon::PlayerWeapon(const std::shared_ptr<Stage>& stagePtr, const std::shared_ptr<Player>& player) :
		GameObject(stagePtr),
		m_Player(player)
	{
		m_transParam.position = Vec3(0.0f, -100.0f, 0.0f);
	}

	void PlayerWeapon::OnCreate()
	{
		SetAlphaActive(false);
		SetShadowActive(false);

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->SetFogEnabled(true);
		ptrDraw->SetOwnShadowActive(false);
		ptrDraw->SetDiffuseColor(kPlayerBlasterColor);
		ptrDraw->SetLightingEnabled(true);

		const auto& meshes = BaseScene::Get()->GetModelMesh(kPlayerBlasterModelKey);
		ptrDraw->AddBaseModelMesh(meshes);
		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(std::wstring(kPlayerBlasterMaterialPrefix) + std::to_wstring(i));
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
		if (!player || !player->IsUpdateActive())
		{
			return false;
		}

		if (!player->IsSpawnIntroCharacterVisible())
		{
			// キャラ本体を消している登場待機中は、手に追従する武器だけが先に見えないようにする。
			m_HasStableTransform = false;
			return false;
		}

		auto weaponTransform = GetComponent<Transform>(false);
		if (!weaponTransform)
		{
			return false;
		}

		Mat4x4 weaponWorld;
		if (!TryGetPlayerBlasterWorldMatrix(player, weaponWorld))
		{
			return false;
		}

		Vec3 candidateScale, candidatePosition;
		Quat candidateRotation;
		weaponWorld.decompose(candidateScale, candidateRotation, candidatePosition);

		const bool invalidCandidate = candidatePosition.isNaN() || candidatePosition.isInfinite()
			|| candidateScale.isNaN() || candidateScale.isInfinite();

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
		const bool jumpedInIdle = m_HasStableTransform
			&& m_StableTransformIsIdle
			&& isIdle
			&& !playerTeleported
			&& (invalidCandidate || (candidatePosition - expectedStablePosition).length() > kWeaponAttachMaxIdleSnapDistance);

		if (jumpedInIdle)
		{
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

	void Player::AddBombAmmo(int amount)
	{
		if (amount <= 0)
		{
			return;
		}

		m_BombAmmo += amount;
		m_CurrentBullet = BulletType::Bomb;
	}

	void Player::OnUpdate(double elapsedTime)
	{
		const double rawElapsedTime = elapsedTime;
		bool hitStopActive = false;
		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			hitStopActive = gameStage->IsHitStopActive();
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (anim)
		{
			// AnimationStateBehaviorは共通Behavior更新で生のelapsedTimeを受け取るため、
			// プレイヤー側でゲーム内時間との比率を渡してヒットストップ対象にする。
			const double animationTimeScale = rawElapsedTime > 1e-8
				? elapsedTime / rawElapsedTime
				: 1.0;
			anim->SetPlaybackTimeScale(animationTimeScale);
		}

		if (m_IsDead)
		{

			if (!anim->IsFinished())
			{
				anim->ChangeAnimation(AnimState::Dead);
			}
			else
			{
				m_DeathAnimFinished = true;
			}

			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (transform && transform->GetPosition().y < kFallDeathY)
		{
			if (auto hp = GetComponent<Health>(false))
			{
				hp->SetHP(0);
			}
			m_IsDead = true;
			m_DeathAnimFinished = false;
			anim->ChangeAnimation(AnimState::Dead, true);
			return;
		}

		if (m_SpawnIntroActive)
		{
			// 登場演出中は移動・射撃入力を受けず、ワープホールから歩いて出る動きだけを見せる。
			if (IsSpawnIntroCharacterVisible())
			{
				anim->ChangeAnimation(AnimState::Sprint);
			}
			UpdateSpawnIntro(elapsedTime);
			if (m_BombPreview)
			{
				m_BombPreview->SetPreviewInput(
					false,
					Vec3(0.0f, 0.0f, 0.0f),
					Vec3(0.0f, 0.0f, 0.0f),
					Vec3(0.0f, 1.0f, 0.0f),
					false);
			}
			return;
		}

		if (!anim->IsPlayingAttack() || anim->IsFinished())
		{
			if (!m_IsGround)
			{
				anim->ChangeAnimation(AnimState::Jump);
			}
			else if (GetMoveVector().length() > 0.0f)
			{
				anim->ChangeAnimation(AnimState::Sprint);
			}
			else
			{
				anim->ChangeAnimation(AnimState::Idle);
			}
		}

		m_InputHandler.PushHandle(GetThis<Player>());

		// 移動
		MovePlayer(static_cast<float>(elapsedTime));
		ResolveSlopeCollision(elapsedTime);

		// ジャンプ（地面にいるときのみ）
		if (!hitStopActive && App::GetInputDevice().KeyDown(VK_SPACE))
		{
			OnPushA();
		}

		// フレームの最後に地面判定をリセット
		m_IsGround = false;

		// 発射クールダウン更新
		m_ShotCool -= elapsedTime;

		const auto& input = App::GetInputDevice();

		// --- 入力 ---
		const bool fireInput = input.KeyDown(VK_LBUTTON) || input.KeyDown('J');

		if (m_CurrentBullet == BulletType::Bomb && m_BombAmmo <= 0)
		{
			m_CurrentBullet = BulletType::Default;
		}

		const bool bombMode = IsBombMode();
		const bool canFire = !hitStopActive && fireInput && m_ShotCool <= 0.0;
		const bool traceNormalShot = canFire && !bombMode;
		const bool traceBombPreview = bombMode;

		// --- 狙い点計算（Raycast） ---
		Vec3 muzzle(0, 0, 0);

		Vec3 aimPointShot(0, 0, 0);
		RaycastHit shotHit;
		bool hasHitShot = false;

		Vec3 aimPointPreview(0, 0, 0);
		Vec3 hitNormalPreview(0, 1, 0);
		bool hasHitPreview = false;
		Quat shotRotPreview;

		const float bombAimMaxDist = m_BombPreview ? m_BombPreview->GetMaxRange() : 20.0f;

		if (m_MainCamera && m_CollisionManager)
		{
			auto trans = GetComponent<Transform>();

			// 銃口
			muzzle = trans->GetPosition()
				+ trans->GetForward() * 0.2f
				+ Vec3(0.0f, 0.055f, 0.0f);

			MuzzleSocketWorldTransform muzzleSocketTransform;
			if (TryGetPlayerBlasterMuzzleWorldTransform(GetThis<Player>(), muzzleSocketTransform))
			{
				muzzle = muzzleSocketTransform.position;
			}

			// カメラレイ（クロスヘア=画面中央）
			Vec3 rayOrigin = m_MainCamera->GetEye();
			Vec3 rayDir = m_MainCamera->GetAt() - m_MainCamera->GetEye();
			rayDir.normalize();

			// ----------------------------
			// ① 弾用（Enemyは拾う / Bulletは無視）
			// ----------------------------
			if (traceNormalShot)
			{
				Vec3 shotAimOrigin = rayOrigin;
				float shotAimRange = kNormalShotRange;

				// カメラが壁や木に密接している時、その手前の障害物を照準ヒットにすると
				// プレイヤーの銃口からは見えている敵ではなく、カメラ横の壁を撃ってしまう。
				// そのため通常射撃の照準レイだけは、銃口の奥行き付近まで進めた位置から始める。
				const float muzzleDepth = bsmUtil::dot(muzzle - rayOrigin, rayDir);
				if (std::isfinite(muzzleDepth) && muzzleDepth > kNormalShotCameraAimBacktrack)
				{
					const float startOffset = bsmUtil::Clamp(
						muzzleDepth - kNormalShotCameraAimBacktrack,
						0.0f,
						kNormalShotRange - 0.1f);
					shotAimOrigin = rayOrigin + rayDir * startOffset;
					shotAimRange = kNormalShotRange - startOffset;
				}

				aimPointShot = shotAimOrigin + rayDir * shotAimRange;

				if (m_CollisionManager->Raycast(shotAimOrigin, rayDir, shotAimRange, shotHit, GetThis<GameObject>(), { L"Bullet" }))
				{
					hasHitShot = true;
					aimPointShot = shotHit.m_Point;
				}

				if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
				{
					Vec3 generatedPoint(0.0f, 0.0f, 0.0f);
					Vec3 generatedNormal(0.0f, 1.0f, 0.0f);
					float generatedDistance = 0.0f;
					if (gameStage->TryRaycastGeneratedGround(shotAimOrigin, rayDir, shotAimRange, generatedPoint, generatedNormal, generatedDistance))
					{
						// 坂や高台の床は軽量な生成地形として管理しているため、通常のCollisionManagerだけでは拾えない場合がある。
						// 物理Raycastのヒットが手前にある場合は敵や壁を優先し、生成床面が手前なら着弾点として使う。
						const bool generatedIsNearest = !hasHitShot || generatedDistance < shotHit.m_Distance - 0.05f;
						if (generatedIsNearest)
						{
							hasHitShot = true;
							aimPointShot = generatedPoint;
							shotHit = RaycastHit{};
							shotHit.m_Point = generatedPoint;
							shotHit.m_Normal = generatedNormal;
							shotHit.m_Distance = generatedDistance;
						}
					}
				}

				// 照準決定ではカメラ近くの障害物を無視したが、実際の弾は銃口から出る。
				// 銃口と狙い点の間に壁がある場合は、そこで止めることで壁越し射撃を防ぐ。
				Vec3 muzzleRay = aimPointShot - muzzle;
				const float muzzleRayLength = muzzleRay.length();
				if (muzzleRayLength > 1e-4f)
				{
					muzzleRay.normalize();

					RaycastHit muzzleHit;
					if (m_CollisionManager->Raycast(
						muzzle,
						muzzleRay,
						muzzleRayLength + kNormalShotMuzzleBlockMargin,
						muzzleHit,
						GetThis<GameObject>(),
						{ L"Bullet" }))
					{
						hasHitShot = true;
						shotHit = muzzleHit;
						aimPointShot = muzzleHit.m_Point;
					}
				}
			}

			// ----------------------------
			// ② プレビュー用（Enemy/Bulletを無視）
			// ----------------------------
			if (traceBombPreview)
			{
				RaycastHit hit;
				aimPointPreview = rayOrigin + rayDir * bombAimMaxDist;

				if (m_CollisionManager->Raycast(rayOrigin, rayDir, bombAimMaxDist, hit, GetThis<GameObject>(), { L"Bullet", L"Enemy" }))
				{
					hasHitPreview = true;
					aimPointPreview = hit.m_Point;
					hitNormalPreview = hit.m_Normal;
				}

				if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
				{
					Vec3 generatedPoint(0.0f, 0.0f, 0.0f);
					Vec3 generatedNormal(0.0f, 1.0f, 0.0f);
					float generatedDistance = 0.0f;
					if (gameStage->TryRaycastGeneratedGround(rayOrigin, rayDir, bombAimMaxDist, generatedPoint, generatedNormal, generatedDistance))
					{
						const bool physicalHitIsWall = hasHitPreview && hitNormalPreview.y < 0.45f;
						const bool wallIsInFront = physicalHitIsWall && hit.m_Distance <= generatedDistance + 0.1f;
						const bool generatedCanReplacePhysical = !hasHitPreview || generatedDistance <= hit.m_Distance + 0.25f || hitNormalPreview.y > 0.45f;
						if (!wallIsInFront && generatedCanReplacePhysical)
						{
							aimPointPreview = generatedPoint;
							hitNormalPreview = generatedNormal;
							hasHitPreview = true;
						}
					}
				}

				// rot（銃口→aimPointPreview）
				Vec3 shotDir = aimPointPreview - muzzle;
				if (shotDir.length() > 1e-6f)
				{
					shotDir.normalize();
					const Vec3 localForward = Vec3(0, 0, 1);
					shotRotPreview = bsmUtil::MakeFromToQuat(localForward, shotDir);
				}
			}
		}

		// --- BombAimPreview 更新 ---
		if (m_BombPreview)
		{
			m_BombPreview->SetPreviewInput(
				bombMode,
				muzzle,
				aimPointPreview,
				hitNormalPreview,
				hasHitPreview
			);
		}

		// --- 発射 ---
		if (canFire)
		{
			// ボム
			if (bombMode)
			{
				auto bulletMgr = GetStage()->GetSharedGameObjectEx<BulletManager>(L"BulletManager", false);
				const Vec3 scale = kBombProjectileScale;

				if (bulletMgr)
				{
					bulletMgr->FireEx<BombBullet>(muzzle, shotRotPreview, scale,
									[&](BombBullet& b)
									{
										// プレビューに渡した値をそのまま実弾へ（Enemy無視の狙い点）
										b.SetAimFromPreview(aimPointPreview, m_BombPreview->GetTuning(), hitNormalPreview, hasHitPreview);
									});
				}
				GameAudio::Instance().PlaySound(GameSoundId::BombThrow);

				m_ShotCool = 1.0;
				if (m_BombAmmo > 0)
				{
					--m_BombAmmo;
				}
				if (m_BombAmmo <= 0)
				{
					m_BombAmmo = 0;
					m_CurrentBullet = BulletType::Default;
				}


				// 攻撃方向を向く
				Vec3 attackDir = aimPointPreview - GetComponent<Transform>()->GetPosition();
				attackDir.y = 0.0f;

				if (attackDir.length() > 1e-6f)
				{
					attackDir.normalize();

					auto util = GetBehavior<UtilBehavior>();
					util->RotToHead(attackDir, 1.0f);
				}
				// 攻撃アニメーション
				anim->ChangeAnimation(AnimState::AttackMeleeLeft);
			}
			// 通常弾
			else
			{
				GameAudio::Instance().PlaySound(GameSoundId::PlayerShot);

				if (hasHitShot)
				{
					ApplyHitscanDamage(GetThis<GameObject>(), shotHit, kNormalShotDamage);
				}

				// 攻撃方向を向く
				Vec3 attackDir = aimPointShot - GetComponent<Transform>()->GetPosition();
				attackDir.y = 0.0f;

				if (attackDir.length() > 1e-6f)
				{
					attackDir.normalize();

					auto util = GetBehavior<UtilBehavior>();
					util->RotToHead(attackDir, 1.0f);
				}

				m_ShotCool = kNormalShotCooldown;
				Vec3 flashForward = aimPointShot - muzzle;
				if (flashForward.length() > 1e-6f)
				{
					flashForward.normalize();
				}
				else
				{
					flashForward = GetComponent<Transform>()->GetForward();
				}
				SpawnMuzzleFlash(GetThis<Player>(), muzzle, flashForward);
				const float hitVisualDistance = hasHitShot ? (aimPointShot - muzzle).length() : kNormalShotRange;
				if (hasHitShot && hitVisualDistance >= kNormalShotMinVisualEffectDistance)
				{
					// Raycastの着弾位置に小さいスパークを出す。弾道表示と同じくゲーム判定には影響しない。
					SpawnBulletImpactSpark(GetStage(false), shotHit, flashForward);
				}
				// 命中処理とは別に、銃口から命中点へ見た目だけの弾道を飛ばす。
				SpawnBulletTracer(GetStage(false), muzzle, aimPointShot, flashForward);
				anim->ChangeAnimation(AnimState::HoldingRightShoot, true);
			}
		}
	}
}
