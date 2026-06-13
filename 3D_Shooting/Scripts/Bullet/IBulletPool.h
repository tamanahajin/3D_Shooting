#pragma once
#include "stdafx.h"

namespace shooting {

	class IBulletPool : public GameObject
	{
	public:
		explicit IBulletPool(const std::shared_ptr<Stage>& stagePtr)
			: GameObject(stagePtr)
		{
		}
		virtual ~IBulletPool() = default;
		virtual void OnUpdate(double elapsedTime) override = 0;
		virtual void AllClear() = 0;
	};
}