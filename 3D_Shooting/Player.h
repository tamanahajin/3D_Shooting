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
		void MovePlayer(float elapsedTime);
		void BeginSpawnIntro();
		bool UpdateSpawnIntro(double elapsedTime);
		void UpdateSpawnIntroCamera(const Vec3& playerPosition);
		void SetSpawnIntroCharacterVisible(bool visible);
		//入力ハンドラー
		InputHandler<Player> m_InputHandler;
		//スピード
		float m_Speed;
		// 地面にいるかどうか
		bool m_IsGround;
		// 弾発射間隔
		double m_ShotCool = 0.0;
		std::shared_ptr<MainCamera> m_MainCamera;
		// CollisionManager側も空間分割ノードからPlayerを保持するため、弱参照にして循環所有を防ぐ。
		std::weak_ptr<CollisionManager> m_CollisionManager;
		std::shared_ptr<BulletManager> m_BulletManager;

		// 地面衝突判定の共通処理
		void CheckGroundCollision(const CollisionPair& pair);
		void CheckItemPickup(const CollisionPair& pair);
		void ResolveSlopeCollision(double elapsedTime);
		// 現在の弾タイプ
		BulletType m_CurrentBullet = BulletType::Default;
		int m_BombAmmo = 0;

		std::shared_ptr<BombAimPreview> m_BombPreview;

		bool m_IsDead = false;
		bool m_DeathAnimFinished = false;
		bool m_SpawnIntroActive = false;
		bool m_SpawnIntroCharacterVisible = true;
		bool m_SpawnIntroSePlayed = false;
		double m_SpawnIntroTimer = 0.0;
		Vec3 m_SpawnIntroStartPosition = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_SpawnIntroEndPosition = Vec3(0.0f, 0.0f, 0.0f);
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

		void AddBombAmmo(int amount);
		int GetBombAmmo() const { return m_BombAmmo; }
		bool IsBombMode() const { return m_CurrentBullet == BulletType::Bomb && m_BombAmmo > 0; }

		bool IsDead() const { return m_IsDead; }
		bool IsDeathAnimationFinished() const { return m_DeathAnimFinished; }
		bool IsSpawnIntroActive() const { return m_SpawnIntroActive; }
		bool IsSpawnIntroCharacterVisible() const { return m_SpawnIntroCharacterVisible; }
	};

}

