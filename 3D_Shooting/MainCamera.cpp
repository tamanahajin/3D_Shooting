
#include "stdafx.h"
#include "Project.h"

namespace shooting {

	//--------------------------------------------------------------------------------------
	//	MainCameraカメラ
	//--------------------------------------------------------------------------------------
	MainCamera::MainCamera(const std::shared_ptr<Stage>& stage) :
		PerspecCamera(),
		m_Stage(stage),
		m_ToTargetLerp(1.0f),
		m_TargetToAt(0, 0, 0),
		m_RadY(0.5f),
		m_RadXZ(0),
		m_CameraUpDownSpeed(0.5f),
		m_CameraUnderRot(0.1f),
		m_ArmLen(5.0f),
		m_MaxArm(20.0f),
		m_MinArm(2.0f),
		m_RotSpeed(1.0f),
		m_ZoomSpeed(0.1f),
		m_LRBaseMode(true),
		m_UDBaseMode(true)
	{
		m_ArmLenCurrent = m_ArmLen;
	}

	MainCamera::MainCamera(float ArmLen) :
		PerspecCamera(),
		m_ToTargetLerp(1.0f),
		m_TargetToAt(0, 0, 0),
		m_RadY(0.5f),
		m_RadXZ(0),
		m_CameraUpDownSpeed(0.5f),
		m_CameraUnderRot(0.1f),
		m_ArmLen(5.0f),
		m_MaxArm(20.0f),
		m_MinArm(2.0f),
		m_RotSpeed(1.0f),
		m_ZoomSpeed(0.1f),
		m_LRBaseMode(true),
		m_UDBaseMode(true)
	{
		m_ArmLen = ArmLen;
		auto eye = GetEye();
		eye.y = m_ArmLen;
		SetEye(eye);
	}

	MainCamera::~MainCamera() {}

	//アクセサ
	void MainCamera::SetEye(const Vec3& Eye)
	{
		PerspecCamera::SetEye(Eye);
		//UpdateArmLengh();
	}
	void MainCamera::SetEye(float x, float y, float z)
	{
		PerspecCamera::SetEye(x, y, z);
		//UpdateArmLengh();
	}


	std::shared_ptr<GameObject> MainCamera::GetTargetObject() const
	{
		if (!m_TargetObject.expired())
		{
			return m_TargetObject.lock();
		}
		return nullptr;
	}

	void MainCamera::SetTargetObject(const std::shared_ptr<GameObject>& Obj)
	{
		m_TargetObject = Obj;
	}

	float MainCamera::GetToTargetLerp() const
	{
		return m_ToTargetLerp;
	}
	void MainCamera::SetToTargetLerp(float f)
	{
		m_ToTargetLerp = f;
	}

	float MainCamera::GetArmLengh() const
	{
		return m_ArmLen;
	}

	void MainCamera::UpdateArmLengh()
	{
		auto vec = GetEye() - GetAt();
		m_ArmLen = bsmUtil::length(vec);
		if (m_ArmLen >= m_MaxArm)
		{
			//m_MaxArm以上離れないようにする
			m_ArmLen = m_MaxArm;
		}
		if (m_ArmLen <= m_MinArm)
		{
			//m_MinArm以下近づかないようにする
			m_ArmLen = m_MinArm;
		}
	}

	float MainCamera::GetMaxArm() const
	{
		return m_MaxArm;

	}
	void MainCamera::SetMaxArm(float f)
	{
		m_MaxArm = f;
	}
	float MainCamera::GetMinArm() const
	{
		return m_MinArm;
	}
	void MainCamera::SetMinArm(float f)
	{
		m_MinArm = f;
	}

	float MainCamera::GetRotSpeed() const
	{
		return m_RotSpeed;

	}
	void MainCamera::SetRotSpeed(float f)
	{
		m_RotSpeed = abs(f);
	}

	Vec3 MainCamera::GetTargetToAt() const
	{
		return m_TargetToAt;

	}
	void MainCamera::SetTargetToAt(const Vec3& v)
	{
		m_TargetToAt = v;
	}

