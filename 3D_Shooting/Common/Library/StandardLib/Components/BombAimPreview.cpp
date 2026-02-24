#include "stdafx.h"
#include "Project.h"
#include "BombAimPreview.h"

namespace shooting {

	void BombAimPreview::OnCreate()
	{
		// 表示用ドットを事前生成（毎フレーム生成しない）
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
		m_Target = target;
		m_HitNormal = hitNormal;
		m_HasHit = hasHit;
	}

	void BombAimPreview::OnUpdate(double /*elapsedTime*/)
	{
		if (!m_Visible)
		{
			SetDotsVisible(false);
			return;
		}

		auto stage = GetGameObject()->GetStage();
		auto colMgr = stage ? stage->GetCollisionManager() : nullptr;

		// まず「狙い点から得た法線」。ただし後で SphereCast のヒットで上書きするかも
		Vec3 ringNormal = (m_HasHit) ? SafeNormalize(m_HitNormal) : Vec3(0, 1, 0);
		Vec3 ringCenter = m_Target;

		// ------------------------------------------------------------
		// 実弾(BombBullet)と同じ：頂点高さに距離ボーナスを足す
		//   BombBullet::ResetForSpawn():
		//     apexY = max(p0.y,p1.y) + m_ArcHeight + distXZ*0.1f;
		// ------------------------------------------------------------
		const Vec3 deltaXZ(m_Target.x - m_Start.x, 0.0f, m_Target.z - m_Start.z);
		const float distXZ = deltaXZ.length();
		const float arcHeight = m_Tuning.arcHeightBase + distXZ * m_Tuning.arcHeightPerDistXZ;

		// 弾道の初速と理論飛翔時間
		Vec3 v0;
		float T = 0.0f;
		if (!SolveBallistic_ApexHeight(m_Start, m_Target, m_Tuning.gravity, arcHeight, v0, T))
		{
			SetDotsVisible(false);
			return;
		}

		SetDotsVisible(true);

		// ------------------------------------------------------------
		// プレビュー点の配置：実弾と同じ積分順でシミュレーション
		//   BombBullet::OnUpdate:
		//     v += g*dt;
		//     p += v*dt;
		//
		// “未来のフレームdt”は正確には分からないので、
		// ここでは 60fps 想定の固定dtで回す（実機が60fpsなら一致しやすい）
		// ------------------------------------------------------------
		const float fixedDt = 1.0f / 60.0f;

		// ボムの飛行中コリジョン半径（あなたの BulletManager は scale=0.2 が既定）
		// CollisionSphere は直径1.0が既定なので半径=0.5*scale = 0.1
		const float travelRadius = 0.1f;

		Vec3 p = m_Start;
		Vec3 v = v0;
		float curT = 0.0f;

		bool earlyHit = false;

		for (int i = 0; i < m_PathCount; ++i)
		{
			const float nextT = (m_PathCount <= 1) ? 0.0f : (T * (float)i / (float)(m_PathCount - 1));

			// curT -> nextT まで、fixedDt刻みで進める
			while (!earlyHit && (curT + 1e-6f < nextT))
			{
				const float dt = bsmUtil::Min(fixedDt, nextT - curT);

				const Vec3 prevP = p;

				// 実弾と同じ更新順
				v += m_Tuning.gravity * dt;
				p += v * dt;

				// 途中で壁などに当たるなら、そこで止める（実弾は衝突で爆発開始するので）
				if (colMgr)
				{
					Vec3 seg = p - prevP;
					const float segLen = seg.length();
					if (segLen > 1e-6f)
					{
						seg /= segLen;

						RaycastHit hit;
						if (colMgr->SphereCast(prevP, seg, segLen, travelRadius, hit, GetGameObject(), { L"Bullet", L"Enemy" }))
						{
							earlyHit = true;
							p = hit.m_Point;
							ringCenter = hit.m_Point;
							ringNormal = SafeNormalize(hit.m_Normal);
							v = Vec3(0, 0, 0); // 以降は停止扱い
						}
					}
				}

				curT += dt;
			}

			// dot を置く（earlyHit後は p が固定なので、その地点に並ぶ）
			if (auto tr = m_PathDots[i]->GetComponent<Transform>())
			{
				tr->SetPosition(p);
				tr->SetScale(Vec3(m_PathDotScale, m_PathDotScale, m_PathDotScale));
			}
		}

		// ------------------------------------------------------------
		// 爆発範囲リング（ヒットした地点 or 狙い地点）
		// ------------------------------------------------------------
		Vec3 t, b;
		MakeTangentBasis(ringNormal, t, b);

		const Vec3 lift = ringNormal * 0.03f; // Z-fighting回避

		for (int i = 0; i < m_RingCount; ++i)
		{
			const float ang = (2.0f * 3.1415926535f) * (float)i / (float)m_RingCount;
			const float c = std::cos(ang);
			const float s = std::sin(ang);

			Vec3 rp = ringCenter + lift + (t * (c * m_Tuning.explosionRadius)) + (b * (s * m_Tuning.explosionRadius));

			if (auto tr = m_RingDots[i]->GetComponent<Transform>())
			{
				tr->SetPosition(rp);
				tr->SetScale(Vec3(m_RingDotScale, m_RingDotScale, m_RingDotScale));
			}
		}
	}

	void BombAimPreview::SetDotsVisible(bool v)
	{
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
		// n と平行になりにくい基準ベクトルを選ぶ
		Vec3 ref = (std::fabs(n.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);

		outT = SafeNormalize(bsmUtil::cross(ref, n));
		outB = SafeNormalize(bsmUtil::cross(n, outT));
	}

	bool BombAimPreview::SolveBallistic_ApexHeight(
		const Vec3& p0, const Vec3& p1,
		const Vec3& gravity, float arcHeight,
		Vec3& outV0, float& outT) const
	{
		// gravity は (0, -g, 0) を想定（Y+が上）
		const float g = -gravity.y;
		if (g <= 1e-6f) return false;

		const float apexY = bsmUtil::Max(p0.y, p1.y) + arcHeight;

		const float h0 = bsmUtil::Max(0.0f, apexY - p0.y);
		const float h1 = bsmUtil::Max(0.0f, apexY - p1.y);

		const float vY0 = std::sqrt(2.0f * g * h0);
		const float tUp = vY0 / g;
		const float tDown = std::sqrt(2.0f * h1 / g);
		const float T = bsmUtil::Max(0.001f, tUp + tDown);

		const Vec3 deltaXZ(p1.x - p0.x, 0.0f, p1.z - p0.z);
		const Vec3 vXZ = deltaXZ * (1.0f / T);

		outV0 = Vec3(vXZ.x, vY0, vXZ.z);
		outT = T;
		return true;
	}

	Vec3 BombAimPreview::SamplePos(const Vec3& p0, const Vec3& v0, const Vec3& g, float t)
	{
		// p(t) = p0 + v0*t + 0.5*g*t^2
		return p0 + v0 * t + g * (0.5f * t * t);
	}

}
