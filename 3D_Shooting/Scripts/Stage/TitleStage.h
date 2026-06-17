#pragma once
#include "stdafx.h"

namespace shooting {

	class TitleStage : public GameStage
	{
	private:
		double m_time = 0.0;

		void CreateTitleActors();

	public:
		explicit TitleStage(ID3D12Device* pDevice) :
			GameStage(pDevice)
		{
		}

		virtual ~TitleStage() {}

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
		virtual void OnUpdate2(double elapsedTime) override;
		virtual void UpdateCollision() override;
	};

	class TitleStaticModel : public GameObject
	{
	private:
		std::wstring m_modelKey;
		std::wstring m_materialPrefix;
		Col4 m_fallbackColor = Col4(1.0f);
		float m_rotationSpeed = 0.0f;

	public:
		TitleStaticModel(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const std::wstring& modelKey,
			const std::wstring& materialPrefix,
			const Col4& fallbackColor,
			float rotationSpeed = 0.0f);

		virtual ~TitleStaticModel() {}
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
	};

	class TitleSkinnedModel : public GameObject
	{
	private:
		std::wstring m_meshKey;
		std::wstring m_textureKey;
		Vec3 m_modelOffset;
		AnimState m_animState = AnimState::Idle;
		float m_rotationSpeed = 0.0f;

	public:
		TitleSkinnedModel(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const std::wstring& meshKey,
			const std::wstring& textureKey,
			const Vec3& modelOffset,
			AnimState animState,
			float rotationSpeed = 0.0f);

		virtual ~TitleSkinnedModel() {}
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
	};

}
