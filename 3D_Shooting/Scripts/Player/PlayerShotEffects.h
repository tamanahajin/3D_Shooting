#pragma once
#include "stdafx.h"

namespace shooting {

	class Player;
	struct RaycastHit;

	/*!
	@brief 通常射撃の銃口発光を生成する
	@param player 発射したプレイヤー
	@param fallbackPosition 銃口ソケットを取得できない場合の位置
	@param fallbackForward 銃口ソケットを取得できない場合の前方向
	*/
	void SpawnPlayerMuzzleFlash(
		const std::shared_ptr<Player>& player,
		const Vec3& fallbackPosition,
		const Vec3& fallbackForward);

	/*!
	@brief 判定を持たない表示専用の弾道を生成する
	@param stage エフェクトを追加するステージ
	@param start 弾道の開始位置
	@param end 弾道の終了位置
	@param fallbackForward 開始位置と終了位置が一致した場合の予備方向
	*/
	void SpawnPlayerBulletTracer(
		const std::shared_ptr<Stage>& stage,
		const Vec3& start,
		const Vec3& end,
		const Vec3& fallbackForward);

	/*!
	@brief Raycastの着弾位置へ表示専用の火花を生成する
	@param stage エフェクトを追加するステージ
	@param shotStart 射撃開始位置
	@param hit Raycastの命中情報
	@param shotForward 射撃方向
	*/
	void SpawnPlayerBulletImpactSpark(
		const std::shared_ptr<Stage>& stage,
		const Vec3& shotStart,
		const RaycastHit& hit,
		const Vec3& shotForward);

}
