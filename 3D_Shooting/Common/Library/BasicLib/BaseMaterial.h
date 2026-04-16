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

	private:
		std::shared_ptr<BaseTexture> m_BaseColorTexture;
	};
}