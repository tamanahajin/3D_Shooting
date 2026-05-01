#pragma once
#include "stdafx.h"
#include <vector>
#include <cmath>
#include "BombTuning.h"

namespace shooting {

	class BombPreviewMarker : public GameObject
	{
	private:
		std::wstring m_MeshKey;
		Col4 m_Color;

	public:
		BombPreviewMarker(
			const std::shared_ptr<Stage>& stagePtr,
			const TransParam& param,
			const std::wstring& meshKey,
			const Col4& color)
			: GameObject(stagePtr)
			, m_MeshKey(meshKey)
			, m_Color(color)
		{
			m_transParam = param;
		}

		void OnCreate() override
		{
			SetAlphaActive(true);
			SetShadowActive(false);

			auto draw = AddComponent<BcPNTStaticDraw>();
			draw->AddBaseMesh(m_MeshKey);
			draw->SetOwnShadowActive(false);
			draw->SetFogEnabled(false);
			draw->SetLightingEnabled(false);
			draw->SetDiffuseColor(m_Color);

			SetUpdateActive(false);
		}

		void OnUpdate(double /*elapsedTime*/) override {}
	};

	class BombAimPreview : public Component
	{
	private:
		std::vector<std::shared_ptr<BombPreviewMarker>> m_PathMarkers;
		std::shared_ptr<BombPreviewMarker> m_AreaMarker;

		int   m_LineSegmentCount = 28;
		float m_LineWidth = 0.05f;
		float m_SurfaceLift = 0.05f;
		float m_AreaRadiusScale = 0.90f;

		bool  m_Visible = false;
		Vec3  m_Start = Vec3(0, 0, 0);
		Vec3  m_Target = Vec3(0, 0, 0);
		Vec3  m_HitNormal = Vec3(0, 1, 0);
		bool  m_HasHit = false;

		BombTuning m_Tuning{};
		float m_MaxRange = 500.0f;

		bool  m_MarkersShown = false;
		bool  m_HasCachedLayout = false;
		float m_RebuildTimer = 0.0f;
		float m_RebuildInterval = 0.05f;
		float m_StartRebuildThreshold = 0.05f;
		float m_TargetRebuildThreshold = 0.10f;

		Vec3  m_LastBuiltStart = Vec3(0, 0, 0);
		Vec3  m_LastBuiltTarget = Vec3(0, 0, 0);

	public:
		explicit BombAimPreview(const std::shared_ptr<GameObject>& go)
			: Component(go)
		{
		}

		const BombTuning& GetTuning() const { return m_Tuning; }
		void SetTuning(const BombTuning& t) { m_Tuning = t; }

		void SetMaxRange(float maxRange) { m_MaxRange = maxRange; }
		float GetMaxRange() const { return m_MaxRange; }

		void OnCreate() override;

		void SetPreviewInput(
			bool visible,
			const Vec3& start,
			const Vec3& target,
			const Vec3& hitNormal,
			bool hasHit
		);

		void OnUpdate(double elapsedTime) override;

	private:
		static Vec3 SafeNormalize(const Vec3& v);

		bool SolveBallistic_ApexHeight(
			const Vec3& p0,
			const Vec3& p1,
			const Vec3& gravity,
			float arcHeight,
			Vec3& outV0,
			float& outT
		) const;

		static Vec3 SamplePos(const Vec3& p0, const Vec3& v0, const Vec3& g, float t);
		void SetMarkersVisible(bool v);
	};

}
