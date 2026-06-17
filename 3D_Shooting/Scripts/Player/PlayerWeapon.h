#pragma once
#include "stdafx.h"

namespace shooting {

	class Player;

	/*!
	@brief プレイヤー武器の銃口ソケットをワールド空間へ変換した結果
	*/
	struct PlayerWeaponMuzzleTransform
	{
		Vec3 position;
		Vec3 right;
		Vec3 forward;
		Vec3 up;
	};

	/*!
	@brief プレイヤーが装備している武器の銃口位置と向きを取得する
	@param player 対象プレイヤー
	@param outTransform 取得したワールド変換
	@return 取得できた場合はtrue

	モデル側の銃口ソケットが見つからない場合は、武器ローカル座標の予備位置を使用する。
	*/
	bool TryGetPlayerWeaponMuzzleTransform(
		const std::shared_ptr<Player>& player,
		PlayerWeaponMuzzleTransform& outTransform);

	/*!
	@brief プレイヤーの右手ソケットへ追従する武器表示オブジェクト
	*/
	class PlayerWeapon : public GameObject
	{
	private:
		std::weak_ptr<Player> m_player;
		bool m_hasStableTransform = false;
		bool m_stableTransformIsIdle = false;
		Vec3 m_stablePosition = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_stableScale = Vec3(1.0f, 1.0f, 1.0f);
		Quat m_stableRotation = Quat();

		bool TryUpdateFromPlayerHand();

	public:
		PlayerWeapon(const std::shared_ptr<Stage>& stagePtr, const std::shared_ptr<Player>& player);
		virtual ~PlayerWeapon() = default;
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnUpdate2(double elapsedTime) override;
	};

}
