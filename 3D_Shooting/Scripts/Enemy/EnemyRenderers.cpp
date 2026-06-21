#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace {
		const std::wstring kEnemyBlobShadowMeshKey = L"ENEMY_BLOB_SHADOW_MODEL";
		const Col4 kEnemyBlobShadowColor(0.0f, 0.0f, 0.0f, 0.24f);
		const Vec3 kEnemyBlobShadowScale(0.42f, 1.0f, 0.32f);
		const float kEnemyBlobShadowLift = 0.06f;
		const float kEnemyModelOffsetCompensation = 0.35f;

		float GetHitPushPower(double timer, double duration)
		{
			if (timer <= 0.0 || duration <= 0.0)
			{
				return 0.0f;
			}

			// 被弾直後を最大にして、その後すぐ元の位置へ戻す。
			float remainingRate = static_cast<float>(timer / duration);
			remainingRate = bsmUtil::Clamp(remainingRate, 0.0f, 1.0f);

			// 二乗で後半を早く収束させ、「一瞬押された」見た目に寄せる。
			return remainingRate * remainingRate;
		}

		Vec3 GetHitPushOffset(const Vec3& direction, float distance, float pushPower)
		{
			if (pushPower == 0.0f || distance <= 0.0f)
			{
				return Vec3(0.0f, 0.0f, 0.0f);
			}

			Vec3 pushDirection = direction;
			pushDirection.y = 0.0f;
			if (!bsmUtil::IsFiniteVec3(pushDirection) || bsmUtil::lengthSqr(pushDirection) <= 1e-6f)
			{
				// directionが不正またはほぼゼロの場合でもNaNを出さないよう、固定方向に退避する。
				pushDirection = Vec3(1.0f, 0.0f, 0.0f);
			}

			pushDirection.normalize();
			// 実座標ではなく描画位置だけに加えるため、移動AIやコリジョンには影響しない。
			return pushDirection * (distance * pushPower);
		}

		Quat GetHitPushRotation(const Vec3& direction, float leanAngle, float pushPower)
		{
			Quat rotation;
			rotation.identity();
			if (pushPower == 0.0f || leanAngle <= 0.0f)
			{
				return rotation;
			}

			// 位置補正だけより「体が押された」印象を出すため、描画用の回転だけを少し足す。
			Vec3 axis(direction.z, 0.0f, -direction.x);
			if (!bsmUtil::IsFiniteVec3(axis) || bsmUtil::lengthSqr(axis) <= 1e-6f)
			{
				axis = Vec3(1.0f, 0.0f, 0.0f);
			}

			axis.normalize();
			rotation.rotationAxis(axis, -leanAngle * pushPower);
			return rotation;
		}
	}

	EnemyInstancedRenderer::EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyInstancedRenderer::EnemyInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller) :
		GameObject(stage),
		m_controller(controller)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	void EnemyInstancedRenderer::OnCreate()
	{
		m_draw = AddComponent<InstancedSkinnedDraw>();
		m_draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	void EnemyInstancedRenderer::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		if (!m_renderingEnabled || !m_draw)
		{
			return;
		}

		auto controller = m_controller.lock();
		if (controller)
		{
			controller->FillInstanceSources(m_instanceSources, m_modelOffset);
		}
		else
		{
			m_instanceSources.clear();
		}

		m_draw->SetInstances(m_instanceSources);
		m_draw->BuildInstanceBuffer();
	}

	void EnemyInstancedRenderer::SetRenderingEnabled(bool enabled)
	{
		if (m_renderingEnabled == enabled)
		{
			return;
		}

		m_renderingEnabled = enabled;
		SetUpdateActive(enabled);
		SetDrawActive(enabled);

		if (!enabled && m_draw)
		{
			// 無効化した瞬間に古いインスタンスが残らないよう、描画用バッファも空にしておく。
			m_instanceSources.clear();
			m_draw->SetInstances(m_instanceSources);
			m_draw->BuildInstanceBuffer();
		}
	}

	EnemyIndividualDrawProxy::EnemyIndividualDrawProxy(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyIndividualDrawProxy::~EnemyIndividualDrawProxy() {}

	void EnemyIndividualDrawProxy::OnCreate()
	{
		SetBatchUpdateManaged(true);
		SetShadowActive(false);

		m_draw = AddComponent<BcPNTBoneDraw>();
		m_draw->AddBaseMesh(L"ENEMY_MODEL_SKINNED");
		m_draw->AddBaseTexture(L"CHARACTER_TEXTURE_SKINNED");
		m_draw->SetOwnShadowActive(false);
		m_draw->SetModelOffset(Vec3(0.0f, 0.0f, 0.0f));

		AddTag(L"EnemyIndividualDrawProxy");
		SetRenderingEnabled(false);
	}

	void EnemyIndividualDrawProxy::ApplyInstanceSource(const SkinnedInstanceSource& source)
	{
		Vec3 scale;
		Quat rotation;
		Vec3 position;
		source.world.decompose(scale, rotation, position);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			transform->SetScale(scale);
			transform->SetQuaternion(rotation);
			transform->SetPosition(position);
		}

		if (m_draw)
		{
			m_draw->SetAnimationIndex(source.animationIndex);
			m_draw->UpdateAnimation(static_cast<double>(source.animationTime));
		}

		SetRenderingEnabled(true);
	}

	void EnemyIndividualDrawProxy::SetRenderingEnabled(bool enabled)
	{
		SetUpdateActive(enabled);
		SetDrawActive(enabled);
		SetShadowActive(false);
	}

	EnemyIndividualRenderer::EnemyIndividualRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyIndividualRenderer::EnemyIndividualRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller) :
		GameObject(stage),
		m_controller(controller)
	{
	}

	EnemyIndividualRenderer::~EnemyIndividualRenderer() {}

	void EnemyIndividualRenderer::OnCreate()
	{
		SetDrawActive(false);
		SetShadowActive(false);
		SetUpdateActive(m_renderingEnabled);
		AddTag(L"EnemyRenderer");
		AddTag(L"EnemyIndividualRenderer");
	}

	void EnemyIndividualRenderer::ResizeDrawProxies(size_t requiredCount)
	{
		auto stage = GetStage(false);
		if (!stage)
		{
			return;
		}

		while (m_drawProxies.size() < requiredCount)
		{
			auto proxy = stage->AddGameObject<EnemyIndividualDrawProxy>();
			m_drawProxies.push_back(proxy);
		}

		if (m_drawProxies.size() <= requiredCount)
		{
			return;
		}

		// 敵が死亡して描画数が減った場合は、余った通常描画プロキシをステージから外す。
		// 配列側の敵インデックスとは対応させず、表示する順番だけを詰め直す。
		for (size_t i = requiredCount; i < m_drawProxies.size(); ++i)
		{
			if (m_drawProxies[i])
			{
				m_drawProxies[i]->SetRenderingEnabled(false);
				stage->RemoveGameObject(m_drawProxies[i]);
			}
		}
		m_drawProxies.resize(requiredCount);
	}

	void EnemyIndividualRenderer::ClearDrawProxies()
	{
		auto stage = GetStage(false);
		for (auto& proxy : m_drawProxies)
		{
			if (!proxy)
			{
				continue;
			}

			proxy->SetRenderingEnabled(false);
			if (stage)
			{
				stage->RemoveGameObject(proxy);
			}
		}
		m_drawProxies.clear();
	}

	void EnemyIndividualRenderer::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		if (!m_renderingEnabled)
		{
			return;
		}

		auto controller = m_controller.lock();
		if (controller)
		{
			controller->FillInstanceSources(m_instanceSources, m_modelOffset);
		}
		else
		{
			m_instanceSources.clear();
		}

		ResizeDrawProxies(m_instanceSources.size());
		const size_t drawCount = (m_instanceSources.size() < m_drawProxies.size())
			? m_instanceSources.size()
			: m_drawProxies.size();
		for (size_t i = 0; i < drawCount; ++i)
		{
			if (m_drawProxies[i])
			{
				m_drawProxies[i]->ApplyInstanceSource(m_instanceSources[i]);
			}
		}
	}

	void EnemyIndividualRenderer::SetRenderingEnabled(bool enabled)
	{
		if (m_renderingEnabled == enabled)
		{
			return;
		}

		m_renderingEnabled = enabled;
		SetUpdateActive(enabled);
		SetDrawActive(false);
		SetShadowActive(false);

		if (!enabled)
		{
			m_instanceSources.clear();
			ClearDrawProxies();
		}
	}

	void EnemyController::FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const
	{
		outSources.clear();
		outSources.reserve(m_enemies.size());

		for (const auto& enemy : m_enemies)
		{
			if (!enemy.active)
			{
				continue;
			}

			const float hitPushPower = GetHitPushPower(enemy.hitPushTimer, enemy.hitPushDuration);
			Vec3 drawPosition = enemy.position + modelOffset + GetHitPushOffset(enemy.hitPushDirection, enemy.hitPushDistance, hitPushPower);
			Quat drawRotation = enemy.rotation;
			if (hitPushPower != 0.0f)
			{
				// 実座標やコリジョンは動かさず、描画用のワールド行列だけ一瞬押された形にする。
				Quat pushRotation = GetHitPushRotation(enemy.hitPushDirection, enemy.hitPushLeanAngle, hitPushPower);
				drawRotation = pushRotation * drawRotation;
				drawRotation.normalize();
			}

			if (enemy.knockbackSpinSpeed != 0.0f)
			{
				// 爆風回転は描画行列にだけ合成する。enemy.rotation は移動方向を向くための基準として残す。
				drawRotation = enemy.knockbackSpinRotation * drawRotation;
				drawRotation.normalize();
			}

			Mat4x4 world;
			world.affineTransformation(
				enemy.status.modelScale,
				Vec3(0.0f, 0.0f, 0.0f),
				drawRotation,
				drawPosition);

			SkinnedInstanceSource source{};
			source.world = world;
			source.animationIndex = static_cast<unsigned int>(enemy.animationState);
			source.animationTime = static_cast<float>(enemy.animationTime);
			source.damage = GetDamageFlashValue(enemy);
			outSources.push_back(source);
		}
	}
}

