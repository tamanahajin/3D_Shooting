#include "stdafx.h"
#include "Project.h"
#include "BombAimPreview.h"

namespace shooting {

	namespace
	{
		const std::wstring kPreviewAreaMeshKey = L"BOMB_PREVIEW_DISC";
		const std::wstring kPreviewLineMeshKey = L"BOMB_PREVIEW_LINE";
		const Col4 kPreviewAreaColor(1.0f, 1.0f, 1.0f, 0.16f);
		const Col4 kPreviewLineColor(1.0f, 0.88f, 0.08f, 0.42f);

		float LengthSq(const Vec3& v)
		{
			return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
		}

		void SetMarkerTransform(
			const std::shared_ptr<BombPreviewMarker>& marker,
			const Vec3& position,
			const Quat& rotation,
			const Vec3& scale)
		{
			if (!marker) return;

			if (auto tr = marker->GetComponent<Transform>())
			{
				tr->SetPosition(position);
				tr->SetQuaternion(rotation);
				tr->SetScale(scale);
			}
		}

		void SetMarkerTransform(
			const std::shared_ptr<BombPreviewMarker>& marker,
			const Vec3& position,
			const Quat& rotation,
			float scale)
		{
			SetMarkerTransform(marker, position, rotation, Vec3(scale, scale, scale));
		}
	}

	void BombAimPreview::OnCreate()
	{
		auto stage = GetGameObject()->GetStage();
		if (!stage) return;

		TransParam tp;
		tp.position = Vec3(0, -100.0f, 0);
		tp.scale = Vec3(m_LineWidth, 1.0f, 1.0f);
		tp.quaternion = Quat();

		m_PathMarkers.reserve(m_LineSegmentCount);
		for (int i = 0; i < m_LineSegmentCount; ++i)
		{
			auto marker = stage->AddGameObject<BombPreviewMarker>(tp, kPreviewLineMeshKey, kPreviewLineColor);
			marker->SetDrawActive(false);
			m_PathMarkers.push_back(marker);
		}

		tp.scale = Vec3(1.0f, 1.0f, 1.0f);
		m_AreaMarker = stage->AddGameObject<BombPreviewMarker>(tp, kPreviewAreaMeshKey, kPreviewAreaColor);
		m_AreaMarker->SetDrawActive(false);
	}

	void BombAimPreview::SetPreviewInput(
		bool visible,
		const Vec3& start,
		const Vec3& target,
		const Vec3& hitNormal,
		bool hasHit)
	{
		m_Visible = visible;
		m_Start = start;

		Vec3 dirXZ(target.x - start.x, 0.0f, target.z - start.z);
		const float distXZ = dirXZ.length();

		if (distXZ > m_MaxRange)
		{
			if (distXZ > 1e-6f)
			{
				dirXZ /= distXZ;
			}

			m_Target = start + (dirXZ * m_MaxRange);
			m_Target.y = target.y;
			m_HitNormal = Vec3(0, 1, 0);
			m_HasHit = false;
		}
		else
		{
			m_Target = target;
			m_HitNormal = hitNormal;
			m_HasHit = hasHit;
		}
	}

