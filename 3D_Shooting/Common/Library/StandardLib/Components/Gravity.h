#pragma once
#include "stdafx.h"

namespace shooting {

	class Gravity : public Component
	{
	private:
		Vec3 m_Gravity;
		Vec3 m_GravityVelocity;
	public:
		explicit Gravity(const std::shared_ptr<GameObject>& gameObjectPtr,
						 const Vec3& gravity = Vec3(0, -9.8f, 0));
		virtual ~Gravity();
		Vec3 GetGravity() const;
		void SetGravity(const Vec3& gravity);
		void SetGravityZero();
		Vec3 GetGravityVelocity() const;
		void SetGravityVelocity(const Vec3& gravityVelocity);
		void SetGravityVelocityZero();
		void StartJump(const Vec3& jumpVelocity);
		virtual void OnUpdate(double elapsedTime);

		virtual void OnCreate()override {}
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnDestroy()override {}
	};
}