	bool MainCamera::GetLRBaseMode() const
	{
		return m_LRBaseMode;

	}
	bool MainCamera::IsLRBaseMode() const
	{
		return m_LRBaseMode;

	}
	void MainCamera::SetLRBaseMode(bool b)
	{
		m_LRBaseMode = b;
	}
	bool MainCamera::GetUDBaseMode() const
	{
		return m_UDBaseMode;

	}
	bool MainCamera::IsUDBaseMode() const
	{
		return m_UDBaseMode;
	}
	void MainCamera::SetUDBaseMode(bool b)
	{
		m_UDBaseMode = b;

	}

	void MainCamera::SetSpawnIntroView(bool active, const Vec3& eye, const Vec3& at)
	{
		m_SpawnIntroViewActive = active;
		m_SpawnIntroEye = eye;
		m_SpawnIntroAt = at;

		if (active)
		{
			PerspecCamera::SetAt(m_SpawnIntroAt);
			PerspecCamera::SetEye(m_SpawnIntroEye);
		}
	}

	void MainCamera::FinishSpawnIntroViewAndResumeFollow()
	{
		const Vec3 eye = m_SpawnIntroEye;
		const Vec3 at = m_SpawnIntroAt;
		Vec3 arm = eye - at;
		const float armLen = bsmUtil::length(arm);

		if (armLen > 0.0001f)
		{
			arm.normalize();

			// 通常カメラは m_RadY / m_RadXZ / m_ArmLen から毎フレーム Eye を作り直す。
			// そのため、固定解除前に登場カメラの Eye-At ベクトルを通常カメラの内部値へ逆算しておく。
			m_ArmLen = bsmUtil::Clamp(armLen, m_MinArm, m_MaxArm);
			m_ArmLenCurrent = m_ArmLen;
			m_RadY = std::asin(bsmUtil::Clamp(arm.y, -1.0f, 1.0f));

			// 通常カメラの水平基準は「-Z方向を yaw 回転した向き」なので、
			// 現在の水平向きから yaw を逆算して、登場時の角度をそのまま引き継ぐ。
			m_RadXZ = std::atan2(-arm.x, -arm.z);
		}

		PerspecCamera::SetAt(at);
		PerspecCamera::SetEye(eye);
		m_SpawnIntroViewActive = false;

		// 演出中はマウス追従を止めていたため、復帰1フレーム目の大きなdeltaで
		// カメラが跳ねないようにカーソル状態を同期する。
		App::GetInputDevice().WarpCursorToClientPos(GetClientCenter(App::GetHwnd()));
	}


	void MainCamera::SetAt(const Vec3& At)
	{
		PerspecCamera::SetAt(At);
		Vec3 armVec = GetEye() - GetAt();
		armVec.normalize();
		armVec *= m_ArmLen;
		Vec3 newEye = GetAt() + armVec;
		PerspecCamera::SetEye(newEye);
	}
	void MainCamera::SetAt(float x, float y, float z)
	{
		PerspecCamera::SetAt(x, y, z);
		Vec3 armVec = GetEye() - GetAt();
		armVec.normalize();
		armVec *= m_ArmLen;
		Vec3 newEye = GetAt() + armVec;
		PerspecCamera::SetEye(newEye);

	}

	bool MainCamera::GetMouseDelta(HWND hwnd, POINT& prev, bool& hasPrev, float& dx, float& dy)
	{
		POINT p;
		if (!GetCursorPos(&p)) return false;
		ScreenToClient(hwnd, &p);

		if (!hasPrev)
		{
			prev = p;
			hasPrev = true;
			dx = dy = 0.0f;
			return true;
		}

		dx = float(p.x - prev.x);
		dy = float(p.y - prev.y);
		prev = p;
		return true;
	}

