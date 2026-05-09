#pragma once
#include "stdafx.h"
#include "Project.h"

namespace shooting {

	class Player;

	class PlayerWeapon : public GameObject
	{
	private:
		std::weak_ptr<Player> m_Player;
		bool m_HasStableTransform = false;
		bool m_StableTransformIsIdle = false;
		Vec3 m_StablePosition = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_StableScale = Vec3(1.0f, 1.0f, 1.0f);
		Quat m_StableRotation = Quat();

		bool TryUpdateFromPlayerHand();

	public:
		PlayerWeapon(const std::shared_ptr<Stage>& stagePtr, const std::shared_ptr<Player>& player);
		virtual ~PlayerWeapon() {}
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnUpdate2(double elapsedTime) override;
	};

	class Player : public GameObject
	{
	private:
		double m_totalTime;
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
		std::shared_ptr<MainCamera> m_MainCamera;
		std::shared_ptr<CollisionManager> m_CollisionManager;
		std::shared_ptr<BulletManager> m_BulletManager;

		// 地面衝突判定の共通処理
		void CheckGroundCollision(const CollisionPair& pair);
		void CheckItemPickup(const CollisionPair& pair);
		void ResolveSlopeCollision();
		// 現在の弾タイプ
		BulletType m_CurrentBullet = BulletType::Default;

		std::shared_ptr<BombAimPreview> m_BombPreview;

		bool m_IsDead = false;
		bool m_DeathAnimFinished = false;
	public:
		Player(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~Player() {}
		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);
		virtual void OnUpdate2(double elapsedTime) override;
		//衝突開始時処理
		virtual void OnCollisionEnter(const CollisionPair& pair)override;
		//衝突継続時処理
		virtual void OnCollisionExecute(const CollisionPair& pair)override;
		//Aボタン
		void OnPushA();
		//Bボタン
		void OnPushB() {}

		bool IsDead() const { return m_IsDead; }
		bool IsDeathAnimationFinished() const { return m_DeathAnimFinished; }
	};

}

