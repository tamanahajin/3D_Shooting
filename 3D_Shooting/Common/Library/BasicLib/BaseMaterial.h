#pragma once
#include "stdafx.h"

namespace shooting {

	class BaseTexture;

	class BaseMaterial
	{
	public:
		void SetBaseColorTexture(const std::shared_ptr<BaseTexture>& texture)
		{
			m_BaseColorTexture = texture;
		}

		std::shared_ptr<BaseTexture> GetBaseColorTexture() const
		{
			return m_BaseColorTexture;
		}

		void SetBaseColor(const Col4& color)
		{
			m_BaseColor = color;
		}

		const Col4& GetBaseColor() const
		{
			return m_BaseColor;
		}

	private:
		std::shared_ptr<BaseTexture> m_BaseColorTexture;
		Col4 m_BaseColor = Col4(1.0f, 1.0f, 1.0f, 1.0f);
	};
}
