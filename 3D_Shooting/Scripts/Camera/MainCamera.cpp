
#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const float kMaximumCameraShakeIntensity = 0.55f;
	}

	//--------------------------------------------------------------------------------------
	//	MainCameraカメラ
	//--------------------------------------------------------------------------------------
	MainCamera::MainCamera(const std::shared_ptr<Stage>& stage) :
		PerspecCamera(),
		m_stage(stage),
		m_toTargetLerp(1.0f),
		m_targetToAt(0, 0, 0),
		m_radY(0.5f),
		m_radXZ(0),
		m_cameraUpDownSpeed(0.5f),
		m_cameraUnderRot(0.1f),
		m_armLen(5.0f),
		m_maxArm(20.0f),
		m_minArm(2.0f),
		m_rotSpeed(1.0f),
		m_zoomSpeed(0.1f),
		m_lrBaseMode(true),
		m_udBaseMode(true)
	{
		m_armLenCurrent = m_armLen;
	}

	MainCamera::MainCamera(float armLen) :
		PerspecCamera(),
		m_toTargetLerp(1.0f),
		m_targetToAt(0, 0, 0),
		m_radY(0.5f),
		m_radXZ(0),
		m_cameraUpDownSpeed(0.5f),
		m_cameraUnderRot(0.1f),
		m_armLen(5.0f),
		m_maxArm(20.0f),
		m_minArm(2.0f),
		m_rotSpeed(1.0f),
		m_zoomSpeed(0.1f),
		m_lrBaseMode(true),
		m_udBaseMode(true)
	{
		m_armLen = armLen;
		auto eye = GetEye();
		eye.y = m_armLen;
		SetEye(eye);
	}

	MainCamera::~MainCamera() {}

	//アクセサ
	void MainCamera::SetEye(const Vec3& eye)
	{
		PerspecCamera::SetEye(eye);
		//UpdateArmLengh();
	}
	void MainCamera::SetEye(float x, float y, float z)
	{
		PerspecCamera::SetEye(x, y, z);
		//UpdateArmLengh();
	}


	std::shared_ptr<GameObject> MainCamera::GetTargetObject() const
	{
		if (!m_targetObject.expired())
		{
			return m_targetObject.lock();
		}
		return nullptr;
	}

	void MainCamera::SetTargetObject(const std::shared_ptr<GameObject>& obj)
	{
		m_targetObject = obj;
	}

	float MainCamera::GetToTargetLerp() const
	{
		return m_toTargetLerp;
	}
	void MainCamera::SetToTargetLerp(float f)
	{
		m_toTargetLerp = f;
	}

	float MainCamera::GetArmLengh() const
	{
		return m_armLen;
	}

	void MainCamera::UpdateArmLengh()
	{
		auto vec = GetEye() - GetAt();
		m_armLen = bsmUtil::length(vec);
		if (m_armLen >= m_maxArm)
		{
			//m_MaxArm以上離れないようにする
			m_armLen = m_maxArm;
		}
		if (m_armLen <= m_minArm)
		{
			//m_MinArm以下近づかないようにする
			m_armLen = m_minArm;
		}
	}

	float MainCamera::GetMaxArm() const
	{
		return m_maxArm;

	}
	void MainCamera::SetMaxArm(float f)
	{
		m_maxArm = f;
	}
	float MainCamera::GetMinArm() const
	{
		return m_minArm;
	}
	void MainCamera::SetMinArm(float f)
	{
		m_minArm = f;
	}

	float MainCamera::GetRotSpeed() const
	{
		return m_rotSpeed;

	}
	void MainCamera::SetRotSpeed(float f)
	{
		m_rotSpeed = abs(f);
	}

	Vec3 MainCamera::GetTargetToAt() const
	{
		return m_targetToAt;

	}
	void MainCamera::SetTargetToAt(const Vec3& v)
	{
		m_targetToAt = v;
	}

	bool MainCamera::GetLRBaseMode() const
	{
		return m_lrBaseMode;

	}
	bool MainCamera::IsLRBaseMode() const
	{
		return m_lrBaseMode;

	}
	void MainCamera::SetLRBaseMode(bool b)
	{
		m_lrBaseMode = b;
	}
	bool MainCamera::GetUDBaseMode() const
	{
		return m_udBaseMode;

	}
	bool MainCamera::IsUDBaseMode() const
	{
		return m_udBaseMode;
	}
	void MainCamera::SetUDBaseMode(bool b)
	{
		m_udBaseMode = b;

	}

	void MainCamera::SetSpawnIntroView(bool active, const Vec3& eye, const Vec3& at)
	{
		m_spawnIntroViewActive = active;
		m_spawnIntroEye = eye;
		m_spawnIntroAt = at;

		if (active)
		{
			PerspecCamera::SetAt(m_spawnIntroAt);
			PerspecCamera::SetEye(m_spawnIntroEye);
		}
	}

	void MainCamera::FinishSpawnIntroViewAndResumeFollow()
	{
		const Vec3 eye = m_spawnIntroEye;
		const Vec3 at = m_spawnIntroAt;
		Vec3 arm = eye - at;
		const float armLen = bsmUtil::length(arm);

		if (armLen > 0.0001f)
		{
			arm.normalize();

			// 通常カメラは m_radY / m_radXZ / m_armLen から毎フレーム Eye を作り直す。
			// そのため、固定解除前に登場カメラの Eye-At ベクトルを通常カメラの内部値へ逆算しておく。
			m_armLen = bsmUtil::Clamp(armLen, m_minArm, m_maxArm);
			m_armLenCurrent = m_armLen;
			m_radY = std::asin(bsmUtil::Clamp(arm.y, -1.0f, 1.0f));

			// 通常カメラの水平基準は「-Z方向を yaw 回転した向き」なので、
			// 現在の水平向きから yaw を逆算して、登場時の角度をそのまま引き継ぐ。
			m_radXZ = std::atan2(-arm.x, -arm.z);
		}

		PerspecCamera::SetAt(at);
		PerspecCamera::SetEye(eye);
		m_spawnIntroViewActive = false;

		// 演出中はマウス追従を止めていたため、復帰1フレーム目の大きなdeltaで
		// カメラが跳ねないようにカーソル状態を同期する。
		App::GetInputDevice().WarpCursorToClientPos(GetClientCenter(App::GetHwnd()));
	}

	void MainCamera::RequestCameraShake(
		const Vec3& worldPosition,
		float intensity,
		float duration,
		float maxDistance)
	{
		if (!bsmUtil::IsFiniteVec3(worldPosition) ||
			!std::isfinite(intensity) ||
			!std::isfinite(duration) ||
			!std::isfinite(maxDistance) ||
			intensity <= 0.0f ||
			duration <= 0.0f ||
			maxDistance <= 0.0f)
		{
			return;
		}

		// 前フレームで加えたシェイクを除いた注視点を使い、距離計算が揺れ自体に影響されないようにする。
		const Vec3 baseAt = GetAt() - m_lastShakeOffset;
		const float distance = bsmUtil::length(baseAt - worldPosition);
		if (!std::isfinite(distance) || distance >= maxDistance)
		{
			return;
		}

		const float distanceRate =
			1.0f - bsmUtil::Clamp(distance / maxDistance, 0.0f, 1.0f);
		const float appliedIntensity = intensity * distanceRate;
		m_shakeIntensity = bsmUtil::Min(
			m_shakeIntensity + appliedIntensity,
			kMaximumCameraShakeIntensity);
		m_shakeDuration = bsmUtil::Max(m_shakeDuration, duration);
		m_shakeTimeRemaining = bsmUtil::Max(m_shakeTimeRemaining, duration);
	}

	Vec3 MainCamera::UpdateCameraShake(float elapsedTime)
	{
		if (m_shakeTimeRemaining <= 0.0f ||
			m_shakeDuration <= 0.0f ||
			m_shakeIntensity <= 0.0f)
		{
			m_shakeIntensity = 0.0f;
			m_shakeDuration = 0.0f;
			m_shakeTimeRemaining = 0.0f;
			m_shakeElapsedTime = 0.0f;
			return Vec3(0.0f, 0.0f, 0.0f);
		}

		const float safeElapsedTime = bsmUtil::Max(elapsedTime, 0.0f);
		m_shakeElapsedTime += safeElapsedTime;
		m_shakeTimeRemaining = bsmUtil::Max(
			0.0f,
			m_shakeTimeRemaining - safeElapsedTime);

		// 残り時間の二乗で減衰させ、爆発直後は強く、終了間際は滑らかに静止させる。
		const float remainingRate = bsmUtil::Clamp(
			m_shakeTimeRemaining / m_shakeDuration,
			0.0f,
			1.0f);
		const float amplitude =
			m_shakeIntensity * remainingRate * remainingRate;
		const float phase = m_shakeElapsedTime;

		// 軸ごとに異なる周波数を使い、単純な往復運動に見えない揺れを作る。
		const Vec3 offset(
			std::sin(phase * 67.0f) * amplitude,
			std::sin((phase * 83.0f) + 1.7f) * amplitude * 0.65f,
			std::sin((phase * 53.0f) + 3.1f) * amplitude * 0.45f);

		if (m_shakeTimeRemaining <= 0.0f)
		{
			m_shakeIntensity = 0.0f;
			m_shakeDuration = 0.0f;
			m_shakeElapsedTime = 0.0f;
		}
		return offset;
	}


	void MainCamera::SetAt(const Vec3& at)
	{
		PerspecCamera::SetAt(at);
		Vec3 armVec = GetEye() - GetAt();
		armVec.normalize();
		armVec *= m_armLen;
		Vec3 newEye = GetAt() + armVec;
		PerspecCamera::SetEye(newEye);
	}
	void MainCamera::SetAt(float x, float y, float z)
	{
		PerspecCamera::SetAt(x, y, z);
		Vec3 armVec = GetEye() - GetAt();
		armVec.normalize();
		armVec *= m_armLen;
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
		if (m_cursorLocked) return;

		HWND hwnd = App::GetHwnd();

		// 念のためフォーカスを取る（好み）
		::SetForegroundWindow(hwnd);
		::SetFocus(hwnd);
		::SetCapture(hwnd);

		// 現在位置を保存（解除時に戻したい場合）
		::GetCursorPos(&m_saveCursorPos);

		// カーソル非表示 + ウィンドウ内に制限
		SetCursorVisible(false, m_showCursorCount);
		ClipCursorToClient(hwnd, true);

		// 中央へ移動（初回delta暴れ防止）
		POINT c = GetClientCenterInScreen(hwnd);
		::SetCursorPos(c.x, c.y);

		m_cursorLocked = true;
	}

	void MainCamera::EndMouseLook()
	{
		if (!m_cursorLocked) return;

		HWND hwnd = App::GetHwnd();

		::ReleaseCapture();
		ClipCursorToClient(hwnd, false);
		SetCursorVisible(true, m_showCursorCount);

		// 保存した位置へ戻す（不要なら消してOK）
		::SetCursorPos(m_saveCursorPos.x, m_saveCursorPos.y);

		m_cursorLocked = false;
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
		auto considerHitTime = [&](float t)
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
					considerHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereSphere(camBefore, spanVel, destBefore, 0, dt, hitTime))
					considerHitTime(hitTime);
			}
			else if (auto ccap = obj->GetComponent<CollisionCapsule>(false))
			{
				CAPSULE dest0 = ccap->GetBeforeCapsule();
				Vec3 dummy;
				if (HitTest::SPHERE_CAPSULE(camBefore, dest0, dummy))
				{
					considerHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereCapsule(camBefore, spanVel, dest0, 0, dt, hitTime))
					considerHitTime(hitTime);
			}
			else if (auto cobb = obj->GetComponent<CollisionObb>(false))
			{
				OBB dest0 = cobb->GetBeforeObb();
				Vec3 dummy;
				if (HitTest::SPHERE_OBB(camBefore, dest0, dummy))
				{
					considerHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereObb(camBefore, spanVel, dest0, 0, dt, hitTime))
					considerHitTime(hitTime);
			}
			else if (auto crect = obj->GetComponent<CollisionRect>(false))
			{
				COLRECT dest0 = crect->GetBeforeColRect();
				Vec3 dummy;
				if (HitTest::SPHERE_COLRECT(camBefore, dest0, dummy))
				{
					considerHitTime(0.0f);
					continue;
				}
				if (HitTest::CollisionTestSphereRect(camBefore, spanVel, dest0, 0, dt, hitTime))
					considerHitTime(hitTime);
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
		auto stage = m_stage.lock();
		if (stage)
		{
			m_collisionManager = stage->GetCollisionManager();
		}
	}

	void MainCamera::OnUpdate(double elapsedTime)
	{
		if (m_spawnIntroViewActive)
		{
			// 登場カメラ固定が有効な間は、通常のマウス追従カメラを止めて正面カメラ位置を維持する。
			// 演出終了後も解除しなければ、この最終位置のままゲームを続けられる。
			PerspecCamera::SetAt(m_spawnIntroAt);
			PerspecCamera::SetEye(m_spawnIntroEye);
			m_lastShakeOffset = Vec3(0.0f, 0.0f, 0.0f);
			PerspecCamera::OnUpdate(elapsedTime);
			return;
		}

		//前回のターンからの時間
		// 前フレームのシェイクを除去してから通常追従を計算し、揺れの位置が累積しないようにする。
		Vec3 newEye = GetEye() - m_lastShakeOffset;
		Vec3 newAt = GetAt() - m_lastShakeOffset;
		m_lastShakeOffset = Vec3(0.0f, 0.0f, 0.0f);
		//計算に使うための腕角度（ベクトル）
		Vec3 armVec = newEye - newAt;
		//正規化しておく
		armVec.normalize();

		auto& input = App::GetInputDevice();
		HWND hwnd = App::GetHwnd();

		if (m_mouseLook)
		{
			BeginMouseLook();

			// 押した瞬間のフレームはdeltaを無視（ジャンプ防止）
			//POINT d = input.MousePressed(VK_RBUTTON) ? POINT{ 0,0 } : input.GetMouseDelta();
			// 中央固定方式：毎フレーム、中央からのズレをdeltaとして使う
			POINT d = input.GetMouseDelta();

			float dx = float(d.x);
			float dy = float(d.y);

			// yaw（左右）
			if (IsLRBaseMode())  m_radXZ += (dx)*m_mouseSens * m_rotSpeed;
			else                m_radXZ += (-dx) * m_mouseSens * m_rotSpeed;

			if (std::abs(m_radXZ) >= XM_2PI) m_radXZ = 0.0f;

			// pitch（上下）
			if (IsUDBaseMode())  m_radY += (dy)*m_mouseSens * m_cameraUpDownSpeed;
			else                m_radY += (-dy) * m_mouseSens * m_cameraUpDownSpeed;

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
		m_radY = bsmUtil::Clamp(m_radY, m_pitchMin, m_pitchMax);

		// ホイールでズーム（上で寄る）
		const int wheel = input.GetMouseWheelDelta();
		if (wheel != 0)
		{
			m_armLen -= (wheel / 120.0f) * m_zoomSpeed;
			m_armLen = bsmUtil::Clamp(m_armLen, m_minArm, m_maxArm);
		}

		armVec.y = sin(m_radY);

		Quat qtXZ;
		qtXZ.rotationAxis(Vec3(0, 1.0f, 0), m_radXZ);
		qtXZ.normalize();

		Mat4x4 mat;
		mat.strTransformation(
			Vec3(1.0f, 1.0f, 1.0f),
			Vec3(0.0f, 0.0f, -1.0f),
			qtXZ
		);

		Vec3 posXZ = mat.transInMatrix();

		const float s = std::sin(m_radY);
		const float c = std::cos(m_radY);

		armVec.x = posXZ.x * c;
		armVec.z = posXZ.z * c;
		armVec.y = s;
		armVec.normalize();

		auto ptrTarget = GetTargetObject();
		if (ptrTarget)
		{
			Vec3 toAt = ptrTarget->GetComponent<Transform>()->GetWorldMatrix().transInMatrix();
			toAt += m_targetToAt;
			newAt = Lerp::CalculateLerp(newAt, toAt, 0, 1.0f, 1.0, Lerp::Linear);
		}

		//Vec3 toEye = newAt + armVec * m_armLen;
		//newEye = Lerp::CalculateLerp(GetEye(), toEye, 0, 1.0f, m_toTargetLerp, Lerp::Linear);

		float desiredArm = bsmUtil::Clamp(m_armLen, m_minArm, m_maxArm);
		float targetArm = desiredArm;
		bool hitNow = false;

		auto cm = m_collisionManager.lock();
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
					m_cameraColProbeStartOffset,
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
					m_cameraColRadius,
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
				const Vec3 safeEye = hit.m_Point + hit.m_Normal * (m_cameraColRadius + m_cameraColMargin);
				float safeArm = bsmUtil::dot(safeEye - newAt, armVec);
				targetArm = bsmUtil::Clamp(safeArm, m_minArm, desiredArm);
				hitNow = true;
			}
		}

		// “縮む” と “戻る” で速度を変える:contentReference[oaicite:3]{index=3}
		//const float rate = (targetArm < m_armLenCurrent) ? m_pushInRate : m_returnRate;
		//m_armLenCurrent += (targetArm - m_armLenCurrent) * rate;
		//m_armLenCurrent = bsmUtil::Clamp(m_armLenCurrent, m_minArm, m_maxArm);

		if (targetArm < m_armLenCurrent)
		{
			// 壁に当たって縮む時は即座に
			m_armLenCurrent = targetArm;
		}
		else
		{
			// 壁が無い（戻る）時だけゆっくり
			m_armLenCurrent += (targetArm - m_armLenCurrent) * m_returnRate;
		}
		m_armLenCurrent = bsmUtil::Clamp(m_armLenCurrent, m_minArm, m_maxArm);

		Vec3 toEye = newAt + armVec * m_armLenCurrent;

		if (hitNow)  // SphereCastが当たったフラグ
		{
			newEye = toEye; // 直行ワープ（壁貫通防止を最優先）
		}
		else
		{
			newEye = Lerp::CalculateLerp(newEye, toEye, 0, 1.0f, m_toTargetLerp, Lerp::Linear);
		}

		// 既存の Eye lerp は残してOK（好みで m_toTargetLerp=1 にしても良い）
		//newEye = Lerp::CalculateLerp(GetEye(), toEye, 0, 1.0f, m_toTargetLerp, Lerp::Linear);

		// EyeとAtへ同じ量を加えることで照準方向を維持し、画面全体だけを揺らす。
		m_lastShakeOffset = UpdateCameraShake(static_cast<float>(elapsedTime));

		// SetAtがEyeを動かす実装だとガタつくので、ベースを直接呼ぶ。
		PerspecCamera::SetAt(newAt + m_lastShakeOffset);
		PerspecCamera::SetEye(newEye + m_lastShakeOffset);
		PerspecCamera::OnUpdate(elapsedTime);
	}
}
