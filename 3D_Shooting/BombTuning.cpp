#include "stdafx.h"
#include "BombTuning.h"

namespace shooting {

	const BombTuning& GetBombTuning()
	{
		static const BombTuning k{};
		return k;
	}

}
