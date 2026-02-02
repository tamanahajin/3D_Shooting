/*!
@file Component.cpp
@brief コンポーネント親　実体
*/

#include "stdafx.h"

namespace shooting {

	Component::Component(const std::shared_ptr<GameObject>& gameObjectPtr) :
		m_gameObject(gameObjectPtr),
		m_updateActive(true),
		m_drawActive(false)
	{
	}


}