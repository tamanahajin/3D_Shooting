#pragma once
#include "stdafx.h"
#include <vector>
#include <cmath>

namespace shooting {

	/// 表示用の小さい球（予測線・リングに使う）
	class PreviewDot : public GameObject
	{
	public:
		explicit PreviewDot(const std::shared_ptr<Stage>& stagePtr, const TransParam& param)
			: GameObject(stagePtr)
		{
			m_transParam = param;
		}

		void OnCreate() override
		{
			// 衝突は不要
			// 見た目だけ欲しいので Draw だけ付ける
			auto draw = AddComponent<BcPNTStaticDraw>();
			draw->AddBaseMesh(L"DEFAULT_SPHERE");
			//draw->AddBaseTexture(L"WALL_TX"); // 手持ちの適当なテクスチャでOK
			draw->SetOwnShadowActive(false);

			// Stage更新は不要（位置は外からTransformを触って動かす）
			SetUpdateActive(false);
		}

		void OnUpdate(double /*elapsedTime*/) override {}
	};

	/// <summary>
	/// ボム弾の発射前プレビュー
	/// ・軌道：点列
	/// ・爆発範囲：円（リング）を点で表現
	/// </summary>
	class BombAimPreview : public Component
	{
	private:
		// 表示用ドット
		std::vector<std::shared_ptr<PreviewDot>> m_PathDots;
		std::vector<std::shared_ptr<PreviewDot>> m_RingDots;

		// パラメータ
		int   m_PathCount = 20;
		int   m_RingCount = 32;
		float m_PathDotScale = 0.06f;
		float m_RingDotScale = 0.08f;

		// 予測の入力（Player側が毎フレームセットする）
		bool  m_Visible = false;
		Vec3  m_Start = Vec3(0, 0, 0);     // muzzle
		Vec3  m_Target = Vec3(0, 0, 0);    // 着弾点
		Vec3  m_HitNormal = Vec3(0, 1, 0); // 地面法線
		bool  m_HasHit = false;

		// 弾道パラメータ（BombBullet と揃える）
		Vec3  m_Gravity = Vec3(0, -9.8f, 0);
		float m_ArcHeight = 2.5f;
		float m_ExplosionRadius = 2.0f; // 見た目の円の半径（実際の爆発範囲と揃える）
	public:
		explicit BombAimPreview(const std::shared_ptr<GameObject>& go)
			: Component(go)
		{
		}

		void OnCreate() override;

		/// Player側から毎フレームこれを呼ぶ想定
		void SetPreviewInput(
			bool visible,
			const Vec3& start,
			const Vec3& target,
			const Vec3& hitNormal,
			bool hasHit,
			float arcHeight,
			const Vec3& gravity,
			float explosionRadius
		);

		void OnUpdate(double elapsedTime) override;

	private:
		static Vec3 SafeNormalize(const Vec3& v);
		static void MakeTangentBasis(const Vec3& n, Vec3& outT, Vec3& outB);

		// 弾道（頂点高さ方式）を解く：v0 と飛行時間 T
		bool SolveBallistic_ApexHeight(
			const Vec3& p0,
			const Vec3& p1,
			const Vec3& gravity,
			float arcHeight,
			Vec3& outV0,
			float& outT
		) const;

		static Vec3 SamplePos(const Vec3& p0, const Vec3& v0, const Vec3& g, float t);
		void SetDotsVisible(bool v);
	};

}
