#pragma once
#include "stdafx.h"

namespace shooting {

	class Player : public GameObject
	{
	private:
		//プレイヤーが使用するコントローラとキーボードの入力
		Vec2 GetInputState() const;
		Vec2 GetInputKey() const;
		// 方向ベクトルを得る
		Vec3 GetMoveVector() const;
		//プレイヤーの移動
		void MovePlayer();
		//入力ハンドラー
		InputHandler<Player> m_InputHandler;
		//スピード
		float m_Speed;
		// 地面にいるかどうか
		bool m_IsGround;
		// 弾発射間隔
		double m_ShotCool = 0.0;

		// 地面衝突判定の共通処理
		void CheckGroundCollision(const CollisionPair& pair);
	public:
		Player(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~Player() {}
		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);
		//衝突開始時処理
		virtual void OnCollisionEnter(const CollisionPair& pair)override;
		//衝突継続時処理
		virtual void OnCollisionExecute(const CollisionPair& pair)override;
		//Aボタン
		void OnPushA();
		//Bボタン
		void OnPushB() {}
	};

}