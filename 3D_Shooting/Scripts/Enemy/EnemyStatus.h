/*!
@file EnemyStatus.h
@brief 敵1体分の調整値
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	/*!
	@brief 敵1体分の調整値

	HP、移動速度、当たり判定、被弾演出など、敵種別ごとに変えたい値をまとめる。
	EnemyBatchController はこの設定値を敵の実行時状態へコピーして保持する。
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
