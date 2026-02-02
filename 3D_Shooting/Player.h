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
	public:
		Player(const std::shared_ptr<Stage>& StagePtr, const TransParam& param);
		virtual ~Player() {}
		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);
		//Aボタン
		void OnPushA();
		//Bボタン
		void OnPushB() {}
	};

}