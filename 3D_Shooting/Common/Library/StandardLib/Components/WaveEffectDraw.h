#pragma once
#include "stdafx.h"

namespace shooting {

	struct WaveEffectDrawConstant
	{
		Mat4x4 worldViewProjection;
		Col4 color;
		Vec4 waveTimeAmplitudeFrequencySpeed;
		Vec4 waveDirectionEdgeStartEnd;
		Vec4 waveShakeAxis;
	};

	class WaveEffectDraw : public Component
	{
	private:
		WaveEffectDrawConstant m_ConstantBuffer = {};
		size_t m_ConstantBufferIndex = 0;
		Col4 m_Color = Col4(1.0f);
		float m_WaveTime = 0.0f;
		float m_WaveAmplitude = 0.355f;
		float m_WaveFrequency = 14.0f;
		float m_WaveSpeed = 5.8f;
		Vec2 m_WaveDirection = Vec2(1.0f, 0.45f);
		float m_WaveEdgeStart = 0.12f;
		float m_WaveEdgeEnd = 1.0f;
		Vec3 m_WaveShakeAxis = Vec3(0.0f, 1.0f, 0.0f);

	public:
		explicit WaveEffectDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~WaveEffectDraw() {}

		void SetColor(const Col4& color);
		void SetWaveTime(float time);
		void SetWave(float amplitude, float frequency, float speed);
		void SetWaveDirection(const Vec2& direction);
		void SetEdgeMask(float edgeStart, float edgeEnd);
		void SetShakeAxis(const Vec3& axis);

		virtual void OnCreate() override;
		virtual void OnUpdateConstantBuffers() override;
		virtual void OnCommitConstantBuffers() override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList) override;
	};
}
