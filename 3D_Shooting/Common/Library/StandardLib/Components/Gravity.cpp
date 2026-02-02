#include "stdafx.h"

namespace shooting {

	Gravity::Gravity(const std::shared_ptr<GameObject>& gameObjectPtr,
					 const Vec3& gravity) :
		Component(gameObjectPtr),
		m_Gravity(0),
		m_GravityVelocity(0)
	{
		m_Gravity = gravity;
	}

	Gravity::~Gravity() {}

	Vec3 Gravity::GetGravity() const
	{
		return m_Gravity;
	}
	void Gravity::SetGravity(const Vec3& gravity)
	{
		m_Gravity = gravity;
	}
	void Gravity::SetGravityZero()
	{
		m_Gravity = Vec3(0);
	}
	Vec3 Gravity::GetGravityVelocity() const
	{
		return m_GravityVelocity;
	}
	void Gravity::SetGravityVelocity(const Vec3& gravityVelocity)
	{
		m_GravityVelocity = gravityVelocity;
	}
	void Gravity::SetGravityVelocityZero()
	{
		m_GravityVelocity = Vec3(0);
	}

	void Gravity::StartJump(const Vec3& JumpVerocity)
	{
		m_GravityVelocity = JumpVerocity;
	}

	void Gravity::OnUpdate(double elapsedTime)
	{
		//コリジョンがあって、スリープ状態なら更新しない
		//auto PtrCollision = GetGameObject()->GetComponent<Collision>(false);
		//if (PtrCollision && PtrCollision->IsSleep()) {
		//	return;
		//}
		auto PtrTransform = GetGameObject()->GetComponent<Transform>();
		//前回のフレームからの時間（フレームレート非依存にするため）
		float ElapsedTime = (float)Scene::GetElapsedTime();
		m_GravityVelocity += m_Gravity * ElapsedTime;
		auto Pos = PtrTransform->GetPosition();
		//		auto Pos = PtrTransform->GetWorldPosition();
		Pos += m_GravityVelocity * ElapsedTime;
		PtrTransform->SetPosition(Pos);
	}
}