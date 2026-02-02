#include "stdafx.h"

namespace shooting {

	Transform::Transform(const std::shared_ptr<GameObject>& gameObjectPtr, const TransParam& param) :
		Component(gameObjectPtr),
		m_beforeParam(param),
		m_param(param),
		m_worldMatrix()
	{
	}

	//Get&Set	Before
	Vec3 Transform::GetBeforeScale() const
	{
		return m_beforeParam.scale;
	}

	Vec3 Transform::GetBeforePivot() const
	{
		return m_beforeParam.rotateOrigin;
	}

	Quat Transform::GetBeforeQuaternion() const
	{
		return m_beforeParam.quaternion;
	}

	Vec3 Transform::GetBeforeRotation() const
	{
		return m_beforeParam.quaternion.toRotVec();
	}

	Vec3 Transform::GetBeforePosition() const
	{
		return m_beforeParam.position;
	}

	Vec3 Transform::GetBeforeWorldPosition() const
	{
		return GetBeforeWorldMatrix().transInMatrix();
	}

	bool Transform::IsSameBeforeWorldMatrix(const Mat4x4& mat) const
	{
		return mat.equalInt(GetBeforeWorldMatrix());
	}


	const Mat4x4 Transform::GetBeforeWorldMatrix() const
	{
		auto parPtr = GetParent();
		Mat4x4 BefWorld;
		BefWorld.affineTransformation(
			m_beforeParam.scale,
			m_beforeParam.rotateOrigin,
			m_beforeParam.quaternion,
			m_beforeParam.position
		);
		if (parPtr)
		{
			auto ParBeforeWorld = parPtr->GetComponent<Transform>()->GetBeforeWorldMatrix();
			ParBeforeWorld.scaleIdentity();
			BefWorld *= ParBeforeWorld;
		}
		return BefWorld;
	}

	//Get&Set
	Vec3 Transform::GetScale() const
	{
		return m_param.scale;
	}

	void Transform::SetScale(const Vec3& scale)
	{
		if (!scale.isNaN() && !scale.isInfinite())
		{
			m_param.scale = scale;
		}
	}
	void Transform::SetScale(float x, float y, float z)
	{
		SetScale(Vec3(x, y, z));
	}

	Vec3 Transform::GetRotateOrigin() const
	{
		return m_param.rotateOrigin;
	}
	void Transform::SetRotateOrigin(const Vec3& rotateOrigin)
	{
		if (!rotateOrigin.isNaN() && !rotateOrigin.isInfinite())
		{
			m_param.rotateOrigin = rotateOrigin;
		}
	}
	void Transform::SetRotateOrigin(float x, float y, float z)
	{
		SetRotateOrigin(Vec3(x, y, z));
	}

	Quat Transform::GetQuaternion() const
	{
		return m_param.quaternion;
	}
	void Transform::SetQuaternion(const Quat& quaternion)
	{
		if (!quaternion.isNaN() && !quaternion.isInfinite())
		{
			m_param.quaternion = quaternion;
			m_param.quaternion.normalize();
		}
	}
	Vec3 Transform::GetRotation() const
	{
		Vec3 r = m_param.quaternion.toRotVec();
		r.normalize();
		return r;
	}

	void Transform::SetRotation(const Vec3& rot)
	{
		if (!rot.isNaN() && !rot.isInfinite())
		{
			Quat qt;
			qt.rotationRollPitchYawFromVector(rot);
			SetQuaternion(qt);
		}
	}
	void Transform::SetRotation(float x, float y, float z)
	{
		SetRotation(Vec3(x, y, z));
	}

	void Transform::AddPosition(const Vec3& addpos)
	{
		if (!addpos.isNaN() && !addpos.isInfinite())
		{
			m_param.position += addpos;
		}
	}


	Vec3 Transform::GetPosition() const
	{
		return m_param.position;
	}

	void Transform::SetPosition(const Vec3& position)
	{
		if (!position.isNaN() && !position.isInfinite())
		{
			m_param.position = position;
		}
	}
	void Transform::SetPosition(float x, float y, float z)
	{
		SetPosition(Vec3(x, y, z));
	}

	void Transform::ResetPosition(const Vec3& position)
	{
		if (!position.isNaN() && !position.isInfinite())
		{
			m_beforeParam.position = position;
			m_param.position = position;
		}
	}

	Vec3 Transform::GetWorldPosition()
	{
		return GetWorldMatrix().transInMatrix();
	}
	void Transform::SetWorldPosition(const Vec3& position)
	{
		auto SetPos = position;
		auto parPtr = GetParent();
		if (parPtr)
		{
			auto parWorldPos = parPtr->GetComponent<Transform>()->GetWorldMatrix().transInMatrix();
			SetPos -= parWorldPos;
			auto parQt = parPtr->GetComponent<Transform>()->GetWorldMatrix().quatInMatrix();
			parQt.inverse();
			Mat4x4 parQtMat(parQt);
			SetPos *= parQtMat;
		}
		SetPosition(SetPos);
	}
	void Transform::ResetWorldPosition(const Vec3& position)
	{
		auto setPos = position;
		auto parPtr = GetParent();
		if (parPtr)
		{
			auto parWorldPos = parPtr->GetComponent<Transform>()->GetWorldMatrix().transInMatrix();
			setPos -= parWorldPos;
			auto parQt = parPtr->GetComponent<Transform>()->GetWorldMatrix().quatInMatrix();
			parQt.inverse();
			Mat4x4 parQtMat(parQt);
			setPos *= parQtMat;
		}
		ResetPosition(setPos);
	}