	POINT MainCamera::GetClientCenter(HWND hwnd)
	{
		RECT rc{};
		::GetClientRect(hwnd, &rc);
		return POINT{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
	}

	POINT MainCamera::GetClientCenterInScreen(HWND hwnd)
	{
		RECT rc{};
		::GetClientRect(hwnd, &rc);
		POINT center{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
		::ClientToScreen(hwnd, &center);
		return center;
	}

	void MainCamera::ClipCursorToClient(HWND hwnd, bool enable)
	{
		if (!enable)
		{
			::ClipCursor(nullptr);
			return;
		}

		RECT rc{};
		::GetClientRect(hwnd, &rc);
		POINT tl{ rc.left, rc.top };
		POINT br{ rc.right, rc.bottom };
		::ClientToScreen(hwnd, &tl);
		::ClientToScreen(hwnd, &br);

		RECT clip{ tl.x, tl.y, br.x, br.y };
		::ClipCursor(&clip);
	}

	void MainCamera::SetCursorVisible(bool visible, int& counter)
	{
		// ShowCursorは内部カウンタ式なので、目的の状態になるまで回す
		if (visible)
		{
			while (::ShowCursor(TRUE) < 0) {}
			counter = 0;
		}
		else
		{
			while (::ShowCursor(FALSE) >= 0) {}
			counter = 0;
		}
	}

	void MainCamera::BeginMouseLook()
	{
		if (m_CursorLocked) return;

		HWND hwnd = App::GetHwnd();

		// 念のためフォーカスを取る（好み）
		::SetForegroundWindow(hwnd);
		::SetFocus(hwnd);
		::SetCapture(hwnd);

		// 現在位置を保存（解除時に戻したい場合）
		::GetCursorPos(&m_SaveCursorPos);

		// カーソル非表示 + ウィンドウ内に制限
		SetCursorVisible(false, m_ShowCursorCount);
		ClipCursorToClient(hwnd, true);

		// 中央へ移動（初回delta暴れ防止）
		POINT c = GetClientCenterInScreen(hwnd);
		::SetCursorPos(c.x, c.y);

		m_CursorLocked = true;
	}

	void MainCamera::EndMouseLook()
	{
		if (!m_CursorLocked) return;

		HWND hwnd = App::GetHwnd();

		::ReleaseCapture();
		ClipCursorToClient(hwnd, false);
		SetCursorVisible(true, m_ShowCursorCount);

		// 保存した位置へ戻す（不要なら消してOK）
		::SetCursorPos(m_SaveCursorPos.x, m_SaveCursorPos.y);

		m_CursorLocked = false;
	}

	// pivot -> desiredEye の移動を「半径radiusの球」としてスイープし、当たったら手前に寄せたEyeを返す
	Vec3 MainCamera::ResolveCameraEyeBySweep(
		const std::shared_ptr<Stage>& stage,
		const Vec3& pivot,
		const Vec3& desiredEye,
		float radius,
		float skin,
		const std::shared_ptr<GameObject>& ignoreObj,
		bool onlyFixed
	)
	{
		if (!stage) return desiredEye;
		if (!bsmUtil::IsFiniteVec3(pivot) || !bsmUtil::IsFiniteVec3(desiredEye)) return desiredEye;
		if (!std::isfinite(radius) || radius <= 0.0f) return desiredEye;
		if (!std::isfinite(skin) || skin < 0.0f) skin = 0.0f;

		// このフレームで pivot→desiredEye まで移動する速度（units/sec）
		const float dt = bsmUtil::Max(1e-4f, (float)Scene::GetElapsedTime());
		const Vec3 move = desiredEye - pivot;
		const float moveLen = bsmUtil::length(move);
		if (moveLen < 1e-6f) return desiredEye;

		Vec3 dir = move;
		dir.normalize();

		const Vec3 camVel = move / dt;         // src velocity
		const SPHERE camBefore(pivot, radius); // 時刻0でのカメラ球

		// broad-phase（ざっくり候補を減らす）: 線分を包むAABB
		const Vec3 mn(
			std::min(pivot.x, desiredEye.x) - radius,
			std::min(pivot.y, desiredEye.y) - radius,
			std::min(pivot.z, desiredEye.z) - radius
		);
		const Vec3 mx(
			bsmUtil::Max(pivot.x, desiredEye.x) + radius,
			bsmUtil::Max(pivot.y, desiredEye.y) + radius,
			bsmUtil::Max(pivot.z, desiredEye.z) + radius
		);
		const AABB sweepAabb(mn, mx);

		bool hit = false;
		float bestHitTime = dt; // 最短ヒット
		// ここでは“最初に当たった時刻”だけ欲しい（当たり面の法線は不要）

		// ヒット候補が見つかったときに、そのヒット時刻で更新するラムダ
		auto ConsiderHitTime = [&](float t)
			{
				if (!std::isfinite(t)) return;
				t = bsmUtil::Clamp(t, 0.0f, dt);
				if (t < bestHitTime) { bestHitTime = t; hit = true; }
			};

		auto& objs = stage->GetGameObjectVec();
		for (auto& obj : objs)
		{
			if (!obj || !obj->IsUpdateActive()) continue;
			if (ignoreObj && obj == ignoreObj) continue;

			auto col = obj->GetComponent<Collision>(false);
			if (!col || !col->IsUpdateActive()) continue;
			// 環境（地面・壁）だけでよければ fixed のみに絞る
			if (onlyFixed && !col->IsFixed()) continue;

			// broad-phase
			if (!HitTest::AABB_AABB(sweepAabb, col->GetWrappedAABB())) continue;

			// 相手が動く場合の相対速度にも対応（fixedなら基本0のはず）
			Vec3 destVel(0, 0, 0);
			if (!col->IsFixed())
			{
				if (auto tr = obj->GetComponent<Transform>(false))
				{
					const Vec3 p1 = tr->GetWorldMatrix().transInMatrix();
					const Vec3 p0 = tr->GetBeforeWorldMatrix().transInMatrix();
					destVel = (p1 - p0) / dt;

					if (!bsmUtil::IsFiniteVec3(destVel))
					{
						destVel = Vec3(0, 0, 0);
					}
				}
			}
			const Vec3 spanVel = camVel - destVel;
			if (!bsmUtil::IsFiniteVec3(spanVel))
			{
				continue; // 壊れた相手は無視（クラッシュ回避）
			}

			float hitTime = 0.0f;

			// 相手の形状ごとに “球 vs ○○” の連続判定を呼ぶ
			if (auto csp = obj->GetComponent<CollisionSphere>(false))
			{
				SPHERE destBefore = csp->GetBeforeSphere();
				if (HitTest::SPHERE_SPHERE(camBefore, destBefore))
				{
					ConsiderHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereSphere(camBefore, spanVel, destBefore, 0, dt, hitTime))
					ConsiderHitTime(hitTime);
			}
			else if (auto ccap = obj->GetComponent<CollisionCapsule>(false))
			{
				CAPSULE dest0 = ccap->GetBeforeCapsule();
				Vec3 dummy;
				if (HitTest::SPHERE_CAPSULE(camBefore, dest0, dummy))
				{
					ConsiderHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereCapsule(camBefore, spanVel, dest0, 0, dt, hitTime))
					ConsiderHitTime(hitTime);
			}
			else if (auto cobb = obj->GetComponent<CollisionObb>(false))
			{
				OBB dest0 = cobb->GetBeforeObb();
				Vec3 dummy;
				if (HitTest::SPHERE_OBB(camBefore, dest0, dummy))
				{
					ConsiderHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereObb(camBefore, spanVel, dest0, 0, dt, hitTime))
					ConsiderHitTime(hitTime);
			}
			else if (auto crect = obj->GetComponent<CollisionRect>(false))
			{
				COLRECT dest0 = crect->GetBeforeColRect();
				Vec3 dummy;
				if (HitTest::SPHERE_COLRECT(camBefore, dest0, dummy))
				{
					ConsiderHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereRect(camBefore, spanVel, dest0, 0, dt, hitTime))
					ConsiderHitTime(hitTime);
			}
		}

		if (!hit) return desiredEye;

		// ヒット時刻の“安全な中心位置”（球が接触した瞬間の中心）
		Vec3 eyeAtHit = pivot + camVel * bestHitTime;

		// NaN保険
		if (!bsmUtil::IsFiniteVec3(eyeAtHit))
			return desiredEye;

		// 数値誤差でチラつかないように少しだけ手前へ
		float distFromPivot = bsmUtil::dot(eyeAtHit - pivot, dir);
		distFromPivot = bsmUtil::Max(0.0f, distFromPivot - skin);

		Vec3 safeEye = pivot + dir * distFromPivot;

		// 最終NaN保険
		if (!bsmUtil::IsFiniteVec3(safeEye))
			return desiredEye;

		return safeEye;
	}

	void MainCamera::OnCreate()
	{
		auto stage = m_Stage.lock();
		if (stage)
		{
			m_CollisionManager = stage->GetCollisionManager();
		}
	}

	void MainCamera::OnUpdate(double elapsedTime)
	{
		if (m_SpawnIntroViewActive)
		{
			// 登場カメラ固定が有効な間は、通常のマウス追従カメラを止めて正面カメラ位置を維持する。
			// 演出終了後も解除しなければ、この最終位置のままゲームを続けられる。
			PerspecCamera::SetAt(m_SpawnIntroAt);
			PerspecCamera::SetEye(m_SpawnIntroEye);
			PerspecCamera::OnUpdate(elapsedTime);
			return;
		}

		//前回のターンからの時間
		Vec3 newEye = GetEye();
		Vec3 newAt = GetAt();
		//計算に使うための腕角度（ベクトル）
		Vec3 armVec = newEye - newAt;
		//正規化しておく
		armVec.normalize();

		auto& input = App::GetInputDevice();
		HWND hwnd = App::GetHwnd();

		if (m_MouseLook)
		{
			BeginMouseLook();

			// 押した瞬間のフレームはdeltaを無視（ジャンプ防止）
			//POINT d = input.MousePressed(VK_RBUTTON) ? POINT{ 0,0 } : input.GetMouseDelta();
			// 中央固定方式：毎フレーム、中央からのズレをdeltaとして使う
			POINT d = input.GetMouseDelta();

			float dx = float(d.x);
			float dy = float(d.y);

			// yaw（左右）
			if (IsLRBaseMode())  m_RadXZ += (dx)*m_MouseSens * m_RotSpeed;
			else                m_RadXZ += (-dx) * m_MouseSens * m_RotSpeed;

			if (std::abs(m_RadXZ) >= XM_2PI) m_RadXZ = 0.0f;

			// pitch（上下）
			if (IsUDBaseMode())  m_RadY += (dy)*m_MouseSens * m_CameraUpDownSpeed;
			else                m_RadY += (-dy) * m_MouseSens * m_CameraUpDownSpeed;

			// 使用後に中央へ戻す（これで無限回転）
			//POINT c = GetClientCenterInScreen(hwnd);
			//::SetCursorPos(c.x, c.y);
			POINT centerClient = GetClientCenter(hwnd);
			App::GetInputDevice().WarpCursorToClientPos(centerClient);
		}
		else
		{
			EndMouseLook();
		}


		// pitch制限
		m_RadY = bsmUtil::Clamp(m_RadY, m_PitchMin, m_PitchMax);

		// ホイールでズーム（上で寄る）
		const int wheel = input.GetMouseWheelDelta();
		if (wheel != 0)
		{
			m_ArmLen -= (wheel / 120.0f) * m_ZoomSpeed;
			m_ArmLen = bsmUtil::Clamp(m_ArmLen, m_MinArm, m_MaxArm);
		}

		armVec.y = sin(m_RadY);

		Quat qtXZ;
		qtXZ.rotationAxis(Vec3(0, 1.0f, 0), m_RadXZ);
		qtXZ.normalize();

		Mat4x4 Mat;
		Mat.strTransformation(
			Vec3(1.0f, 1.0f, 1.0f),
			Vec3(0.0f, 0.0f, -1.0f),
			qtXZ
		);

		Vec3 posXZ = Mat.transInMatrix();

		const float s = std::sin(m_RadY);
		const float c = std::cos(m_RadY);

		armVec.x = posXZ.x * c;
		armVec.z = posXZ.z * c;
		armVec.y = s;
		armVec.normalize();

		auto ptrTarget = GetTargetObject();
		if (ptrTarget)
		{
			Vec3 toAt = ptrTarget->GetComponent<Transform>()->GetWorldMatrix().transInMatrix();
			toAt += m_TargetToAt;
			newAt = Lerp::CalculateLerp(GetAt(), toAt, 0, 1.0f, 1.0, Lerp::Linear);
		}

		//Vec3 toEye = newAt + armVec * m_ArmLen;
		//newEye = Lerp::CalculateLerp(GetEye(), toEye, 0, 1.0f, m_ToTargetLerp, Lerp::Linear);

		float desiredArm = bsmUtil::Clamp(m_ArmLen, m_MinArm, m_MaxArm);
		float targetArm = desiredArm;
		bool hitNow = false;

		auto cm = m_CollisionManager.lock();
		auto isCameraPathBlocked = [&](const Vec3& candidateEye, RaycastHit* outHit) -> bool
			{
				if (!cm)
				{
					return false;
				}

				Vec3 probeVec = candidateEye - newAt;
				const float probeLen = bsmUtil::length(probeVec);
				if (probeLen <= 1e-4f)
				{
					return false;
				}
				probeVec.normalize();

				// プレイヤーのすぐ近くから太いSphereCastを始めると、壁や木に密着しただけで
				// 「カメラが塞がれた」と判定されるため、少し離した位置から遮蔽を調べる。
				const float probeStartOffset = bsmUtil::Clamp(
					m_CameraColProbeStartOffset,
					0.0f,
					bsmUtil::Max(0.0f, probeLen - 0.01f));
				const float probeDistance = probeLen - probeStartOffset;
				if (probeDistance <= 1e-4f)
				{
					return false;
				}

				RaycastHit hit{};
				const bool blocked = cm->SphereCast(
					newAt + probeVec * probeStartOffset,
					probeVec,
					probeDistance,
					m_CameraColRadius,
					hit,
					ptrTarget,
					{ L"Bullet", L"Bomb", L"Enemy", L"EnemyProxy", L"Item", L"HpRecoveryItem", L"BombItem" });
				if (blocked && outHit)
				{
					*outHit = hit;
				}
				return blocked;
		};

		if (cm)
		{
			RaycastHit hit{};
			const Vec3 centerEye = newAt + armVec * desiredArm;
			if (isCameraPathBlocked(centerEye, &hit))
			{
				// カメラ経路が塞がれている場合は、壁の少し手前まで距離を縮める。
				const Vec3 safeEye = hit.m_Point + hit.m_Normal * (m_CameraColRadius + m_CameraColMargin);
				float safeArm = bsmUtil::dot(safeEye - newAt, armVec);
				targetArm = bsmUtil::Clamp(safeArm, m_MinArm, desiredArm);
				hitNow = true;
			}
		}

		// “縮む” と “戻る” で速度を変える:contentReference[oaicite:3]{index=3}
		//const float rate = (targetArm < m_ArmLenCurrent) ? m_PushInRate : m_ReturnRate;
		//m_ArmLenCurrent += (targetArm - m_ArmLenCurrent) * rate;
		//m_ArmLenCurrent = bsmUtil::Clamp(m_ArmLenCurrent, m_MinArm, m_MaxArm);

		if (targetArm < m_ArmLenCurrent)
		{
			// 壁に当たって縮む時は即座に
			m_ArmLenCurrent = targetArm;
		}
		else
		{
			// 壁が無い（戻る）時だけゆっくり
			m_ArmLenCurrent += (targetArm - m_ArmLenCurrent) * m_ReturnRate;
		}
		m_ArmLenCurrent = bsmUtil::Clamp(m_ArmLenCurrent, m_MinArm, m_MaxArm);

		Vec3 toEye = newAt + armVec * m_ArmLenCurrent;

		if (hitNow)  // SphereCastが当たったフラグ
		{
			newEye = toEye; // 直行ワープ（壁貫通防止を最優先）
		}
		else
		{
			newEye = Lerp::CalculateLerp(GetEye(), toEye, 0, 1.0f, m_ToTargetLerp, Lerp::Linear);
		}

		// 既存の Eye lerp は残してOK（好みで m_ToTargetLerp=1 にしても良い）
		//newEye = Lerp::CalculateLerp(GetEye(), toEye, 0, 1.0f, m_ToTargetLerp, Lerp::Linear);

		// SetAtがEyeを動かす実装だとガタつくので、ベースを直接呼ぶ
		PerspecCamera::SetAt(newAt);
		PerspecCamera::SetEye(newEye);
		PerspecCamera::OnUpdate(elapsedTime);
	}
}
