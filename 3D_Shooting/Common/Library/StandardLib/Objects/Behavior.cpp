#include "stdafx.h"

namespace shooting {

	Behavior::Behavior(const std::shared_ptr<GameObject>& gameObjectPtr) :
		m_gameObject(gameObjectPtr)
	{
	}
	Behavior::~Behavior() {}
	std::shared_ptr<GameObject> Behavior::GetGameObject() const
	{
		auto gameObjectPtr = m_gameObject.lock();
		if (!gameObjectPtr)
		{
			throw BaseException(
				L"GameObjectは有効ではありません",
				L"if (!gameObjectPtr)",
				L"Behavior::GetGameObject()"
			);
		}
		else
		{
			return gameObjectPtr;
		}
	}

	std::shared_ptr<Stage> Behavior::GetStage() const
	{
		return GetGameObject()->GetStage();
	}

	//--------------------------------------------------------------------------------------
	///	行動ユーティリティクラス
	//--------------------------------------------------------------------------------------
	//進行方向を向くようにする
	void UtilBehavior::RotToHead(float LerpFact)
	{
		if (LerpFact <= 0.0f)
		{
			//補間係数が0以下なら何もしない
			return;
		}
		//回転の更新
		//Velocityの値で、回転を変更する
		//これで進行方向を向くようになる
		auto PtrTransform = GetGameObject()->GetComponent<Transform>();
		Vec3 Velocity = PtrTransform->GetVelocity();
		if (Velocity.length() > 0.0f)
		{
			Vec3 Temp = Velocity;
			Temp.normalize();
			// 与えられた Y 座標と X 座標に基づいて、点の極座標における角度を求める
			float ToAngle = atan2(Temp.x, Temp.z);
			Quat Qt;
			Qt.rotationRollPitchYawFromVector(Vec3(0, ToAngle, 0));
			Qt.normalize();
			// 現在の回転を取得
			Quat NowQt = PtrTransform->GetQuaternion();
			// 現在と目標を補間
			// 補間が1.0未満なら滑らかに回転
			if (LerpFact >= 1.0f)
			{
				NowQt = Qt;
			}
			else
			{
				NowQt = XMQuaternionSlerp(NowQt, Qt, LerpFact);
			}
			PtrTransform->SetQuaternion(NowQt);
		}
	}

	void UtilBehavior::RotToHead(const Vec3& Velocity, float LerpFact)
	{
		if (LerpFact <= 0.0f)
		{
			//補間係数が0以下なら何もしない
			return;
		}
		auto PtrTransform = GetGameObject()->GetComponent<Transform>();
		//回転の更新
		if (Velocity.length() > 0.0f)
		{
			Vec3 Temp = Velocity;
			Temp.normalize();
			float ToAngle = atan2(Temp.x, Temp.z);
			Quat Qt;
			Qt.rotationRollPitchYawFromVector(Vec3(0, ToAngle, 0));
			Qt.normalize();
			//現在の回転を取得
			Quat NowQt = PtrTransform->GetQuaternion();
			//現在と目標を補間
			if (LerpFact >= 1.0f)
			{
				NowQt = Qt;
			}
			else
			{
				NowQt = XMQuaternionSlerp(NowQt, Qt, LerpFact);
			}
			PtrTransform->SetQuaternion(NowQt);
		}
	}
}