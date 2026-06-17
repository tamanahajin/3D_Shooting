/*!
@file EnemyRenderers.cpp
@brief 敵のインスタンシング描画と比較用個別描画

EnemyControllerが保持する敵配列から描画用データだけを受け取り、InstancedSkinnedDrawに渡す。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace {
		/*!
		@brief 被弾押され演出の現在強度を計算する
		@param timer 残り時間
		@param duration 全体時間
		@return 0.0から1.0の演出強度
		*/
		float GetHitPushPower(double timer, double duration)
		{
			if (timer <= 0.0 || duration <= 0.0)
			{
				return 0.0f;
			}

			// remainingRateは「押され演出があとどれだけ残っているか」を0.0～1.0で表す。
			// 被弾直後を最大にして、その後すぐ元の位置へ戻すための基準値。
			float remainingRate = static_cast<float>(timer / duration);
			remainingRate = bsmUtil::Clamp(remainingRate, 0.0f, 1.0f);

			// 二乗しておくと、最初だけ強く押されて後半は素早く収束する。
			// 左右への往復動作ではなく「一瞬押された」見た目にするため、sin波は使わない。
			return remainingRate * remainingRate;
		}

		/*!
		@brief 被弾押され演出の描画位置オフセットを計算する
		@param direction 押される水平方向
		@param distance 最大押し戻し距離
		@param pushPower 現在の演出強度
		@return 描画用の位置オフセット
		*/
		Vec3 GetHitPushOffset(const Vec3& direction, float distance, float pushPower)
		{
			// pushPowerが0なら位置補正は不要。描画位置をそのまま使う。
			if (pushPower == 0.0f || distance <= 0.0f)
			{
				return Vec3(0.0f, 0.0f, 0.0f);
			}

			// directionは「攻撃者から敵へ向かう水平ベクトル」。
			// その方向へ少しだけ描画位置をずらすと、敵が撃たれて後ろに押されたように見える。
			Vec3 pushDirection = direction;
			pushDirection.y = 0.0f;
			if (!bsmUtil::IsFiniteVec3(pushDirection) || bsmUtil::lengthSqr(pushDirection) <= 1e-6f)
			{
				// directionが不正またはほぼゼロの場合でもNaNを出さないよう、固定方向に退避する。
				pushDirection = Vec3(1.0f, 0.0f, 0.0f);
			}

			pushDirection.normalize();
			// distanceは最大押し戻し量、pushPowerは現在フレームの戻り具合。
			// 実座標ではなく描画位置だけに加えるため、移動AIやコリジョンには影響しない。
			return pushDirection * (distance * pushPower);
		}

		/*!
		@brief 被弾押され演出の描画用傾き回転を計算する
		@param direction 押される水平方向
		@param leanAngle 最大傾き角
		@param pushPower 現在の演出強度
		@return 描画用の追加回転
		*/
		Quat GetHitPushRotation(const Vec3& direction, float leanAngle, float pushPower)
		{
			Quat rotation;
			rotation.identity();
			if (pushPower == 0.0f || leanAngle <= 0.0f)
			{
				return rotation;
			}

			// 押された方向と直交する軸で少し傾ける。
			// 位置補正だけより「体が押された」印象が出るが、こちらも描画用の回転だけに限定する。
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

	/*!
	@brief 敵インスタンシング描画オブジェクトを生成する
	@param stage 所属するステージ
	*/
	EnemyInstancedRenderer::EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	/*!
	@brief 敵インスタンシング描画オブジェクトを生成し、参照先コントローラを保持する
	@param stage 所属するステージ
	@param controller 描画元になる敵バッチコントローラ
	*/
	EnemyInstancedRenderer::EnemyInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller) :
		GameObject(stage),
		m_controller(controller)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	/*!
	@brief 敵モデル用の InstancedSkinnedDraw を作成する
	*/
	void EnemyInstancedRenderer::OnCreate()
	{
		m_draw = AddComponent<InstancedSkinnedDraw>();
		m_draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	/*!
	@brief 保持している敵バッチから描画インスタンスを受け取り、GPU用バッファを更新する
	@param elapsedTime 経過時間。この処理では使用しない
	*/
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

	/*!
	@brief インスタンシング描画の有効状態を切り替える
	@param enabled 有効にする場合は true
	*/
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

	/*!
	@brief 通常描画プロキシを生成する
	@param stage 所属するステージ
	*/
	EnemyIndividualDrawProxy::EnemyIndividualDrawProxy(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyIndividualDrawProxy::~EnemyIndividualDrawProxy() {}

	/*!
	@brief 敵1体分の通常スキンメッシュ描画を作成する
	*/
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

	/*!
	@brief インスタンス描画用データを通常描画用 Transform とアニメーションへ反映する
	@param source EnemyController が作成した描画用データ
	*/
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

	/*!
	@brief プロキシの描画・更新を切り替える
	@param enabled 有効にする場合は true
	*/
	void EnemyIndividualDrawProxy::SetRenderingEnabled(bool enabled)
	{
		SetUpdateActive(enabled);
		SetDrawActive(enabled);
		SetShadowActive(false);
	}

	/*!
	@brief 通常描画レンダラーを生成する
	@param stage 所属するステージ
	*/
	EnemyIndividualRenderer::EnemyIndividualRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	/*!
	@brief 通常描画レンダラーを生成し、参照先コントローラを保持する
	@param stage 所属するステージ
	@param controller 描画元になる敵バッチコントローラ
	*/
	EnemyIndividualRenderer::EnemyIndividualRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller) :
		GameObject(stage),
		m_controller(controller)
	{
	}

	EnemyIndividualRenderer::~EnemyIndividualRenderer() {}

	/*!
	@brief 通常描画レンダラー本体を初期化する
	*/
	void EnemyIndividualRenderer::OnCreate()
	{
		SetDrawActive(false);
		SetShadowActive(false);
		SetUpdateActive(m_renderingEnabled);
		AddTag(L"EnemyRenderer");
		AddTag(L"EnemyIndividualRenderer");
	}

	/*!
	@brief 描画に必要なプロキシ数を揃える
	@param requiredCount 必要なプロキシ数
	*/
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

	/*!
	@brief 生成済みプロキシをステージから外す
	*/
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

	/*!
	@brief 保持している敵バッチから描画データを受け取り、通常描画プロキシへ同期する
	@param elapsedTime 経過時間。この処理では使用しない
	*/
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

	/*!
	@brief 通常描画経路の有効状態を切り替える
	@param enabled 有効にする場合は true
	*/
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

	/*!
	@brief 敵配列からスキンメッシュ描画用インスタンス配列を作成する
	@param outSources 出力先のインスタンス配列
	@param modelOffset モデル原点補正

	被弾押され演出はここで描画用行列にだけ反映し、敵の実座標やコリジョンには影響させない。
	*/
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