	bool Transform::IsSameWorldMatrix(const Mat4x4& mat)
	{
		return mat.equalInt(GetWorldMatrix());
	}



	const Mat4x4& Transform::GetWorldMatrix()
	{
		auto parPtr = GetParent();
		//Mat4x4 WorldMat;
		m_worldMatrix.affineTransformation(
			m_param.scale,
			m_param.rotateOrigin,
			m_param.quaternion,
			m_param.position
		);
		if (parPtr)
		{
			auto parWorld = parPtr->GetComponent<Transform>()->GetWorldMatrix();
			parWorld.scaleIdentity();
			m_worldMatrix *= parWorld;
		}
		return m_worldMatrix;
	}


	const Mat4x4& Transform::Get2DWorldMatrix()
	{
		auto parPtr = GetParent();
		m_param.scale.z = 1.0f;
		Vec4 temp_z(m_param.position.z);
		temp_z = XMVector4ClampLength(temp_z, 0.0f, 1.0f);
		m_param.position.z = temp_z.z;
		m_param.rotateOrigin.z = 0;
		//Mat4x4 WorldMat;
		m_worldMatrix.affineTransformation(
			m_param.scale,
			m_param.rotateOrigin,
			m_param.quaternion,
			m_param.position
		);
		if (parPtr)
		{
			auto parWorld = parPtr->GetComponent<Transform>()->Get2DWorldMatrix();
			parWorld.scaleIdentity();
			m_worldMatrix *= parWorld;
		}
		return m_worldMatrix;
	}




	std::shared_ptr<GameObject> Transform::GetParent()const
	{
		auto shPtr = m_parent.lock();
		if (shPtr)
		{
			return shPtr;
		}
		return nullptr;
	}
	void Transform::SetParent(const std::shared_ptr<GameObject>& Obj)
	{
		if (GetParent() == Obj)
		{
			return;
		}
		if (Obj)
		{
			ClearParent();
			m_parent = Obj;
			auto parWorld = Obj->GetComponent<Transform>()->GetWorldMatrix();
			parWorld.scaleIdentity();
			auto posSpan = GetPosition() - parWorld.transInMatrix();
			auto qtSpan = parWorld.quatInMatrix();
			qtSpan.inverse();
			Mat4x4 qarQtMat(qtSpan);
			posSpan *= qarQtMat;

			Mat4x4 Mat = GetWorldMatrix() * parWorld;
			Vec3 scale, pos;
			Quat qt;
			Mat.decompose(scale, qt, pos);
			SetScale(scale);
			SetQuaternion(qt);
			SetPosition(posSpan);
			SetToBefore();

		}
		else
		{
			//nullptrが渡された
			ClearParent();
		}
	}

	void Transform::ClearParent()
	{
		if (auto parPtr = GetParent())
		{
			auto pos = GetWorldPosition();
			SetPosition(pos);
			SetToBefore();
		}
		m_parent.reset();
	}

	/// <summary>
	/// 現在の速度ベクトルを取得
	/// </summary>
	/// <returns>前回のフレームからの位置変化に基づいて計算された速度ベクトル</returns>
	Vec3 Transform::GetVelocity() const
	{
		// 1. 前回のフレームからの経過時間を取得
		auto ElapsedTime = (float)Scene::GetElapsedTime();
		
		// 2. 位置の変化量を計算
		Vec3 Velocity = m_param.position - m_beforeParam.position;
		
		// 3. 時間で割って速度に変換
		Velocity /= ElapsedTime;
		
		return Velocity;
	}


	Vec3 Transform::GetMovePositiom() const
	{
		return m_param.position - m_beforeParam.position;
	}

	float Transform::GetMoveSize() const
	{
		Vec3 Move = m_param.position - m_beforeParam.position;
		return Move.length();
	}

	void Transform::SetToBefore()
	{
		m_beforeParam = m_param;
	}

	Vec3 Transform::GetForward()
	{
		Vec3 ret = GetWorldMatrix().rotZInMatrix();
		ret.normalize();
		return ret;
	}

	Vec3 Transform::GetUp()
	{
		Vec3 ret = GetWorldMatrix().rotYInMatrix();
		ret.normalize();
		return ret;
	}
	Vec3 Transform::GetRight()
	{
		Vec3 ret = GetWorldMatrix().rotXInMatrix();
		ret.normalize();
		return ret;
	}
}