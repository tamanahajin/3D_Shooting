#pragma once
#include "stdafx.h"

namespace shooting {

	class CollisionManager;
	class MainCamera;
	class Player;

	/*!
	@brief 通常射撃の照準解決結果
	*/
	struct PlayerNormalShotAim
	{
		Vec3 muzzle;
		Vec3 aimPoint;
		RaycastHit hit;
		bool hasHit = false;
		bool isValid = false;
	};

	/*!
	@brief 爆弾投擲とプレビューで共有する照準解決結果
	*/
	struct PlayerBombAim
	{
		Vec3 start;
		Vec3 aimPoint;
		Vec3 hitNormal = Vec3(0.0f, 1.0f, 0.0f);
		Quat rotation;
		bool hasHit = false;
		bool isValid = false;
	};

	/*!
	@brief カメラ照準、物理コリジョン、CSV生成地形をまとめて解決するクラス

	Player本体からRaycastの詳細を分離し、射撃処理は結果だけを利用できるようにする。
	*/
	class PlayerAimResolver final
	{
	public:
		PlayerAimResolver() = delete;

		/*!
		@brief 通常射撃の銃口位置、着弾点、命中情報を解決する
		@param player 射撃するプレイヤー
		@param camera 照準に使用するカメラ
		@param collisionManager Raycastを行うコリジョン管理
		@param maxRange 最大射程
		@return 通常射撃の照準結果
		*/
		static PlayerNormalShotAim ResolveNormalShot(
			const std::shared_ptr<Player>& player,
			const std::shared_ptr<MainCamera>& camera,
			const std::shared_ptr<CollisionManager>& collisionManager,
			float maxRange);

		/*!
		@brief 爆弾プレビューと実弾で共有する着弾候補を解決する
		@param player 投擲するプレイヤー
		@param camera 照準に使用するカメラ
		@param collisionManager Raycastを行うコリジョン管理
		@param maxRange 最大照準距離
		@return 爆弾の照準結果
		*/
		static PlayerBombAim ResolveBombAim(
			const std::shared_ptr<Player>& player,
			const std::shared_ptr<MainCamera>& camera,
			const std::shared_ptr<CollisionManager>& collisionManager,
			float maxRange);
	};

}
