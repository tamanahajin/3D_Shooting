/*!
@file EnemyStatus.h
@brief 敵1体分の調整値をまとめる
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	/*!
	@brief HP、移動、当たり判定、被弾演出など敵種別ごとに変えたい値

	EnemyController はこの設定を生成時に EnemyState へコピーして使う。
	*/
	struct EnemyStatus
	{
		int maxHp = 3;
		float moveSpeed = 5.0f;
		Vec3 modelScale = Vec3(0.01f, 0.01f, 0.01f);
		float collisionRadius = 0.2f;
		float collisionHeight = 0.3f;
		float groundFootOffset = 0.35f;
		double steeringInterval = 0.05;
		double damageFlashDuration = 0.2;
		float damageNumberOffsetY = 0.35f;
		double hitPushDuration = 0.30;
		float hitPushDistance = 0.16f;
		float hitPushLeanAngle = 0.36f;
	};

}