	void BombAimPreview::OnUpdate(double elapsedTime)
	{
		if (!m_Visible)
		{
			SetMarkersVisible(false);
			m_HasCachedLayout = false;
			m_RebuildTimer = 0.0f;
			return;
		}

		m_RebuildTimer += static_cast<float>(elapsedTime);

		const float startThresholdSq = m_StartRebuildThreshold * m_StartRebuildThreshold;
		const float targetThresholdSq = m_TargetRebuildThreshold * m_TargetRebuildThreshold;
		const bool startMoved = (LengthSq(m_Start - m_LastBuiltStart) > startThresholdSq);
		const bool targetMoved = (LengthSq(m_Target - m_LastBuiltTarget) > targetThresholdSq);
		const bool needRebuild = (!m_HasCachedLayout)
			|| startMoved
			|| targetMoved
			|| (m_RebuildTimer >= m_RebuildInterval);

		if (!needRebuild)
		{
			SetMarkersVisible(true);
			return;
		}

		m_RebuildTimer = 0.0f;
		m_LastBuiltStart = m_Start;
		m_LastBuiltTarget = m_Target;
		m_HasCachedLayout = true;

		auto stage = GetGameObject()->GetStage();
		auto colMgr = stage ? stage->GetCollisionManager() : nullptr;

		Vec3 ringCenter = m_Target;
		Vec3 ringNormal = Vec3(0, 1, 0);

		if (colMgr)
		{
			const float probeHeight = 5.0f;
			const float probeDistance = bsmUtil::Max(20.0f, std::fabs(m_Start.y - m_Target.y) + 20.0f);
			const Vec3 probeStart(m_Target.x, bsmUtil::Max(m_Start.y, m_Target.y) + probeHeight, m_Target.z);
			const Vec3 down(0, -1, 0);

			RaycastHit hit;
			if (colMgr->SphereCast(probeStart, down, probeDistance, 0.1f, hit, GetGameObject(), { L"Bullet", L"Enemy" }))
			{
				ringCenter = hit.m_Point;
				ringNormal = SafeNormalize(hit.m_Normal);
			}
			else if (m_HasHit)
			{
				ringCenter = m_Target;
				ringNormal = SafeNormalize(m_HitNormal);
			}
		}
		else if (m_HasHit)
		{
			ringCenter = m_Target;
			ringNormal = SafeNormalize(m_HitNormal);
		}

		const Vec3 deltaXZ(ringCenter.x - m_Start.x, 0.0f, ringCenter.z - m_Start.z);
		const float distXZ = deltaXZ.length();
		const float arcHeight = m_Tuning.arcHeightBase + (distXZ * m_Tuning.arcHeightPerDistXZ);

		Vec3 v0;
		float T = 0.0f;
		if (!SolveBallistic_ApexHeight(m_Start, ringCenter, m_Tuning.gravity, arcHeight, v0, T))
		{
			SetMarkersVisible(false);
			return;
		}

		SetMarkersVisible(true);

		for (int i = 0; i < m_LineSegmentCount; ++i)
		{
			const float ratio0 = static_cast<float>(i) / static_cast<float>(m_LineSegmentCount);
			const float ratio1 = static_cast<float>(i + 1) / static_cast<float>(m_LineSegmentCount);
			const Vec3 p0 = SamplePos(m_Start, v0, m_Tuning.gravity, T * ratio0);
			const Vec3 p1 = SamplePos(m_Start, v0, m_Tuning.gravity, T * ratio1);
			const Vec3 segment = p1 - p0;
			const float length = segment.length();

			if (length <= 1e-5f)
			{
				m_PathMarkers[i]->SetDrawActive(false);
				continue;
			}

			m_PathMarkers[i]->SetDrawActive(true);
			const Vec3 center = (p0 + p1) * 0.5f;
			Quat rotation;
			rotation.facing(segment);
			SetMarkerTransform(m_PathMarkers[i], center, rotation, Vec3(m_LineWidth, 1.0f, length));
		}

		const Vec3 lift = ringNormal * m_SurfaceLift;
		const Quat areaRotation = bsmUtil::MakeFromToQuat(Vec3(0, 1, 0), ringNormal);
		SetMarkerTransform(m_AreaMarker, ringCenter + lift, areaRotation, m_Tuning.explosionRadius * m_AreaRadiusScale);
	}

	void BombAimPreview::SetMarkersVisible(bool v)
	{
		if (m_MarkersShown == v) return;
		m_MarkersShown = v;

		for (auto& d : m_PathMarkers) d->SetDrawActive(v);
		if (m_AreaMarker) m_AreaMarker->SetDrawActive(v);
	}

	Vec3 BombAimPreview::SafeNormalize(const Vec3& v)
	{
		const float len2 = (v.x * v.x + v.y * v.y + v.z * v.z);
		if (len2 < 1e-8f) return Vec3(0, 1, 0);
		return v / std::sqrt(len2);
	}

	bool BombAimPreview::SolveBallistic_ApexHeight(
		const Vec3& p0,
		const Vec3& p1,
		const Vec3& gravity,
		float arcHeight,
		Vec3& outV0,
		float& outT) const
	{
		const float g = -gravity.y;
		if (g <= 1e-6f) return false;

		const float apexY = bsmUtil::Max(p0.y, p1.y) + arcHeight;
		const float h0 = bsmUtil::Max(0.0f, apexY - p0.y);
		const float h1 = bsmUtil::Max(0.0f, apexY - p1.y);

		const float vY0 = std::sqrt(2.0f * g * h0);
		const float tUp = vY0 / g;
		const float tDown = std::sqrt(2.0f * h1 / g);
		const float totalT = bsmUtil::Max(0.001f, tUp + tDown);

		const Vec3 deltaXZ(p1.x - p0.x, 0.0f, p1.z - p0.z);
		const Vec3 vXZ = deltaXZ * (1.0f / totalT);

		outV0 = Vec3(vXZ.x, vY0, vXZ.z);
		outT = totalT;
		return true;
	}

	Vec3 BombAimPreview::SamplePos(const Vec3& p0, const Vec3& v0, const Vec3& g, float t)
	{
		return p0 + (v0 * t) + (g * (0.5f * t * t));
	}

}
