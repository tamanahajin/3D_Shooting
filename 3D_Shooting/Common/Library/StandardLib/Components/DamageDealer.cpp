#include "stdafx.h"
#include "Project.h"

namespace shooting {

	DamageDealer::DamageDealer(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr)
	{
	}

	DamageDealer::~DamageDealer() {}
}