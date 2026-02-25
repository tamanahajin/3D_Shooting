#include "stdafx.h"
#include "Project.h"
#include "BombAimPreview.h"

namespace shooting {

	namespace
	{
		float LengthSq(const Vec3& v)
		{
			return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
		}
	}

	void BombAimPreview::OnCreate()
	{
		auto stage = GetGameObject()->GetStage();
		if (!stage) return;

		TransParam tp;
		tp.position = Vec3(0, -100.0f, 0);
		tp.scale = Vec3(m_PathDotScale, m_PathDotScale, m_PathDotScale);
		tp.quaternion = Quat();

		m_PathDots.reserve(m_PathCount);
		for (int i = 0; i < m_PathCount; ++i)
		{
			auto dot = stage->AddGameObject<PreviewDot>(tp);
			dot->SetDrawActive(false);
			m_PathDots.push_back(dot);
		}

		tp.scale = Vec3(m_RingDotScale, m_RingDotScale, m_RingDotScale);

		m_RingDots.reserve(m_RingCount);
		for (int i = 0; i < m_RingCount; ++i)
		{
			auto dot = stage->AddGameObject<PreviewDot>(tp);
			dot->SetDrawActive(false);
			m_RingDots.push_back(dot);
		}
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
			SetDotsVisible(false);
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
			SetDotsVisible(true);
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
			SetDotsVisible(false);
			return;
		}

		SetDotsVisible(true);

		for (int i = 0; i < m_PathCount; ++i)
		{
			const float t = (m_PathCount <= 1) ? 0.0f : (T * static_cast<float>(i) / static_cast<float>(m_PathCount - 1));
			const Vec3 p = SamplePos(m_Start, v0, m_Tuning.gravity, t);

			if (auto tr = m_PathDots[i]->GetComponent<Transform>())
			{
				tr->SetPosition(p);
			}
		}

		Vec3 tangent;
		Vec3 bitangent;
		MakeTangentBasis(ringNormal, tangent, bitangent);

		const Vec3 lift = ringNormal * 0.03f;

		for (int i = 0; i < m_RingCount; ++i)
		{
			const float ang = (2.0f * 3.1415926535f) * static_cast<float>(i) / static_cast<float>(m_RingCount);
			const float c = std::cos(ang);
			const float s = std::sin(ang);

			const Vec3 rp = ringCenter + lift
				+ (tangent * (c * m_Tuning.explosionRadius))
				+ (bitangent * (s * m_Tuning.explosionRadius));

			if (auto tr = m_RingDots[i]->GetComponent<Transform>())
			{
				tr->SetPosition(rp);
			}
		}
	}

	void BombAimPreview::SetDotsVisible(bool v)
	{
		if (m_DotsShown == v) return;
		m_DotsShown = v;

		for (auto& d : m_PathDots) d->SetDrawActive(v);
		for (auto& d : m_RingDots) d->SetDrawActive(v);
	}

	Vec3 BombAimPreview::SafeNormalize(const Vec3& v)
	{
		const float len2 = (v.x * v.x + v.y * v.y + v.z * v.z);
		if (len2 < 1e-8f) return Vec3(0, 1, 0);
		return v / std::sqrt(len2);
	}

	void BombAimPreview::MakeTangentBasis(const Vec3& n, Vec3& outT, Vec3& outB)
	{
		const Vec3 ref = (std::fabs(n.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
		outT = SafeNormalize(bsmUtil::cross(ref, n));
		outB = SafeNormalize(bsmUtil::cross(n, outT));
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
