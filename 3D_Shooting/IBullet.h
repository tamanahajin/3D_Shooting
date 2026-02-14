#pragma once
#include "stdafx.h"

namespace shooting {
	/// <summary>
	/// 弾丸オブジェクトの親クラス
	/// </summary>
	class IBullet : public GameObject
	{
	public:
		IBullet(const std::shared_ptr<Stage>& stage) :
			GameObject(stage)
		{
		}
		virtual ~IBullet() {}
	};
}