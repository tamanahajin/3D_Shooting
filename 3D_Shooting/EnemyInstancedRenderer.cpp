/*!
@file EnemyInstancedRenderer.cpp
@brief 敵のインスタンシング描画

EnemyBatchControllerが保持する敵配列から描画用データだけを受け取り、InstancedSkinnedDrawに渡す。
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
		const std::shared_ptr<EnemyBatchController>& controller) :
		GameObject(stage),
		m_Controller(controller)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	/*!
	@brief 敵モデル用の InstancedSkinnedDraw を作成する
	*/
	void EnemyInstancedRenderer::OnCreate()
	{
		m_Draw = AddComponent<InstancedSkinnedDraw>();
		m_Draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_Draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_Draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	/*!
	@brief 保持している敵バッチから描画インスタンスを受け取り、GPU用バッファを更新する
	@param elapsedTime 経過時間。この処理では使用しない
	*/
	void EnemyInstancedRenderer::OnUpdate2(double elapsedTime)
	{
		UNREFERENCED_PARAMETER(elapsedTime);

		if (!m_Draw)
		{
			return;
		}

		auto controller = m_Controller.lock();
		if (controller)
		{
			controller->FillInstanceSources(m_InstanceSources, m_ModelOffset);
		}
		else
		{
			m_InstanceSources.clear();
		}

		m_Draw->SetInstances(m_InstanceSources);
		m_Draw->BuildInstanceBuffer();
	}

	/*!
	@brief 敵配列からスキンメッシュ描画用インスタンス配列を作成する
	@param outSources 出力先のインスタンス配列
	@param modelOffset モデル原点補正

	被弾押され演出はここで描画用行列にだけ反映し、敵の実座標やコリジョンには影響させない。
	*/
	void EnemyBatchController::FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const
	{
		outSources.clear();
		outSources.reserve(m_Enemies.size());

		for (const auto& enemy : m_Enemies)
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

